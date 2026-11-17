# Binary helper provenance

The map-video decoder `vc_render` is inherited unchanged from the previously working installer, rather than recompiled for this branding pass. Its C source is in `helpers/vc_render.c` but the precise historic compiler is unavailable, so binary/source byte parity is **not** claimed. `vc_context_query` and the DSI tracer C sources are included in `helpers/`. No executable is patched.
