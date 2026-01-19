# Projet INFO4 : Réseau maillé sans fil pour les secours spéléologiques

---

## Description

Conception et implémentation d'un système de communication LoRa permettant de maintenir une transmission avec l'exterieur lors de déplacement en milieu souterrain (spéléologie). Cela doit être réalisé à l'aide d'un réseau modifiable de noeuds relais.

---

## Exigences

### Exigences Fonctionnelles
- Transmission fiable entre deux noeuds consecutifs sans perte d'informations (Le systeme doit permettre l'échange de messages entre le noeud de départ et le noeud d'arrivée via une chaine de noeuds intermédiaires)
- Architecture linéaire du réseau
- Chaque noeud doit pouvoir communiquer de manière bidirectionnelle (chaque noeud doit pouvoir communiquer avec son prédecesseur et son successeur pour transmettre des messages dans les deux sens)
- Les messages du système doivent être de type : SMS, d'alerte ou de communication réseau (état du réseau ou insertion d'un nouveau noeud dans la chaine)
- Détection de limite de portée entre deux noeuds consecutifs (le systeme doit detecter l'approche de la limite de portée du dernier noeuds posé, prevenant l'utilisateur qu'il faut ajouter un nouveau noeud à la chaine)
- L'utilisateur doit etre prévenu lorsque la limite de portée est atteinte
- Les messages d'alertes doivent etre traités en priorité sur le réseau et transmis à tous les noeuds de la chaine ainsi qu'à tous les utilisateurs connéctés (Les messages d'alertes doivent declencher une LED rouge sur chaque noeud et doivent etre signalés aux utilisateurs par un son et une vibration)
- Deploiment incrémental du réseau de noeuds (le systeme doit etre deployé au fil de l'avancée en souterrain, permettant à tout moment l'ajout d'un nouveau noeud relais. Il doit s'inserer automatiquement à l'allumage et confirmer cela par l'allumage d'une LED)
- Indication de l'état de chaque noeud à travers une LED (Eteinte : Hors tension / Bleu : Allumé mais non connécté / Verte : connécté au réseau / Rouge : message d'alerte actif)
- Garantir une interface utilisateur par application smartphone (l'application doit permettre l'envoi et la reception de messages textuels ou d'alertes et l'état de la connection au réseau (perdu, connécté ou limite atteinte))
- Garantir une livraison fiable des messages (avec retransmission en cas d'echec (mécanisme d'ACK))
- Connaissance de l'état du réseau (Les noeuds doivent échanger des messages permettant de connaitre l'état du réseau à tout moment (continuité de la chaine, nouvelle insertion de noeud ou modification du reseau))

### Exigences Non-Fonctionnelles
- La fiabilité doit etre privilégiée (par rapport au débit et à la latence)
- Les messages d'alertes doivent toujours être transmis en priorité
- Le système doit pouvoir fonctionner en milieu souterrain (propagation radio fortement attenuée et imprévisible et environnement evolutif (humide et instable))
- La consommation énergitique doit être maitrisée (les noeuds doivent fonctionner sur batterie rechargeable et les communications doivent utiliser le minimum d'énergie)
- Le système doit etre simple et rapide d'usage (UI minimale et application simple d'utilisation permettant la communication rapide)

---
## Etape 1

---
## Etape 1

---
## ...

---
## Etape n



