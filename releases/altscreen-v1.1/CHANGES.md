# Changes from AltScreen v1.0 to v1.1

This document compares the public v1.0 (car-tested older native/Java implementation) with v1.1 (modular source-derived release).

## Added / improved

- Firmware-specific K2161 input and output connectors with a firmware-independent video/navigation core.
- Context 76 map-only Virtual Cockpit selection, with actual-context feedback, throttled reacquisition and pre-session context restoration.
- RGI input processing, HUD output queueing, bounded state handling, and more detailed diagnostics.
- Trip-summary text presentation through the existing factory BAP interface.
- Teardown logic that isolates the private supplementary CarPlay stream from stock-stream teardown (addresses a plausible cause of a reported A4 B9 `dio_manager` crash; **not confirmed fixed on that car**).
- A source-buildable native library and Java bundle, plus guarded rollback and log collection.

## Changed / limitations

- Factory VC composition context changes **77 → 76**. This excludes factory maneuver planes from the selected composition but may have firmware-specific effects.
- All four advertised video safe areas are now **500, 72, 440, 330** instead of v1.0's differentiated small/big-gauge areas.
- The current `showUI` map URL uses **topAligned** for both gauge layouts; v1.0 supported leftAligned for small gauges. Not an equivalent presentation behavior.
- The dedicated CarPlay instruction-card URL is still advertised; the separate alignment-specific URLs from v1.0 are not advertised.
- Original H.264 `vc_render` map-decoder executable is reused unchanged. No custom arrow/3D maneuver renderer is included.
- **NavActiveIgnore is NOT implemented, bundled, or automatically installed in v1.1.** It must be installed separately on vehicles requiring it; existing settings remain untouched.
- v1.1 has offline build/installer checks, **not** new vehicle validation; v1.0's vehicle testing does not establish v1.1 correctness.

## Not implemented

A custom factory VC maneuver overlay, automated NavActiveIgnore setup, telephone call-card enhancements or related steering-wheel call control are **not** features of this release.
