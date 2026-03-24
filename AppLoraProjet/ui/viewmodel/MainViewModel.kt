package com.example.loraprojet.ui.viewmodel

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.example.loraprojet.data.model.LoRaMessage
import com.example.loraprojet.data.repository.LoRaRepository
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

class MainViewModel(private val repository: LoRaRepository) : ViewModel() {

    val messages: StateFlow<List<LoRaMessage>> = repository.getMessages()
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5000), emptyList())

    val isConnected: StateFlow<Boolean> = repository.getConnectionStatus()
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5000), false)

    fun sendMessage(content: String) {
        viewModelScope.launch {
            repository.sendMessage(content)
        }
    }
}
