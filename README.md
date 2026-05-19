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
