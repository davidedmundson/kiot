# Vendored microfrontend + kissfft

These C/C++ sources are vendored verbatim from
[OHF-Voice/pymicro-features](https://github.com/OHF-Voice/pymicro-features)
v2.0.2, which itself bundles:

- TensorFlow Lite Micro's `microfrontend` (under
  `tensorflow/lite/experimental/microfrontend/lib/`) — the audio
  feature extractor that turns 16 kHz mono PCM into 40-dim INT8
  features for microWakeWord models.
- kissfft (only the int16 fixed-point path), used by the
  microfrontend.

We use them through `audio/MicroFeatures.{h,cpp}`, which exposes a
small C++ wrapper around the `Frontend*` C entry points.

## Layout

```
third_party/microfrontend/
├── LICENSE
└── include/
    ├── tensorflow/lite/experimental/microfrontend/lib/    (.h + .cc)
    └── kissfft/                                           (.h + .cc)
```

Both `.h` and `.cc` live under `include/` so a single
`-Ithird_party/microfrontend/include` makes the in-source `#include`
paths (e.g. `"tensorflow/lite/experimental/microfrontend/lib/frontend.h"`,
`"kiss_fft.h"`) resolve.

## Build flags

`-DFIXED_POINT=16` is required (kissfft wants it; the microfrontend
hard-codes 16-bit fixed-point arithmetic). Set in CMakeLists.txt.

## What's *not* here

We deliberately drop the upstream files matching
`*_io.{h,cc}`, `*_test.cc`, `frontend_main.cc`,
`frontend_memmap_*.cc`. They are diagnostic / test utilities not
needed at runtime.

## Why vendor instead of using the python package's binary

`libmicro_features_cpp.so` from the Python package is a CPython
extension (links against libpython, only callable through Py_Capsule).
The Python wrapper does almost no work — it just exposes the
`FrontendProcessSamples` loop. We get the same behavior in pure C++
by linking the same C sources into our binary, without the Python
dependency.

## Bit-exact parity

The wake-word pipeline must produce inference probabilities that
match the Python build to ~1e-4 per frame (see
`doc/linux-voice-assistant-cpp-refactor-plan.md` §6.2). Since we
compile the same .cc files with the same `-DFIXED_POINT=16`, the
feature extractor stage is identically bit-exact by construction.
The wrapper test in `test/verify_phase4a_features.py` compares
the C++ output against the Python wrapper's output for the same
WAV file.

## License

Apache-2.0 (TensorFlow Lite Micro & pymicro-features).
BSD-3-Clause-like (kissfft).
Both compatible with our project's Apache-2.0.
