# INFO4 Project - LoRa Mesh Network for Caving Rescue

## 1) Context

This repository contains a LoRa-based communication system designed to maintain a link between an underground team and the outside world through a linear chain of nodes.

Project period: 01/05/2026 -> 03/30/2026.

Main goals:
- reliable multi-node text communication
- priority emergency alert handling
- incremental deployment of new nodes
- network state supervision
- Android mobile interface

## 2) Overall Architecture

The repository contains 3 main sub-projects:
- `Loraprojet`: LoRa-E5 coordinator firmware (fixed ID 1)
- `WyresV2`: intermediate node firmware (STM32L151, auto-join + relay)
- `AppLoraProjet`: Android application (Jetpack Compose + Bluetooth SPP)

Target topology:

```text
[Android Smartphone]
        |
   Bluetooth SPP
        |
[Local gateway such as T-Beam]
        |
       LoRa
        |
[Coordinator ID=1] <-> [Node 2] <-> [Node 3] <-> ... <-> [Node N]
```

## 3) Repository Contents

```text
.
|-- AppLoraProjet/                # Android application
|-- Loraprojet/                   # LoRa-E5 coordinator firmware
|-- WyresV2/                      # WyresV2 node firmware (STM32L151)
|-- docs/
|   |-- usecase_diag.png
|   `-- sequence_diag.png
`-- SujetReseauxMaillesSpeleo.pdf
```

## 4) Implemented Features

### Network firmware (LoRa)
- application frames with a unified protocol header
- message types: `TEXT`, `ALERT`, `SUPERVISION`, `ACK`, `JOIN_REQ`, `JOIN_ACK`, `HEARTBEAT`
- ACK + retransmission mechanism for unicast messages
- duplicate filtering using `(src, seq)` tracking
- node join with sequential ID assignment (`2..250`)
- node supervision (online/offline) based on heartbeat and received traffic
- UART command console (`status`, `send`, `broadcast`, `nodes`, etc.)

### Coordinator-specific (`Loraprojet`)
- fixed coordinator mode (`ID=1`)
- join table management
- burst transmission of `JOIN_ACK`
- known node state tracking

### Intermediate node-specific (`WyresV2`)
- non-coordinator mode by default with automatic insertion
- linear relay of unicast frames
- dedicated broadcast relay for `JOIN_REQ/JOIN_ACK` to extend the chain
- range-limit detection through link quality (`LQ`) computed from RSSI/SNR
- LED blink behavior based on link quality
- LED OFF when the coordinator link is considered lost (`APP_LINK_LOST_MS`)

### Android application (`AppLoraProjet`)
- Compose chat UI for sent and received messages
- priority alert button (`ALERT:<message>`)
- Bluetooth status bar and LoRa network status display
- Bluetooth Classic SPP repository layer (serial RFCOMM UUID)

## 5) LoRa Protocol Summary

Application frame format:
- `version` (1 byte)
- `type` (1 byte)
- `src_id` (2 bytes)
- `dst_id` (2 bytes)
- `seq` (2 bytes)
- `ttl` (1 byte)
- `flags` (1 byte)
- `payload_len` (1 byte)
- `payload` (0..64 bytes)

Important parameters (current values):
- ACK timeout: `700 ms`
- max retries: `3`
- periodic heartbeat: `500 ms`
- node offline threshold: `10000 ms`

## 6) Hardware and Prerequisites

### Target hardware
- LoRa-E5 Development Kit (coordinator)
- WyresV2 boards (intermediate nodes)
- Android smartphone
- Bluetooth/LoRa gateway (for example T-Beam), depending on field integration

Hardware mentioned during the project:
- TTGO T-Beam classic
- Nucleo F411RE (flash/debug)
- LoRa-E5 Dev Kit
- WyresV2 x2

### Software tools
- STM32CubeIDE (recent version)
- Android Studio (for `AppLoraProjet`)
- JDK 11 (for Android/Gradle builds)

## 7) Build and Flash

### 7.1 Coordinator firmware (`Loraprojet`)
1. Open STM32CubeIDE.
2. Go to `File > Open Projects from File System...`
3. Select the `Loraprojet` folder.
4. Run `Project > Clean`.
5. Run `Project > Build`.
6. Flash the LoRa-E5 board from the IDE.

