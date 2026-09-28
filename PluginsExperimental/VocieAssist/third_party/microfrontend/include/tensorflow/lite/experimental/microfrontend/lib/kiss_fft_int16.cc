#include <cstdint>

#include "tensorflow/lite/experimental/microfrontend/lib/kiss_fft_common.h"

#define FIXED_POINT 16
namespace kissfft_fixed16 {
#include "kiss_fft.cc"
#include "tools/kiss_fftr.cc"
}  // namespace kissfft_fixed16
#undef FIXED_POINT
