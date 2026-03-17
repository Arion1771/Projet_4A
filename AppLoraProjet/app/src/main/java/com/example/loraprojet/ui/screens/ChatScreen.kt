package com.example.loraprojet.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.Send
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.loraprojet.data.model.LoRaMessage
import com.example.loraprojet.data.repository.ConnectionStatus
import com.example.loraprojet.data.repository.LoRaNetworkStatus
import com.example.loraprojet.ui.viewmodel.MainViewModel

@Composable
fun ChatScreen(viewModel: MainViewModel, modifier: Modifier = Modifier) {
    val messages by viewModel.messages.collectAsState()
    val btStatus by viewModel.connectionStatus.collectAsState()
    val loraStatus by viewModel.loRaNetworkStatus.collectAsState()
    var textState by remember { mutableStateOf("") }
    val listState = rememberLazyListState()
    
    var showAlertDialog by remember { mutableStateOf(false) }

    LaunchedEffect(messages.size) {
        if (messages.isNotEmpty()) {
            listState.animateScrollToItem(messages.size - 1)
        }
    }

    if (showAlertDialog) {
        AlertDialog(
            onDismissRequest = { showAlertDialog = false },
            title = { Text("CONFIRMER L'ALERTE") },
            text = { Text("Êtes-vous sûr de vouloir envoyer une alerte sur le réseau ?") },
            confirmButton = {
                Button(
                    onClick = {
                        viewModel.sendAlert("ALERTE : BESOIN D'ASSISTANCE")
                        showAlertDialog = false
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = Color.Red)
                ) {
                    Text("ENVOYER")
                }
            },
            dismissButton = {
                TextButton(onClick = { showAlertDialog = false }) {
                    Text("ANNULER")
                }
            },
            icon = { Icon(Icons.Default.Warning, contentDescription = null, tint = Color.Red) }
        )
    }

    Column(modifier = modifier.fillMaxSize()) {
        // Barre de statut réseau
        NetworkStatusBar(btStatus, loraStatus)

        // Zone de messages (Partie haute)
        LazyColumn(
            state = listState,
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth()
                .padding(horizontal = 8.dp)
        ) {
            items(messages) { message ->
                MessageItem(message)
            }
        }

        // Zone de contrôle (Partie basse)
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .background(MaterialTheme.colorScheme.surface)
        ) {
            // Barre de saisie classique
            Surface(tonalElevation = 4.dp) {
                Row(
                    modifier = Modifier
                        .padding(8.dp)
                        .imePadding(),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    TextField(
                        value = textState,
                        onValueChange = { textState = it },
                        modifier = Modifier.weight(1f),
                        placeholder = { Text("Entrez un message...") },
                        colors = TextFieldDefaults.colors(
                            focusedContainerColor = Color.Transparent,
                            unfocusedContainerColor = Color.Transparent
                        ),
                        maxLines = 2
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    FloatingActionButton(
                        onClick = {
                            if (textState.isNotBlank()) {
                                viewModel.sendMessage(textState)
                                textState = ""
                            }
                        },
                        containerColor = MaterialTheme.colorScheme.primary,
                        modifier = Modifier.size(48.dp),
                        shape = CircleShape,
                        elevation = FloatingActionButtonDefaults.elevation(0.dp)
                    ) {
                        Icon(Icons.AutoMirrored.Filled.Send, contentDescription = "Envoyer")
                    }
                }
            }

            // Bouton d'alerte (prend environ 1/3 du bas de l'écran ou une hauteur importante)
            Button(
                onClick = { showAlertDialog = true },
                modifier = Modifier
                    .fillMaxWidth()
                    .height(120.dp) // Hauteur importante pour occuper de l'espace
                    .padding(12.dp),
                colors = ButtonDefaults.buttonColors(
                    containerColor = Color(0xFFB71C1C), // Rouge foncé
                    contentColor = Color.White
                ),
                shape = MaterialTheme.shapes.medium
            ) {
                Icon(
                    Icons.Default.Warning, 
                    contentDescription = null, 
                    modifier = Modifier.size(32.dp)
                )
                Spacer(modifier = Modifier.width(16.dp))
                Text(
                    text = "ALERTE URGENCE",
                    fontSize = 24.sp,
                    fontWeight = FontWeight.Black
                )
            }
            Spacer(modifier = Modifier.navigationBarsPadding())
        }
    }
}

@Composable
fun NetworkStatusBar(btStatus: ConnectionStatus, loraStatus: LoRaNetworkStatus) {
    val (color, text) = when (btStatus) {
        ConnectionStatus.DISCONNECTED -> Color(0xFFF44336) to "Non connecté à la T-Beam"
        ConnectionStatus.CONNECTING -> Color(0xFFFFC107) to "Connexion à la T-Beam..."
        ConnectionStatus.ERROR -> Color(0xFFB71C1C) to "Erreur Bluetooth"
        ConnectionStatus.CONNECTED -> {
            when (loraStatus) {
                LoRaNetworkStatus.UNKNOWN -> Color(0xFF9E9E9E) to "T-Beam OK | Réseau LoRa : Initialisation..."
                LoRaNetworkStatus.NO_NODES -> Color(0xFFFF9800) to "T-Beam OK | Aucun autre nœud détecté"
                LoRaNetworkStatus.LOCAL_ONLY -> Color(0xFF2196F3) to "T-Beam OK | Réseau local uniquement"
                LoRaNetworkStatus.GATEWAY_REACHABLE -> Color(0xFF4CAF50) to "T-Beam OK | Gateway LoRa détectée"
                LoRaNetworkStatus.CONNECTED_TO_EXTERIOR -> Color(0xFF4CAF50) to "T-Beam OK | Connexion Extérieur OK"
            }
        }
    }

    Surface(
        color = color.copy(alpha = 0.15f),
        modifier = Modifier.fillMaxWidth()
    ) {
        Row(
            modifier = Modifier.padding(12.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.Center
        ) {
            Box(
                modifier = Modifier
                    .size(10.dp)
                    .background(color, CircleShape)
            )
            Spacer(modifier = Modifier.width(12.dp))
            Text(
                text = text,
                color = color,
                fontSize = 13.sp,
                fontWeight = FontWeight.Bold
            )
        }
    }
}

@Composable
fun MessageItem(message: LoRaMessage) {
    val alignment = if (message.isSentByMe) Alignment.End else Alignment.Start
    val isAlert = message.sender == "ALERTE"
    
    val bubbleColor = when {
        isAlert -> Color(0xFFB71C1C)
        message.isSentByMe -> MaterialTheme.colorScheme.primary
        else -> MaterialTheme.colorScheme.surfaceVariant
    }
    
    val textColor = if (message.isSentByMe || isAlert) Color.White else MaterialTheme.colorScheme.onSurfaceVariant

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        horizontalAlignment = alignment
    ) {
        Surface(
            color = bubbleColor,
            shape = MaterialTheme.shapes.large,
            tonalElevation = if (isAlert) 8.dp else 0.dp
        ) {
            Text(
                text = message.content,
                modifier = Modifier.padding(horizontal = 16.dp, vertical = 10.dp),
                color = textColor,
                style = if (isAlert) MaterialTheme.typography.titleMedium else MaterialTheme.typography.bodyLarge,
                fontWeight = if (isAlert) FontWeight.Bold else FontWeight.Normal
            )
        }
        Text(
            text = if (message.isSentByMe) "Moi" else message.sender,
            style = MaterialTheme.typography.labelSmall,
            modifier = Modifier.padding(top = 2.dp, start = 4.dp, end = 4.dp),
            color = MaterialTheme.colorScheme.outline
        )
    }
}
