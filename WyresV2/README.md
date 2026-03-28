# WyresV2 - Noeud Intermediaire LoRa

Ce dossier contient un projet firmware dedie au noeud intermediaire WyresV2.

Objectif:
- insertion automatique dans une chaine lineaire LoRa,
- relais fiable (ACK + retransmissions),
- priorite absolue des alertes,
- supervision du lien voisin (pred/succ),
- pilotage LED d'etat.

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
