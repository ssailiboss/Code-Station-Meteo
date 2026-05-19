/*
  Fichier Principal : StationMeteo.ino
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h> // NOUVEAU : Pour le nom de domaine local
#include <WiFiUDP.h>
#include <NTPClient.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <SD.h>
#include <espnow.h>
#include <Ticker.h>

#define SD_CS D8
#define SCD40_ADDR 0x62
bool sdOk = false;

// --- NOUVEAU : Variables pour le Wi-Fi (lues depuis la carte SD) ---
String wifi_ssid = "";
String wifi_password = "";
unsigned long dernierTempsWifi = 0; // Pour la boucle de reconnexion

// ================= VARIABLES GLOBALES =================
float lastTemp = 0.0, lastHumidity = 0.0, lastPressure = 0.0;
uint16_t lastCo2 = 400;

float extTemp = 0.0, extHum = 0.0;
float lastWindSpeed = 0.0, lastWindAngle = 0.0;
float rainVolume = 0.0;
bool isRaining = false;
float station_altitude = 60.0;
int utc_offset_hours = 1;      // Valeur par défaut (+1h = heure d'hiver en Belgique)

float tempMax = -1000.0, tempMin = 1000.0;
bool esclaveConnecte = false; 
String currentDayStr = "";    

float pressureHistory[3] = {0.0, 0.0, 0.0}; 

typedef struct struct_message {
  float vitesseVent;
  float angleVent;
  float temperature;
  float humidite;
  bool  ilPleut;
  float pluieMm;
} struct_message;

struct_message donneesRecues;

// ================= TICKERS & FLAGS =================
Ticker tickerSCD40, tickerBMP280, tickerNTP;
volatile bool flagSCD40  = false;
volatile bool flagBMP280 = false;
volatile bool flagNTP    = false;

void IRAM_ATTR onTickerSCD40()  { flagSCD40  = true; }
void IRAM_ATTR onTickerBMP280() { flagBMP280 = true; }
void IRAM_ATTR onTickerNTP()    { flagNTP    = true; }

// ================= OBJETS =================
ESP8266WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600);
Adafruit_BMP280 bmp;

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  Wire.begin(4, 5);

  if (!bmp.begin(0x76)) { Serial.println("Erreur: BMP280 introuvable !"); }

  scd40_send_command(0x3F86); 
  delay(500);
  scd40_send_command(0x21B1);
  delay(100);

  // 1. Initialisation Carte SD EN PREMIER (Pour lire le Wi-Fi)
  sdOk = SD.begin(SD_CS);
  if (sdOk) {
    // Appel de la nouvelle fonction qui lit le fichier wifi.txt
    readConfig();
    
    if (!SD.exists("data.csv")) {
      File file = SD.open("data.csv", FILE_WRITE);
      if (file) { 
        file.println("Date,Heure,TempIn(C),HumIn(%),CO2(ppm),Pression(hPa),Vent(km/h),Dir(deg),TempExt(C),HumExt(%),Pluie(mm)"); 
        file.close();
      }
    }
  } else {
    Serial.println("Erreur SD ! On utilisera les codes Wi-Fi par defaut.");
  }

  // 2. Connexion Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setAutoReconnect(true); // Activer la reconnexion automatique en arrière-plan
  
  // Sécurité : si le fichier texte est vide ou introuvable, on remet ton OPPO par défaut
  if (wifi_ssid == "") wifi_ssid = "OPPO Find X5";
  if (wifi_password == "") wifi_password = "123456789";

  Serial.print("Connexion au Wi-Fi: " + wifi_ssid);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  
  // On attend max 10 secondes pour ne pas bloquer l'ESP s'il n'y a pas de Wi-Fi
  int tentatives = 0;
  while (WiFi.status() != WL_CONNECTED && tentatives < 20) { 
    delay(500); 
    Serial.print("."); 
    tentatives++; 
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi OK ! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nÉchec Wi-Fi (Il continuera de chercher en arrière-plan)");
  }

  // 3. Démarrage du mDNS (Le nom du site)
  if (MDNS.begin("stationmeteo")) {
    Serial.println("Serveur local démarré : http://stationmeteo.local");
    MDNS.addService("http", "tcp", 80);
  }

  // 4. Synchronisation Heure NTP
  timeClient.setTimeOffset(utc_offset_hours * 3600); // Convertit les heures en secondes (ex: 2 * 3600 = 7200)
  timeClient.begin();
  timeClient.update();
  currentDayStr = getDateString(); 

  // On charge les extrêmes après avoir la date
  if (sdOk) loadTempExtremesFromSD(); 

  // 5. Initialisation ESP-NOW
  if (esp_now_init() == 0) {
    esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
    esp_now_register_recv_cb(OnDataRecv);
  }

  setupEcran();

  // 6. Lancement des Tickers
  tickerSCD40.attach(300, onTickerSCD40);
  tickerBMP280.attach(600, onTickerBMP280);
  tickerNTP.attach(3600, onTickerNTP); 

  flagSCD40  = true;
  flagBMP280 = true;

  // 7. Serveur Web
  server.on("/logo.png", handleLogo);
  server.on("/", handleRoot);
  server.on("/download", handleDownload);
  server.on("/reset", handleReset);
  server.on("/data", handleData);
  server.on("/summary", handleSummary);
  server.begin();
}

void loop() {
  MDNS.update();
  server.handleClient();

  // Vérification de la connexion Wi-Fi (toutes les 10 secondes si perdu)
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - dernierTempsWifi > 10000) { 
      Serial.println("Wi-Fi perdu ! Tentative de reconnexion douce...");
      WiFi.disconnect();
      WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
      dernierTempsWifi = millis();
    }
  }

  if (flagSCD40) { flagSCD40 = false; doReadSCD40(); }
  if (flagBMP280) { flagBMP280 = false; doReadBMP280(); }
  if (flagNTP) { flagNTP = false; doSyncNTP(); }

  majEcran();
}
