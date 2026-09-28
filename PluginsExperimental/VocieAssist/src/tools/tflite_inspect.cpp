
#include <cstdio>
#include <cstdlib>
#include <string>

#include "audio/TfliteRuntime.h"

namespace {

void PrintTensor(const char* tag, int idx,
                 const lva::audio::TfliteTensorInfo& info) {
    std::printf("%s %d type=%d shape=", tag, idx,
                static_cast<int>(info.type));
    for (std::size_t i = 0; i < info.shape.size(); ++i) {
        std::printf("%s%d", i == 0 ? "" : ",", info.shape[i]);
    }
    std::printf(" scale=%g zp=%d bytes=%zu\n",
                info.quant.scale, info.quant.zero_point,
                info.byte_size);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <path-to-model.tflite>\n", argv[0]);
        return 2;
    }

    lva::audio::TfliteRuntime rt;
    if (!rt.Ok()) {
        std::fprintf(stderr, "TFLite library load failed: %s\n",
                     rt.LastError().c_str());
        return 1;
    }

    if (!rt.LoadModel(argv[1])) {
        std::fprintf(stderr, "LoadModel failed: %s\n",
                     rt.LastError().c_str());
        return 1;
    }

    const int in_count  = rt.InputCount();
    const int out_count = rt.OutputCount();

    for (int i = 0; i < in_count; ++i) {
        PrintTensor("IN", i, rt.InputInfo(i));
    }
    for (int i = 0; i < out_count; ++i) {
        PrintTensor("OUT", i, rt.OutputInfo(i));
    }
    return 0;
}
