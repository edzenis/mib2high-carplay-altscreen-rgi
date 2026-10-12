# Renderer source provenance and build status

**Upstream source:** [joeyQuery/MHI2-altScreen](https://github.com/joeyQuery/MHI2-altScreen) commit `59b7fa6d1b81e0da266cf4f70b4cf54c34bc03dc`, file `src/vc_render/vc_render.c`, Git blob SHA `0191a785eb2040eec1cb3926b5c0a2454fa34242`.

**License:** upstream GPL-3.0 (full text `../LICENSE-GPL-3.0`). This repository now includes the upstream editable C source and rebuild instructions, rather than linking to it as the sole source record.

**Critical qualification:** the AltScreen v1.0 archive contains the *unchanged vehicle-tested v8S* compiled renderer, SHA-256 `49b2d5c38899dd4aab02015962b5df2cb39162931b1e973b17216ce076501186`. Exact bit-for-bit correspondence between the public upstream C file here and that historical compiled executable has **not** been established. No binary editing is permitted. Do **not** assert this source is an exact match, nor claim fully reproducible release binaries before the missing historical adaptations are recovered and rebuilt.

The renderer accepts `--out 1 --layer 58 --w 1440 --h 542 --fps 30 --ts wall`. These parameters are supplied at runtime by the car-tested `deployment/mib2high-lvds-control.sh`. They do not have to be the C default constants.

**Future changes:** edit source and build a new executable with your licensed QNX SDK; test independently. Never substitute a newly built but unverified renderer into the immutable v8S installer.
