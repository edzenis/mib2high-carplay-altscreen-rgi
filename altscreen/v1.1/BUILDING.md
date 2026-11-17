# Build AltScreen v1.1

Requires QNX 6.5 ARMv7 GCC 4.9.4 (`qcc`) and Java 8, plus the matching K2161 firmware OEM JAR used **only** as a compile dependency. These proprietary toolchains/JARs are not distributed.

- `bash build_native.sh`: builds `output/libmib2high-carplay-rgi.so` and checks expected ABI exports and dependencies.
- `bash build_java.sh`: builds `output/mib2high-carplay-rgi.jar`, validates Java classfile version and class count.
- Helpers are shipped from the unchanged original video/diagnostic binaries. Their corresponding source is in `helpers/`; exact binary provenance is documented, and byte-for-byte helper reproduction is not claimed.

Never patch an existing QNX shared object or Java JAR in place. Changes belong in editable source, rebuilt, then checked. The published installer contains built binaries and a rollback path.
