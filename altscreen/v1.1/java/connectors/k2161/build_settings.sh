#!/usr/bin/env bash
# Firmware-specific compile dependency belongs to this target, not shared driver.
# K2161 OEM compile JAR must originate from the exact reference firmware.
ALTSCREEN_BASE_JAR="${ALTSCREEN_BASE_JAR:-${K2161_BASE_JAR:-}}"
: "${ALTSCREEN_BASE_JAR:?Provide verified K2161 OEM API compile JAR}"
