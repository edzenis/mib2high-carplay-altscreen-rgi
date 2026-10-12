# Building AltScreen v1.0 from source

The exact distributed installer ZIP is **unchanged v8S**. Its payload binary SHA-256 values are recorded in `release_evidence/V8S_PACKAGE_MANIFEST.json` and `ARTIFACT_SHA256SUMS.txt`.

**Native receiver (`.so`):** The archived `build.sh` compiles from `native/` with QNX SDP 6.5 ARMv7 GCC 4.9.4, using a configured `QNX_RUNTIME/activate.sh`. Run `QNX_RUNTIME=/path/to/qnx-runtime ./build.sh`. This script creates `generated/` and `output/` during the build, and does not modify any prebuilt file.

**Java:** Editable sources are under `java/build/src/`. Java 8 and the firmware-specific base JAR are necessary. The archived `java/build_java_v8m.sh` additionally expects an older reference JAR for an exact class-level parity comparison; that historic reference binary is **not included** in this clean public source package. Set `JAVA_HOME` and `K2161_BASE_JAR`, and provide the referenced comparison JAR from your own properly licensed archive if you want to run this legacy verification/build script unchanged. Do not claim byte-for-byte output reproducibility unless the reference toolchain and JAR match.

**Renderer:** The original renderer source and its provenance are documented in the GitHub history/upstream Joey repository; this package ships its exact compiled v8S renderer only inside the installer asset. The source archive contains the corresponding v8S receiver, controller, Java, deployment, configuration and diagnostic sources but **does not contain enough source to rebuild the bundled `vc_render` executable on its own**. Do not claim otherwise. Retrieve the matching renderer source and validate it before making renderer changes.

**SD installer:** Deployment source is `deployment/custom.sh`, `deployment/mib2high-lvds-control.sh`, config is in `config/`. The exact installer should be treated as immutable; rebuilding a new version requires a new release candidate and fresh car tests.

**Host tests:** See `diagnostics/test_v8s_release.py`, `tests/test_v8s_control_mock.c`, and `release_evidence/V8S_*`.

**Source-only policy:** NEVER binary-patch already compiled `.so`, ELF, `.o`, Java `.class` or `.jar` as a substitute for a source edit/rebuild.
