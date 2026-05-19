/*
  Fichier : Stockage.ino
*/

// --- NOUVEAU : Fonction pour lire ou créer le fichier de configuration Wi-Fi ---
void readConfig() {
  if (!sdOk) return;

  // 1. Si le fichier n'existe pas, on le crée avec l'altitude par défaut
  if (!SD.exists("config.txt")) {
    File file = SD.open("config.txt", FILE_WRITE);
    if (file) {
      file.println("SSID=OPPO Find X5");
      file.println("PASS=123456789");
      file.println("ALTITUDE=60.0"); // Ajout du paramètre sur la SD
      file.println("UTC_OFFSET=2"); // +2000 ppm ou heure d'été par défaut
      file.close();
      Serial.println("Fichier config.txt créé avec les paramètres par défaut.");
    }
  }

  // 2. Lecture du fichier ligne par ligne
  File file = SD.open("config.txt", FILE_READ);
  if (!file) {
    Serial.println("Erreur d'ouverture de config.txt");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n'); // Lecture de la ligne
    line.trim(); // Supprime les espaces invisibles et retours à la ligne
    
    if (line.startsWith("SSID=")) {
      wifi_ssid = line.substring(5); // Récupère le SSID
    } else if (line.startsWith("PASS=")) {
      wifi_password = line.substring(5); // Récupère le mot de passe
    } else if (line.startsWith("ALTITUDE=")) {
      station_altitude = line.substring(9).toFloat(); // Récupère l'altitude
    } else if (line.startsWith("UTC_OFFSET=")) {
      utc_offset_hours = line.substring(11).toInt(); // Récupère le décalage horaire
    }
  }
  file.close();
  Serial.println("Configuration chargée depuis la carte SD :");
  Serial.print("-> SSID : "); Serial.println(wifi_ssid);
  Serial.print("-> Altitude : "); Serial.print(station_altitude); Serial.println(" m");
  Serial.print("-> Décalage UTC : "); Serial.print(utc_offset_hours); Serial.println(" heure(s)");
}

void loadTempExtremesFromSD() {
  if (!sdOk || !SD.exists("data.csv")) return;
  File file = SD.open("data.csv", FILE_READ);
  if (!file) return;
  
  file.readStringUntil('\n'); // Ignorer l'en-tête
  
  tempMax = -1000.0;
  tempMin = 1000.0;
  
  String today = getDateString(); 
  
  while (file.available()) {
    majEcran(); yield();
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;
    
    int c[10];
    c[0] = line.indexOf(',');
    for(int i=1; i<10; i++) c[i] = line.indexOf(',', c[i-1] + 1);
    if (c[0] < 0 || c[9] < 0) continue;
    
    String day = line.substring(0, c[0]);
    if (day != today) continue; 
    
    float temp = line.substring(c[7] + 1, c[8]).toFloat();
    
    if (temp > tempMax) tempMax = temp;
    if (temp < tempMin) tempMin = temp;
  }
  file.close();
}

void writeToSD(String date, String heure) {
  if (!sdOk) return;
  File file = SD.open("data.csv", FILE_WRITE);
  if (!file) return;
  
  file.printf("%s,%s,%.2f,%.2f,%d,%.2f,%.2f,%.1f,%.2f,%.2f,%.1f\n", 
              date.c_str(), 
              heure.c_str(), 
              lastTemp, 
              lastHumidity, 
              lastCo2, 
              lastPressure, 
              lastWindSpeed, 
              lastWindAngle,
              extTemp, 
              extHum, 
              rainVolume);
  file.close();
}
