package com.example.loraprojet.data.repository

import com.example.loraprojet.data.model.LoRaMessage
import kotlinx.coroutines.flow.Flow

interface LoRaRepository {
    fun getMessages(): Flow<List<LoRaMessage>>
    suspend fun sendMessage(content: String)
    suspend fun sendAlert(content: String)
    fun getConnectionStatus(): Flow<ConnectionStatus>
    fun getLoRaNetworkStatus(): Flow<LoRaNetworkStatus>
    suspend fun connectToDevice(address: String)
}

enum class ConnectionStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR
}

enum class LoRaNetworkStatus {
    UNKNOWN,
    NO_NODES,
    LOCAL_ONLY,
    GATEWAY_REACHABLE,
    CONNECTED_TO_EXTERIOR
}
