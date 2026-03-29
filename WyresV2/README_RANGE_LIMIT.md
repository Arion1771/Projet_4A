# WyresV2 - Range Limit Warning

This document describes the range-limit warning feature added to the current LoRa mesh behavior.

## Goal
- warn when a node is approaching radio range limit
- make visual feedback immediate on Wyres boards
- detect link loss quickly and clearly

## Implemented behavior
- Coordinator now acknowledges `MSG_HEARTBEAT` with `MSG_ACK`.
- Each non-coordinator Wyres node computes a smoothed link quality value (`LQ`) from coordinator `RSSI` and `SNR`.
- LED blink speed depends on link quality:
  - good link -> slow blink
  - weak link / near limit -> fast blink
- If coordinator link is considered lost, LED is forced OFF.

## Link quality model
- Per packet sample built from:
  - `RSSI score` (weighted 70%)
  - `SNR score` (weighted 30%)
- Smoothed with EWMA to avoid unstable behavior.

## LED behavior rules
- Applies to non-coordinator nodes after join.
- Before first coordinator sample: LED ON (steady).
- With valid link sample: LED toggles with period mapped from `LQ`.
- Link loss timeout reached: LED OFF.

## Key tuning parameters
In `Src/main.c`:
- `APP_LINK_LOST_MS` (current: `12000`)
- `APP_LED_BLINK_FAST_MS` (current: `120`)
- `APP_LED_BLINK_SLOW_MS` (current: `1000`)
- `APP_HEARTBEAT_PERIOD_MS`
- `APP_NODE_OFFLINE_MS`

## UART diagnostics
`status` now includes:
- `LQ=<0..100>`: current smoothed link quality
- `LAge=<ms>`: elapsed time since last coordinator frame

Runtime logs:
- `WARN: coordinator link lost, LED off`
- `INFO: coordinator link restored`

## Files touched by this feature
- `WyresV2/Src/main.c`
- `Loraprojet/Src/main.c`

