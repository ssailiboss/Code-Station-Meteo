/* Fichier principal : Station_Esclave.ino */

#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Wire.h>
#include <DHT20.h>
#include <DFRobot_RainfallSensor.h>
#include <Ticker.h>

// --- Adresse MAC du Maître ---
uint8_t macMaitre[] = {0x5C, 0xCF, 0x7F, 0x23, 0xC8, 0x2C};

// --- Ticker et Flag pour le DHT20 ---
Ticker tickerDHT;
volatile bool flagDHT = false;

// Fonction très courte qui lève le drapeau
void IRAM_ATTR triggerDHT() { flagDHT = true; }

// --- Timers pour le reste ---
unsigned long dernierTempsGirouette = 0;
unsigned long dernierTempsAnemo = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(4, 5); // SDA = D2(4), SCL = D1(5)

  // Initialisation via les autres onglets
  setupCapteurs();
  setupCommunication();

  // On attache le Ticker : le flag s'activera toutes les 2.0 secondes
  tickerDHT.attach(2.0, triggerDHT);

  Serial.println(F("=== Station météo Esclave (Extérieur) démarrée ==="));
}

void loop() {
  // 1. Lecture continue de l'anémomètre (Priorité absolue, sans timer)
  lectureAnemometre();

  // 2. Lecture de la Girouette (toutes les 500 ms)
  if (millis() - dernierTempsGirouette >= 500) {
    lireGirouette();
    dernierTempsGirouette = millis();
  }

  // 3. Lecture DHT20 (Conditionnée par le Drapeau du Ticker)
  if (flagDHT) {
    flagDHT = false; // On abaisse le drapeau immédiatement
    lireDHT20();
  }

  // 4. Calculs et Envoi (toutes les 1 seconde)
  if (millis() - dernierTempsAnemo >= 1000) {
    traiterEtEnvoyerDonnees();
    dernierTempsAnemo = millis();
  }
}