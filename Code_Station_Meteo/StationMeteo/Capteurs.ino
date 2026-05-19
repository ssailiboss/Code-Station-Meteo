/*
  Fichier : Capteurs.ino
*/

void scd40_send_command(uint16_t cmd) {
  Wire.beginTransmission(SCD40_ADDR);
  Wire.write(cmd >> 8);
  Wire.write(cmd & 0xFF);
  Wire.endTransmission();
}

// --- Callback ESP-NOW ---
void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
  if (len == sizeof(donneesRecues)) {
    esclaveConnecte = true; 
    
    memcpy(&donneesRecues, incomingData, sizeof(donneesRecues));
    lastWindSpeed = donneesRecues.vitesseVent;
    lastWindAngle = donneesRecues.angleVent;
    extTemp       = donneesRecues.temperature;
    extHum        = donneesRecues.humidite;
    isRaining     = donneesRecues.ilPleut;
    rainVolume    = donneesRecues.pluieMm;

    String today = getDateString();
    if (today != currentDayStr && today.indexOf("1970") == -1) {
      currentDayStr = today;
      tempMax = -1000.0; 
      tempMin = 1000.0;  
    }

    if (extTemp > tempMax) tempMax = extTemp;
    if (extTemp < tempMin) tempMin = extTemp;
  }
}

// --- Lecture SCD40 ---
void doReadSCD40() {
  scd40_send_command(0xE4B8);
  delay(2);
  Wire.requestFrom(SCD40_ADDR, 3);
  uint16_t ready = (Wire.read() << 8) | Wire.read();
  Wire.read();

  if ((ready & 0x07FF) != 0) {
    scd40_send_command(0xEC05);
    delay(2);
    Wire.requestFrom(SCD40_ADDR, 9);
    
    uint8_t buf[9];
    for (int i = 0; i < 9; i++) buf[i] = Wire.read();
    
    lastCo2      = (buf[0] << 8) | buf[1];
    lastTemp     = -45.0 + 175.0 * ((buf[3] << 8) | buf[4]) / 65535.0;
    lastHumidity = 100.0 * ((buf[6] << 8) | buf[7]) / 65535.0;

    if (esclaveConnecte) {
      writeToSD(getDateString(), getTimeString());
    }
    Serial.printf("SCD40 -> CO2: %d ppm | Temp: %.1f °C | Hum: %.1f %%\n", lastCo2, lastTemp, lastHumidity);
  } else {
    delay(1000);
    flagSCD40 = true; 
  }
}

// --- Lecture BMP280 ---
void doReadBMP280() {
  // Lecture de la pression brute absolue en hPa
  float pressureAbsolue = bmp.readPressure() / 100.0;
  
  // Calcul de la pression relative ajustée dynamiquement selon l'altitude de la SD
  lastPressure = pressureAbsolue + (station_altitude / 8.3);
  
  // Mise à jour de ton historique pour l'algorithme de tendance
  pressureHistory[0] = pressureHistory[1];
  pressureHistory[1] = pressureHistory[2];
  pressureHistory[2] = lastPressure;
}

// --- Synchro Heure ET Historique Météo ---
void doSyncNTP() {
  timeClient.update();
  
  // Le Ticker NTP s'exécute toutes les heures (3600s).
  // On en profite pour décaler notre historique de pression pour calculer la tendance sur 3H !
  if (lastPressure > 0) {
    pressureHistory[2] = pressureHistory[1]; // H-3 devient H-2
    pressureHistory[1] = pressureHistory[0]; // H-2 devient H-1
    pressureHistory[0] = lastPressure;       // H-1 devient l'heure actuelle
  }
}
