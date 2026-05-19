# Code-Station-Meteo
Projet de station météorologique connectée (ESP-NOW, Nextion, Serveur Web local).
Ce dépôt contient le code source de notre projet académique de station météo. Le système est divisé en deux unités communiquant via le protocole ESP-NOW.

## Structure du code
* **`/StationMeteo`** : Code de l'unité intérieure (Maître). Gère l'écran Nextion, la sauvegarde sur carte SD (CSV), le capteur de CO2/Pression et héberge le serveur Web local.
* **`/Station_Esclave`** : Code de l'unité extérieure. Gère l'acquisition de la pluviométrie, du vent (vitesse et direction) et de la température/humidité via des interruptions et requêtes I2C.

## Matériel utilisé
* Microcontrôleurs : 2x ESP8266 (FireBeetle / NodeMCU)
* Écran : Nextion NX4024T032
* Capteurs : DHT20, SCD40, BMP280, AS5600, Anémomètre optique, Pluviomètre.
* 
## ⚙️ Configuration et Installation

### 1. Préparation de la carte SD (Configuration système)
L'unité intérieure (Maître) repose sur une carte Micro SD (formatée en **FAT32**) pour stocker les données et charger la configuration. Cela permet de changer de réseau Wi-Fi ou de déménager la station sans jamais avoir à reprogrammer l'ESP8266.

Prenez la carte SD de l'unité intérieur, créez, si pas présent, un fichier texte nommé `config.txt` et renseignez vos paramètres selon ce format strict :

```text
SSID=Nom_De_Votre_Reseau_WiFi
PASS=Votre_Mot_De_Passe_WiFi
ALTITUDE=60.0
UTC_OFFSET=1
