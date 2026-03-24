# LoRaSpéléo Android App

This is an Android application developed for the **LoRaSpéléo** project. It allows communication with a **T-Beam** board via Bluetooth to send and receive messages through a LoRa network.

## Features
- **Real-time Chat**: Send and receive messages via LoRa.
- **Network Status**: Monitor the connection state between your phone, the T-Beam, and the LoRa network.
- **Emergency Alert**: Send high-priority alert messages to all nodes in the network with a single dedicated button.
- **Bluetooth Connectivity**: Connects to T-Beam boards using Bluetooth Classic (SPP).

## Prerequisites
- An Android device (API 24+) with Bluetooth.
- A **LilyGO T-Beam** board (or similar) flashed with compatible firmware.

## Getting Started
1. **Clone the repository**:
   ```bash
   git clone <repository-url>
   ```
2. **Open in Android Studio**:
   Launch Android Studio and select "Open an existing project", then choose the cloned folder.
3. **Connect your device**:
   - Enable **Developer Options** and **USB Debugging** on your phone.
   - Connect your phone to your computer via USB.
4. **Build and Run**:
   - Click the green **Run** button in Android Studio.
   - Grant the required Bluetooth and Location permissions on your phone.

## Bluetooth Setup
The app currently looks for a T-Beam device. To connect to your specific board, ensure it is paired with your phone in the Android Bluetooth settings.

## Project Structure
- `data/`: Contains the LoRa message model and the Repository for Bluetooth communication.
- `ui/`: Contains the Jetpack Compose screens and ViewModels for the chat interface.

## Disclaimer
This application is part of a specific project. Simulation code is currently included in `LoRaRepositoryImpl.kt` for testing purposes (marked with `// --- A SUPPRIMER ---`).
