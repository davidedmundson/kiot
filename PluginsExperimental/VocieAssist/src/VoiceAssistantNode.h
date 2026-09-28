#pragma once

#include <string>
#include <memory>
#include <filesystem>
#include <atomic>

// Forhåndsdeklarasjoner for å unngå tunge inkluderingssider i headeren
namespace lva {
    namespace proto { class ApiServer; }
    namespace audio { class WakeWordEngine; class AudioCapture; class LibMpvPlayer; class PcmRingBuffer; class WebRtcProcessor; }
    namespace satellite { class Satellite; }
    namespace state { struct ServerState; }
    namespace tr { class Supervisor; class SupervisorHttpServer; class SoundConfWatcher; class MicMuteGpio; class HomeButton; }
}

struct VoiceAssistantConfig {
    std::string devicename = "3RSPK";
    std::string host = "0.0.0.0";
    std::uint16t port = 6053;
    std::filesystem::path preferencesfile = " /home/theoddpirate/config.json";
    std::string wakewordtype = "micro";  // "micro" eller "open"
    std::string wakewordmodels = "okaynabu";
    std::string audiodevice;
    std::string capturebackend = "alsa";
    std::string capturealsadevice = "hw:2,0";
    int capturemicchannel = 0;
    std::string capturerefchannels = "2,3";
    double continueconversationdelay = 0.5;
    bool debug = false;
};

class VoiceAssistantNode {
public:
    // Lar deg sende inn en valgfri forelder (f.eks QObject eller annen kontekst) og konfigurasjon
    explicit VoiceAssistantNode(VoiceAssistantConfig config = {});
    ~VoiceAssistantNode();

    bool Start();
    void Stop();
    bool IsRunning() const { return running; }

private:
    VoiceAssistantConfig config;
    std::atomic<bool> running{false};


    
    int satellitetickfd = -1;
};