### 7.2 Intermediate node firmware (`WyresV2`)
1. Open STM32CubeIDE.
2. Go to `File > Open Projects from File System...`
3. Select the `WyresV2` folder.
4. Run `Project > Clean`.
5. Run `Project > Build`.
6. Flash the WyresV2 board.

Migration notes:
- see `WyresV2/MIGRATION_STM32L151.md`
- `Debug/` and `Release/` folders are local build artifacts and are ignored by git

### 7.3 Android application (`AppLoraProjet`)

Using Android Studio:
1. Open `AppLoraProjet`.
2. Wait for Gradle sync to complete.
3. Run the app on an Android device (API 24+).

Using the command line on Windows:

```powershell
cd AppLoraProjet
.\gradlew.bat assembleDebug
```

Generated APK:
- `AppLoraProjet/app/build/outputs/apk/debug/`

## 8) UART Commands

Recommended serial terminal settings:
- `115200 8N1`

### `Loraprojet` (coordinator)
- `id` -> show coordinator identity
- `nodes` -> list known nodes
- `status` -> print radio/protocol counters
- `dst <id>` -> set default destination
- `send <id> <text>` -> reliable send (ACK/retry)
- `broadcast <text>` -> broadcast send
- `help` -> show help
- `<text>` -> send to the default destination

### `WyresV2` (node)
- `id` -> show local identity (id, parent, destination, mode)
- `nodes` -> show node table (coordinator mode only)
- `status` -> counters + `LQ` + age of the last coordinator packet
- `dst <id>` -> set default destination
- `send <id> <text>` -> reliable send
- `broadcast <text>` -> broadcast send
- `join` -> force a join request
- `help` -> show help
- `<text>` -> send directly to `dst` (linear constraint `n-1` / `n+1`)

## 9) Important Configuration

### `Loraprojet/Src/main.c`
- fixed coordinator role (`APP_COORDINATOR_ID = 1`)
- `APP_CONSOLE_UART` selects the console UART

### `Loraprojet/Inc/radio_board_if.h`
- RF switch profile selected by `RBI_RF_SW_PROFILE`
- current profile is fixed to `RBI_RF_SW_PROFILE_LORAE5`

### `WyresV2/Src/main.c`
- `APP_COORDINATOR_MODE` (`0` by default)
- `APP_FIXED_NODE_ID` (optional forced node ID)
- link/LED thresholds:
- `APP_LINK_LOST_MS`
- `APP_LED_BLINK_FAST_MS`
- `APP_LED_BLINK_SLOW_MS`

### `WyresV2/Src/platform_port_stub.c`
- multi-profile radio detection (SX1272/SX1276)
- battery is currently stubbed (`platform_battery_mv()` returns 3900 mV)

## 10) Android Application - Current State

Working points:
- `Repository -> ViewModel -> Compose UI` architecture
- modern Bluetooth permissions (Android 12+)
- simple `STATUS:*` network status parsing

Known limitations:
- `connectToDevice(address)` exists but is not yet triggered by the UI
- a simulation block is still present in `LoRaRepositoryImpl.kt`
- Bluetooth-to-LoRa gateway protocol integration still needs to be finalized

## 11) Validation / Tests

The repository does not contain a full automated end-to-end firmware test suite.

Recommended validation steps:
1. Flash one coordinator and at least one node.
2. Check `JOIN_ACK` through UART logs.
3. Send a unicast message with `send` and verify `ACK OK`.
4. Test `broadcast`.
5. Power off one node and verify offline detection.
6. Check WyresV2 LED behavior when link quality degrades.

## 12) UML and Documentation

Available diagrams:
- Use case: `docs/usecase_diag.png`
- Message sending sequence: `docs/sequence_diag.png`

Project statement:
- `SujetReseauxMaillesSpeleo.pdf`

## 13) Suggested Roadmap

- finalize the `connect(...)` flow in the Android app
- remove the Android simulation code
- connect a real battery measurement on WyresV2
- add reproducible multi-node integration tests
- document a stable Bluetooth <-> LoRa gateway protocol

