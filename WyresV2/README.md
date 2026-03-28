# WyresV2 - Noeud Intermediaire LoRa

Ce dossier contient le firmware du noeud intermediaire WyresV2.

Objectif:
- insertion automatique dans une chaine lineaire LoRa,
- relais fiable (ACK + retransmissions),
- supervision de presence (heartbeats/deconnexions),
- pilotage LED d'etat.

## Configuration Runtime Par JSON (UART)

La config est modifiable a chaud depuis l'UART.

Commandes:
- `cfg` ou `config`: affiche la config courante en JSON
- `cfg reset`: remet les valeurs par defaut
- `cfg {json}`: applique un objet JSON

Notes importantes:
- Les cles sont sensibles a la casse (`heartbeatMs` != `heartbeatms`).
- Seuls des entiers positifs sont acceptes.
- La config est runtime uniquement (non persistante): reboot => defauts.

## Champs JSON - WyresV2

| Champ | Unite | Plage valide | Defaut | Effet |
|---|---:|---:|---:|---|
| `heartbeatMs` | ms | `200..60000` | `400` | Periode d'envoi des `MSG_HEARTBEAT` |
| `nodeOfflineMs` | ms | `1000..300000` | `5000` | Delai avant de declarer un noeud `disconnected` |
| `ackTimeoutMs` | ms | `100..10000` | `700` | Timeout avant retry d'un message fiable |
| `ackMaxRetries` | count | `0..10` | `3` | Nombre max de retransmissions ACK |
| `ackBurstRepeat` | count | `1..10` | `2` | Nombre d'ACK supplementaires envoyes en burst |
| `ackBurstDelayMs` | ms | `0..5000` | `80` | Delai avant le 1er ACK du burst |
| `ackBurstGapMs` | ms | `0..5000` | `80` | Ecart entre ACK d'un burst |
| `retryBackoffMs` | ms | `0..10000` | `120` | Backoff si emission retry impossible immediatement |
| `joinRetryMs` | ms | `200..60000` | `2000` | Periode de renvoi des `JOIN_REQ` |
| `linkLostMs` | ms | `500..120000` | `12000` | Seuil "coordinator link lost" (LED/lien) |
| `uartAutosubmitMs` | ms | `100..10000` | `3000` | Auto-envoi d'une ligne UART apres inactivite |

Recommandation:
- Garder `nodeOfflineMs` strictement superieur a `heartbeatMs`.

## Champs JSON - Coordinateur LoRaE5 (Loraprojet)

Le coordinateur supporte aussi `cfg` / `cfg reset` / `cfg {json}`.

| Champ | Unite | Plage valide | Defaut | Effet |
|---|---:|---:|---:|---|
| `nodeOfflineMs` | ms | `1000..300000` | `5000` | Delai avant de marquer un noeud offline |
| `ackTimeoutMs` | ms | `100..10000` | `700` | Timeout ACK cote coordinateur |
| `ackMaxRetries` | count | `0..10` | `3` | Retries max cote coordinateur |
| `ackBurstRepeat` | count | `1..10` | `2` | ACK burst cote coordinateur |
| `ackBurstDelayMs` | ms | `0..5000` | `80` | Delai initial du burst ACK |
| `ackBurstGapMs` | ms | `0..5000` | `80` | Ecart entre ACK burst |
| `joinAckRepeat` | count | `1..10` | `3` | Nombre de `JOIN_ACK` emis |
| `joinAckDelayMs` | ms | `0..5000` | `120` | Delai avant 1er `JOIN_ACK` |
| `joinAckGapMs` | ms | `0..5000` | `120` | Ecart entre `JOIN_ACK` repetes |
| `uartAutosubmitMs` | ms | `100..10000` | `900` | Auto-envoi ligne UART |

## Exemples JSON

WyresV2 (heartbeats rapides + timeout 5s):

```json
{"heartbeatMs":400,"nodeOfflineMs":5000}
```

WyresV2 (profil complet):

```json
{"heartbeatMs":350,"nodeOfflineMs":5000,"ackTimeoutMs":700,"ackMaxRetries":3,"ackBurstRepeat":2,"ackBurstDelayMs":80,"ackBurstGapMs":80,"retryBackoffMs":120,"joinRetryMs":2000,"linkLostMs":12000,"uartAutosubmitMs":3000}
```

Coordinateur LoRaE5:

```json
{"nodeOfflineMs":5000,"ackTimeoutMs":700,"ackMaxRetries":3,"ackBurstRepeat":2,"joinAckRepeat":3}
```

## Etats fonctionnels
- `NODE_OFF`: noeud eteint
- `NODE_BOOTING`: demarrage et auto-test
- `NODE_DISCOVERING`: detection du reseau et tentative d'insertion
- `NODE_INSERTED`: noeud actif dans la chaine
- `NODE_ALERT_ACTIVE`: alerte en cours, priorite maximale

## Types de messages supportes
- `MSG_TEXT`: message texte court
- `MSG_ALERT`: message d'alerte prioritaire
- `MSG_SUPERVISION`: telemetrie/etat lien
- `MSG_ACK`: acquittement de trame
- `MSG_JOIN_REQ`: demande d'insertion
- `MSG_JOIN_ACK`: confirmation d'insertion
- `MSG_HEARTBEAT`: keepalive voisin

## Integration hardware
Les fonctions dependantes de la plateforme (radio LoRa, LED, temps) sont declarees dans:
- `Inc/platform_port.h`

Le comportement metier est dans:
- `Src/wyresv2_node.c`

## Prochaines etapes integration
1. Brancher `platform_radio_send` et reception IRQ sur le driver LoRa reel.
2. Mapper les couleurs LED selon ton hardware WyresV2.
3. Brancher la mesure batterie dans `platform_battery_mv`.
4. Integrer ce module dans l'ordonnanceur principal (ou boucle principale).

## Ouverture STM32CubeIDE
1. `File > Open Projects from File System...`
2. Selectionner le dossier `WyresV2`.
3. Lancer `Project > Clean` puis `Build`.

Le projet utilise:
- `Startup/startup_stm32wle5jcix.s`
- `STM32WLE5JCIX_FLASH.ld`
- `Src/system_init.c` (stub de demarrage a remplacer ensuite par init horloge complete)
