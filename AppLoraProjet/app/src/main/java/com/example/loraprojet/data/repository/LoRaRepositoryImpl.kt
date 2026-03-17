package com.example.loraprojet.data.repository

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket
import android.content.Context
import com.example.loraprojet.data.model.LoRaMessage
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import java.io.IOException
import java.io.InputStream
import java.util.*

class LoRaRepositoryImpl(private val context: Context) : LoRaRepository {
    private val _messages = MutableStateFlow<List<LoRaMessage>>(emptyList())
    private val _connectionStatus = MutableStateFlow(ConnectionStatus.DISCONNECTED)
    private val _loRaNetworkStatus = MutableStateFlow(LoRaNetworkStatus.UNKNOWN)

    private val bluetoothManager: BluetoothManager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private val bluetoothAdapter: BluetoothAdapter? = bluetoothManager.adapter
    private var bluetoothSocket: BluetoothSocket? = null
    private val uuid: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB") // UUID standard pour SPP

    private val repositoryScope = CoroutineScope(Dispatchers.Main + Job())

    init {
        // --- A SUPPRIMER ---
        // Simulation d'un message extérieur reçu automatiquement après 10 secondes au démarrage
        repositoryScope.launch {
            delay(10000)
            val testMessage = LoRaMessage(
                sender = "Extérieur",
                content = "Ceci est un message de test simulant une réception LoRa.",
                isSentByMe = false
            )
            _messages.update { it + testMessage }
        }
        // --- A SUPPRIMER ---
    }

    override fun getMessages(): Flow<List<LoRaMessage>> = _messages.asStateFlow()

    @SuppressLint("MissingPermission")
    override suspend fun sendMessage(content: String) {
        val newMessage = LoRaMessage(sender = "Moi", content = content, isSentByMe = true)
        _messages.update { it + newMessage }

        withContext(Dispatchers.IO) {
            try {
                bluetoothSocket?.outputStream?.write(content.toByteArray())
                bluetoothSocket?.outputStream?.flush()
            } catch (e: IOException) {
                withContext(Dispatchers.Main) {
                    _connectionStatus.value = ConnectionStatus.ERROR
                }
            }
        }
    }

    @SuppressLint("MissingPermission")
    override suspend fun sendAlert(content: String) {
        val alertMessage = LoRaMessage(sender = "ALERTE", content = content, isSentByMe = true)
        _messages.update { it + alertMessage }

        withContext(Dispatchers.IO) {
            try {
                // Protocole d'alerte spécifique (ex: préfixe ALERT:)
                val alertData = "ALERT:$content"
                bluetoothSocket?.outputStream?.write(alertData.toByteArray())
                bluetoothSocket?.outputStream?.flush()
            } catch (e: IOException) {
                withContext(Dispatchers.Main) {
                    _connectionStatus.value = ConnectionStatus.ERROR
                }
            }
        }
    }

    override fun getConnectionStatus(): Flow<ConnectionStatus> = _connectionStatus.asStateFlow()
    override fun getLoRaNetworkStatus(): Flow<LoRaNetworkStatus> = _loRaNetworkStatus.asStateFlow()

    @SuppressLint("MissingPermission")
    override suspend fun connectToDevice(address: String) {
        if (_connectionStatus.value == ConnectionStatus.CONNECTED) return
        
        _connectionStatus.value = ConnectionStatus.CONNECTING
        withContext(Dispatchers.IO) {
            try {
                val device: BluetoothDevice? = bluetoothAdapter?.getRemoteDevice(address)
                bluetoothSocket = device?.createRfcommSocketToServiceRecord(uuid)
                bluetoothSocket?.connect()
                
                withContext(Dispatchers.Main) {
                    _connectionStatus.value = ConnectionStatus.CONNECTED
                    _loRaNetworkStatus.value = LoRaNetworkStatus.LOCAL_ONLY
                }
                
                startListening()
            } catch (e: IOException) {
                withContext(Dispatchers.Main) {
                    _connectionStatus.value = ConnectionStatus.ERROR
                    _loRaNetworkStatus.value = LoRaNetworkStatus.UNKNOWN
                }
                cleanup()
            }
        }
    }

    private suspend fun startListening() {
        withContext(Dispatchers.IO) {
            val buffer = ByteArray(1024)
            val inputStream: InputStream? = bluetoothSocket?.inputStream
            
            try {
                while (isActive && _connectionStatus.value == ConnectionStatus.CONNECTED) {
                    if (inputStream != null && inputStream.available() > 0) {
                        val bytes = inputStream.read(buffer)
                        if (bytes > 0) {
                            val rawData = String(buffer, 0, bytes)
                            
                            withContext(Dispatchers.Main) {
                                if (rawData.startsWith("STATUS:")) {
                                    updateNetworkStatus(rawData)
                                } else {
                                    val receivedMessage = LoRaMessage(
                                        sender = "T-Beam",
                                        content = rawData,
                                        isSentByMe = false
                                    )
                                    _messages.update { it + receivedMessage }
                                }
                            }
                        }
                    } else {
                        delay(100)
                    }
                }
            } catch (e: IOException) {
                withContext(Dispatchers.Main) {
                    _connectionStatus.value = ConnectionStatus.DISCONNECTED
                    _loRaNetworkStatus.value = LoRaNetworkStatus.UNKNOWN
                }
                cleanup()
            }
        }
    }

    private fun updateNetworkStatus(statusString: String) {
        _loRaNetworkStatus.value = when {
            statusString.contains("EXTERIOR") -> LoRaNetworkStatus.CONNECTED_TO_EXTERIOR
            statusString.contains("GATEWAY") -> LoRaNetworkStatus.GATEWAY_REACHABLE
            statusString.contains("NODES") -> LoRaNetworkStatus.LOCAL_ONLY
            else -> LoRaNetworkStatus.NO_NODES
        }
    }

    private fun cleanup() {
        try {
            bluetoothSocket?.close()
        } catch (e: IOException) {}
        bluetoothSocket = null
    }
}
