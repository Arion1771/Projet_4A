# Projet INFO4 : Réseau maillé sans fil pour les secours spéléologiques

---

## Description

Conception et implémentation d'un système de communication LoRa permettant de maintenir une transmission avec l'exterieur lors de deplacement en milieu souterrains (spéologie). Cela doit etre réalisé a l'aide d'un reseau modifiable de noeuds relais.

---

## Exigences

### Exigence Fonctionnelles
- Transmission fiable entre deux noeuds consecutifs sans perte d'information (Le systeme doit permettre l'échange de messages entre le noeud de depart et le noeud d'arrivée via une chaine de noeuds intermediaires)
- Architecture lineaire du réseau
- Chaque noeuds doit pouvoir communiquer de manière bidirectionnelle (chaque noeuds doit pouvoir communiquer avec son predecesseur et son successeur pour transmettre des messages dans les deux sens)
- Les messages du système doivent etre de type : SMS, d'alerte ou de communication reseau (état du reseau ou insertion d'un nouveau noeud dans la chaine)
- Détection de limite de portée entre deux noeuds consecutifs (le systeme doit detecter l'approche de la limite de portée du dernier noeuds posé, prevenant l'utilisateur qu'il faut ajouter un nouveau noeud à la chaine)
- L'utilisateur doit etre prevenu lorsque la limite de portée est atteinte
- Les messages d'alertes doivent etre traités en priorité sur le reseau et transmis a tous les noeuds de la chaine ainsi qu'a tous les utilisateurs connéctés (Les messages d'alertes doivent declencher une LED rouge sur chaque noeud et doivent etre signalés aux utilisateurs par un son et une vibration)
- Deploiment incrémental du réseau de noeuds (le systeme doit etre deployé au fil de l'avancée en souterrain, permettant a tout moment l'ajout d'un nouveau noeud relais. Il doit s'inserer automatiquement a l'allumage et confirmer cela par l'allumage d'une LED)
- Indication de l'etat de chaque noeud a travers une LED (éteinte:Hors tension / bleu: Allumé mais non connécté / verte:connécté au réseau / rouge:message d'alerte actif)
- Garantir une interface utilisateur par application smartphone (l'application doit permettre l'envoi et la reception de messages textuels ou d'alertes et l'etat de la connection au réseau (perdu, connécté ou limite atteinte))
- Garantir une livraison fiable des messages (avec retransmission en cas d'echec (mecanisme d'ACK))
- Connaissance de l'etat du réseau (Les noeuds doivent echanger des messages permettant de connaitre l'etat du réseau a tout moment (continuité de la chaine, nouvelle insertion de noeud ou modification du reseau))

### Exigence Non-Fonctionnelles
-
-
-
-
-
-
-


---

