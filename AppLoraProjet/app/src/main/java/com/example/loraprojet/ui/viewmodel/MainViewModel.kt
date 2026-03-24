package com.example.loraprojet.ui.viewmodel

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.example.loraprojet.data.model.LoRaMessage
import com.example.loraprojet.data.repository.ConnectionStatus
import com.example.loraprojet.data.repository.LoRaNetworkStatus
import com.example.loraprojet.data.repository.LoRaRepository
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

class MainViewModel(private val repository: LoRaRepository) : ViewModel() {

    val messages: StateFlow<List<LoRaMessage>> = repository.getMessages()
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5000), emptyList())

    val connectionStatus: StateFlow<ConnectionStatus> = repository.getConnectionStatus()
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5000), ConnectionStatus.DISCONNECTED)

    val loRaNetworkStatus: StateFlow<LoRaNetworkStatus> = repository.getLoRaNetworkStatus()
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5000), LoRaNetworkStatus.UNKNOWN)

    fun sendMessage(content: String) {
        viewModelScope.launch {
            repository.sendMessage(content)
        }
    }

    fun sendAlert(content: String) {
        viewModelScope.launch {
            repository.sendAlert(content)
        }
    }

    fun connect(address: String) {
        viewModelScope.launch {
            repository.connectToDevice(address)
        }
    }
}
