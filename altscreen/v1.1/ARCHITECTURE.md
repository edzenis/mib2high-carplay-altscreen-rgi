# AltScreen v1.1 architecture

K2161 native and Java input connectors receive firmware and CarPlay events. The firmware-independent native and Java core processes and normalizes state. K2161 output connectors send CarPlay display, factory HMI and HUD messages. Data flows only from input to core to output; explicitly documented input-to-output paths are used for raw protocol operations where necessary. Shared core code contains no K2161-specific API assumptions.

Native supplementary H.264 display and HUD RGI are separate paths. Context selection must not depend on active route guidance. Context 76 requests the CarPlay video map without a custom maneuver rendering displayable. The installer changes neither NavActiveIgnore nor firmware binary code.
