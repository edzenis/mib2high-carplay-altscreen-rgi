# CarPlay navigation for Audi MIB2 High

This repository provides community-developed **CarPlay navigation for compatible instrument clusters and factory head-up displays**. It contains a stable HUD route-guidance bridge, a firmware research collector, and **AltScreen**, which puts the CarPlay map in the digital instrument cluster.

## Releases

| Release | Purpose | Status |
| --- | --- | --- |
| [RGI v1.1](https://github.com/edzenis/mib2high-carplay-altscreen-rgi/releases/tag/v1.1) | Turn arrows and maneuver distances in the factory HUD | Stable, car-tested |
| [MIB2 High Software Collector v1.0.2](https://github.com/edzenis/mib2high-carplay-altscreen-rgi/releases/tag/collector-v1.0.2) | Read-only firmware/CarPlay/cluster component research | Prerelease |
| [AltScreen v1.0](https://github.com/edzenis/mib2high-carplay-altscreen-rgi/releases/tag/altscreen-v1.0) | CarPlay map and instruction card in the instrument cluster, plus HUD RGI | Stable on tested platform |

## AltScreen v1.0 — what it does

AltScreen receives a second CarPlay video stream for the instrument cluster. It shows the map next to the gauges, adapts the instruction card to different layouts, and follows VIEW button layout changes. A **separate RGI path** delivers maneuver information to the **factory HUD** via the car's existing interface.

**Car-tested reference:** Audi Q7 4M, Harman MIB2 High K2161/MU1421, digital instrument cluster and HUD. Other cars, firmware versions and wireless adapters are not automatically supported. Native Virtual Cockpit graphics may still appear alongside the map.

**Read the full [AltScreen guide](altscreen/README.md)** for features, architecture, compatibility, installation and rollback.

## Source and development

The \`main\` branch holds public stable source; \`dev\` is for future changes. AltScreen v1.0 source lives in [\`altscreen/\`](altscreen/) and [build notes](altscreen/BUILDING.md) explain toolchains and prerequisites. Change editable source and rebuild; never patch compiled binaries or Java bytecode.

---

## Original RGI documentation
# MIB2 High CarPlay RGI

CarPlay turn-by-turn route guidance for Audi **MIB2 High / MHI2**, using Apple iAP2 Route Guidance Information (RGI) and the factory Audi BAP navigation interface to drive the **factory HUD**.

## Tested platform

- Audi Q7 4M
- MIB2 High / MHI2
- Firmware: `MHI2_ER_AUG22_K2161`
- MU software: `1421`

## v1.1 status

v1.1 is the current vehicle-tested release for K2161.

Validated on the vehicle:

- Apple Maps and Google Maps route guidance on the factory HUD
- real maneuver symbols from the authoritative CarPlay maneuver list
- first actionable maneuver at route start; the `START_ROUTE` pseudo-step is not used as the HUD maneuver when a real maneuver is already available
- numeric maneuver distance, including direct metric values below 1 km instead of the stock formatter's 50 m floor
- maneuver transitions after completing a turn
- side-street / junction geometry in the HUD maneuver graphic
- automatic recovery of RGI after a wireless CarPlay disconnect/reconnect that does not create a new iAP2 identification session
- route-end clear and normal BAP ownership hand-back behavior used by the existing integration

### Display scope

This release is **HUD-only**. CarPlay route guidance is not output as graphical route guidance in the Virtual Cockpit. While CarPlay owns route guidance, the K2161 integration keeps map visibility/presentation disabled for this path.

### Lane guidance

`0x5204` lane-guidance transport, caching, event selection and BAP output are implemented. The code follows iOS `laneGuidanceIndex` / `laneGuidanceShowing` state and can publish `CombiBAPNaviLaneGuidanceData` to K2161.

No qualifying real lane-guidance event was encountered during the v1.1 vehicle test, so **lane rendering is implemented but not yet live-validated**.

### Known observations

- On one route start, the first real maneuver symbol was correct but the HUD initially showed the destination distance instead of the first-maneuver distance. After passing that first maneuver, subsequent maneuver distances behaved normally.
- After a wireless reconnect, the previous maneuver can remain visible briefly while iOS rebuilds and republishes fresh authoritative route state. RGI recovery itself was successful.
- Waze remains dependent on whether the iOS/Waze session exposes the structured CarPlay RGI stream. A previously tested Waze session reported `sourceSupportsRouteGuidance=0` and produced no normal `0x5202` maneuver stream.

See [CHANGELOG.md](CHANGELOG.md) and [vehicle validation](docs/validation.md).

## Architecture

```text
iPhone navigation app
        |
        v
iAP2 Route Guidance
        |
        v
stock K2161 ipod-drvr-iap2.so
        |
        +-- runtime group 0x52 message-table activation
        |
        v
receive observer / RGI parser
        |
        v
localhost native -> Java bus
        |
        v
RouteGuidance -> BAPBridge
        |
        v
K2161 CombiBAPServiceNavi
        |
        v
factory Audi HUD
```

The stock iAP2 driver is **not modified on disk**. The K2161-specific native code validates the expected stock layout and changes only the required writable runtime state in memory.

See [architecture](docs/architecture.md) and [implementation details](docs/implementation.md).

## Build outputs

```text
scripts/build-native.sh
  -> build/libmib2high-carplay-rgi.so

scripts/build-java.sh
  -> build/mib2high-carplay-rgi.jar
```

The native build requires your own QNX SDP 6.5 environment. The Java build requires JDK 8 and a K2161 LSD compile-dependency JAR derived from the target firmware; the OEM-derived JAR is intentionally **not distributed**.

See [build instructions](docs/build.md).

## Installation

This repository contains source and integration documentation, not an automated MMI installer. Runtime integration requires:

1. the RGI message-direction entries in `iap2.cfg`;
2. process-specific preload of the native library into stock `mm-ipod`;
3. the Java bundle registered with the K2161 LSD/OSGi runtime.

See [installation requirements](docs/installation.md).

## Credits

The generic RGI parser/bus and parts of the Java route-guidance layer are adapted from **LuKa's `mib2q-carplay-rgi` work**. K2161/MIB2 High uses a different HARMAN integration path and additional stock-driver/BAP work.

See [NOTICE.md](NOTICE.md) for attribution.
