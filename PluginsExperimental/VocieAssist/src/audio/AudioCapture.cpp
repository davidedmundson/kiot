#include "audio/AudioCapture.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include <alsa/asoundlib.h>
#include <pulse/error.h>
#include <pulse/sample.h>
#include <pulse/simple.h>

#include "audio/PcmRingBuffer.h"
#include "audio/WebRtcProcessor.h"
#include "util/Log.h"

namespace lva::audio {

namespace {

constexpr const char* kTag         = "capture";
constexpr unsigned    kSampleRate  = 16'000;
constexpr unsigned    kMonoChannels = 1;
constexpr const char* kClientName  = "linux-voice-assistant-cpp";
constexpr const char* kStreamName  = "wake-word capture";

}  // namespace

AudioCapture::AudioCapture(const Options& opts, PcmRingBuffer& ring)
    : opts_(opts), ring_(ring) {}

void AudioCapture::AddTap(PcmRingBuffer& ring) {
    if (running_.load(std::memory_order_relaxed)) {
        LVA_LOGW(kTag, "AddTap called after Start; ignoring");
        return;
    }
    taps_.push_back(&ring);
}

void AudioCapture::SetProcessor(WebRtcProcessor* processor) {
    if (running_.load(std::memory_order_relaxed)) {
        LVA_LOGW(kTag, "SetProcessor called after Start; ignoring");
        return;
    }
    processor_ = processor;
}

AudioCapture::~AudioCapture() {
    Stop();
}

bool AudioCapture::Start() {
    if (running_.load(std::memory_order_relaxed)) {
        LVA_LOGD(kTag, "Start: already running, ignoring");
        return true;
    }
    stop_requested_.store(false, std::memory_order_relaxed);
    samples_captured_.store(0, std::memory_order_relaxed);
    dropped_samples_  = 0;
    last_drop_log_at_ = 0;

    if (opts_.backend == Backend::kAlsa) {
        snd_pcm_t* pcm = nullptr;
        int rc = ::snd_pcm_open(&pcm, opts_.alsa_device.c_str(),
                                SND_PCM_STREAM_CAPTURE, 0);
        if (rc < 0) {
            LVA_LOGE(kTag, "snd_pcm_open(%s) failed: %s",
                     opts_.alsa_device.c_str(), ::snd_strerror(rc));
            return false;
        }

        snd_pcm_hw_params_t* hw = nullptr;
        snd_pcm_hw_params_alloca(&hw);

        if ((rc = ::snd_pcm_hw_params_any(pcm, hw)) < 0) {
            LVA_LOGE(kTag, "hw_params_any: %s", ::snd_strerror(rc));
            ::snd_pcm_close(pcm); return false;
        }
        if ((rc = ::snd_pcm_hw_params_set_access(
                 pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
            LVA_LOGE(kTag, "set_access: %s", ::snd_strerror(rc));
            ::snd_pcm_close(pcm); return false;
        }
        if ((rc = ::snd_pcm_hw_params_set_format(
                 pcm, hw, SND_PCM_FORMAT_S16_LE)) < 0) {
            LVA_LOGE(kTag, "set_format S16_LE: %s", ::snd_strerror(rc));
            ::snd_pcm_close(pcm); return false;
        }
        if ((rc = ::snd_pcm_hw_params_set_channels(
                 pcm, hw, opts_.alsa_channels)) < 0) {
            LVA_LOGE(kTag, "set_channels %u: %s",
                     opts_.alsa_channels, ::snd_strerror(rc));
            ::snd_pcm_close(pcm); return false;
        }
        unsigned rate = kSampleRate;
        if ((rc = ::snd_pcm_hw_params_set_rate_near(
                 pcm, hw, &rate, nullptr)) < 0) {
            LVA_LOGE(kTag, "set_rate_near %u: %s",
                     kSampleRate, ::snd_strerror(rc));
            ::snd_pcm_close(pcm); return false;
        }
        if (rate != kSampleRate) {
            LVA_LOGW(kTag, "alsa gave us rate %u, expected %u",
                     rate, kSampleRate);
        }

        snd_pcm_uframes_t period_frames = opts_.frames_per_read;
        if ((rc = ::snd_pcm_hw_params_set_period_size_near(
                 pcm, hw, &period_frames, nullptr)) < 0) {
            LVA_LOGE(kTag, "set_period_size_near: %s",
                     ::snd_strerror(rc));
            ::snd_pcm_close(pcm); return false;
        }
        snd_pcm_uframes_t buffer_frames = kSampleRate / 2;
        if ((rc = ::snd_pcm_hw_params_set_buffer_size_near(
                 pcm, hw, &buffer_frames)) < 0) {
            LVA_LOGE(kTag, "set_buffer_size_near: %s",
                     ::snd_strerror(rc));
            ::snd_pcm_close(pcm); return false;
        }

        if ((rc = ::snd_pcm_hw_params(pcm, hw)) < 0) {
            LVA_LOGE(kTag, "snd_pcm_hw_params commit: %s",
                     ::snd_strerror(rc));
            ::snd_pcm_close(pcm); return false;
        }

        alsa_handle_ = pcm;
        running_.store(true, std::memory_order_relaxed);
        thread_ = std::thread([this] { ThreadLoopAlsa(); });

        LVA_LOGI(kTag,
                 "started alsa (device=%s rate=%u ch=%u period=%lu "
                 "buffer=%lu mic_ch=%u ref=(%d,%d))",
                 opts_.alsa_device.c_str(), rate, opts_.alsa_channels,
                 static_cast<unsigned long>(period_frames),
                 static_cast<unsigned long>(buffer_frames),
                 opts_.mic_channel,
                 opts_.ref_channels[0], opts_.ref_channels[1]);
        return true;
    }

    // ----- PulseAudio capture (legacy / fallback) -----
    pa_sample_spec spec{};
    spec.format   = PA_SAMPLE_S16LE;
    spec.rate     = kSampleRate;
    spec.channels = static_cast<std::uint8_t>(kMonoChannels);

    pa_buffer_attr attr{};
    attr.maxlength = static_cast<std::uint32_t>(-1);
    attr.fragsize  = pa_usec_to_bytes(opts_.buffer_latency_us, &spec);

    int err = 0;
    pa_simple* pa = pa_simple_new(
        /*server=*/nullptr,
        kClientName,
        PA_STREAM_RECORD,
        opts_.source.empty() ? nullptr : opts_.source.c_str(),
        kStreamName,
        &spec,
        /*channel_map=*/nullptr,
        &attr,
        &err);
    if (pa == nullptr) {
        LVA_LOGE(kTag, "pa_simple_new failed: %s",
                 pa_strerror(err));
        return false;
    }

    pa_handle_ = pa;
    running_.store(true, std::memory_order_relaxed);
    thread_ = std::thread([this] { ThreadLoopPulse(); });

    LVA_LOGI(kTag,
             "started pulse (rate=%u ch=%u frames_per_read=%zu source=%s)",
             kSampleRate, kMonoChannels, opts_.frames_per_read,
             opts_.source.empty() ? "default" : opts_.source.c_str());
    return true;
}

void AudioCapture::Stop() {
    if (!running_.load(std::memory_order_relaxed)) return;

    // The capture thread owns handle teardown — closing the pa/alsa
    // handle here would race a blocking read on it (use-after-free).
    // Just signal and join; reads return within a period/buffer.
    stop_requested_.store(true, std::memory_order_relaxed);

    if (thread_.joinable()) {
        thread_.join();
    }
    running_.store(false, std::memory_order_relaxed);
    LVA_LOGI(kTag, "stopped (captured %llu samples)",
             static_cast<unsigned long long>(
                 samples_captured_.load(std::memory_order_relaxed)));
}

void AudioCapture::PostFrame(std::int16_t* mic, std::size_t n) {
    if (mic_volume_ptr_ != nullptr) {
        const int vol = mic_volume_ptr_->load(std::memory_order_relaxed);
        if (vol > 0 && vol != 100) {
            const float gain = static_cast<float>(vol) / 100.0f;
            for (std::size_t i = 0; i < n; ++i) {
                const long scaled =
                    std::lround(static_cast<float>(mic[i]) * gain);
                const long clamped =
                    std::clamp(scaled, -32768L, 32767L);
                mic[i] = static_cast<std::int16_t>(clamped);
            }
        }
    }

    const std::size_t written = ring_.Write(mic, n);
    if (written < n) {
        const std::size_t dropped = n - written;
        dropped_samples_ += dropped;
        if (dropped_samples_ - last_drop_log_at_ >= 160'000) {
            LVA_LOGW(kTag,
                     "ring overrun: %llu samples dropped since start",
                     static_cast<unsigned long long>(dropped_samples_));
            last_drop_log_at_ = dropped_samples_;
        }
    }
    for (PcmRingBuffer* tap : taps_) {
        tap->Write(mic, n);
    }
    samples_captured_.fetch_add(written, std::memory_order_relaxed);
}

void AudioCapture::ThreadLoopPulse() {
    std::vector<std::int16_t> chunk(opts_.frames_per_read);
    const std::size_t bytes_per_read =
        chunk.size() * sizeof(std::int16_t);

    pa_simple* pa = static_cast<pa_simple*>(pa_handle_);

    while (!stop_requested_.load(std::memory_order_relaxed)) {
        if (pa == nullptr) break;

        int err = 0;
        const int rc = pa_simple_read(pa, chunk.data(),
                                      bytes_per_read, &err);
        if (rc < 0) {
            if (!stop_requested_.load(std::memory_order_relaxed)) {
                LVA_LOGE(kTag, "pa_simple_read failed: %s",
                         pa_strerror(err));
            }
            break;
        }

        if (processor_ != nullptr) {
            processor_->Process(chunk.data(), chunk.size());
        }

        PostFrame(chunk.data(), chunk.size());
    }

    // The capture thread owns handle teardown (see Stop()).
    if (pa != nullptr) {
        pa_simple_free(pa);
        pa_handle_ = nullptr;
    }
    LVA_LOGD(kTag, "pulse thread exiting");
}

void AudioCapture::ThreadLoopAlsa() {
    const std::size_t period = opts_.frames_per_read;
    const unsigned    nch    = opts_.alsa_channels;
    std::vector<std::int16_t> il(period * nch);
    std::vector<std::int16_t> mic(period);
    std::vector<std::int16_t> ref(period);

    const int  ref_a = opts_.ref_channels[0];
    const int  ref_b = opts_.ref_channels[1];
    const bool have_ref     = (ref_a >= 0);
    const bool single_ref   = (ref_a >= 0 && ref_b < 0);
    const unsigned mic_ch   = opts_.mic_channel;

    snd_pcm_t* pcm = static_cast<snd_pcm_t*>(alsa_handle_);

    while (!stop_requested_.load(std::memory_order_relaxed)) {
        if (pcm == nullptr) break;

        snd_pcm_sframes_t r = ::snd_pcm_readi(pcm, il.data(), period);
        if (r < 0) {
            const int err = static_cast<int>(r);
            if (err == -EPIPE || err == -ESTRPIPE || err == -EINTR ||
                err == -EIO) {
                if (!stop_requested_.load(std::memory_order_relaxed)) {
                    LVA_LOGW(kTag, "alsa readi %s; recovering",
                             ::snd_strerror(err));
                }
                if (::snd_pcm_recover(pcm, err, /*silent=*/1) < 0) {
                    if (!stop_requested_.load(std::memory_order_relaxed)) {
                        LVA_LOGE(kTag, "snd_pcm_recover failed");
                    }
                    break;
                }
                // Recover dropped samples → mic/ref no longer aligned;
                // reset the AEC so it re-converges instead of leaking.
                if (processor_ != nullptr && have_ref) {
                    processor_->ResetEcho();
                }
                continue;
            }
            if (!stop_requested_.load(std::memory_order_relaxed)) {
                LVA_LOGE(kTag, "alsa readi: %s", ::snd_strerror(err));
            }
            break;
        }
        if (static_cast<std::size_t>(r) != period) {
            // Short read: skip the partial frame rather than zero-pad
            // it (padding would desync the AEC). Reset and retry.
            if (!stop_requested_.load(std::memory_order_relaxed)) {
                LVA_LOGW(kTag, "alsa short read: %ld/%zu frames; skipping",
                         static_cast<long>(r), period);
            }
            if (processor_ != nullptr && have_ref) {
                processor_->ResetEcho();
            }
            continue;
        }

        // De-interleave into mic + ref mono buffers.
        for (std::size_t i = 0; i < period; ++i) {
            const std::int16_t* row = il.data() + i * nch;
            mic[i] = row[mic_ch];
            if (have_ref) {
                if (single_ref) {
                    ref[i] = row[ref_a];
                } else {
                    const int32_t s =
                        static_cast<int32_t>(row[ref_a]) +
                        static_cast<int32_t>(row[ref_b]);
                    ref[i] = static_cast<std::int16_t>(s >> 1);
                }
            }
        }

        if (processor_ != nullptr) {
            if (have_ref) {
                processor_->ProcessReverse(ref.data(), period);
            }
            processor_->Process(mic.data(), period);
        }

        PostFrame(mic.data(), period);
    }

    // The capture thread owns handle teardown (see Stop()).
    if (pcm != nullptr) {
        ::snd_pcm_close(pcm);
        alsa_handle_ = nullptr;
    }
    LVA_LOGD(kTag, "alsa thread exiting");
}

}  // namespace lva::audio
