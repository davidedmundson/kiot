# libtensorflowlite_c.so

Prebuilt TensorFlow Lite C API shared object for ARM64 (aarch64),
**specifically built for the TRSPK target** (glibc ≤ 2.31).

Source: `python-pymicro-wakeword/lib/trspk/libtensorflowlite_c.so`
(the same binary pyopen-wakeword installed when the Python stack was
in use).

GLIBC requirement: max GLIBC_2.29 (verified via grep).

IMPORTANT: Do NOT replace this with the generic `linux_arm64` build
from upstream TensorFlow releases or from pymicro-wakeword's
`lib/linux_arm64/` directory — those require GLIBC_2.34 which is
newer than our toolchain (gcc-arm-10.2-2020.11, glibc 2.31).

Install path on target: `/usr/lib/libtensorflowlite_c.so` (via the
package `.mk` file's POST_INSTALL_TARGET_HOOKS).

License: Apache 2.0 (TensorFlow).
