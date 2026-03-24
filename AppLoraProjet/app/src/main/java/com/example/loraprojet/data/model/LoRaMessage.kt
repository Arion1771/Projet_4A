package com.example.loraprojet.data.model

import java.util.Date

data class LoRaMessage(
    val id: String = java.util.UUID.randomUUID().toString(),
    val sender: String,
    val content: String,
    val timestamp: Long = System.currentTimeMillis(),
    val isSentByMe: Boolean = true
)
