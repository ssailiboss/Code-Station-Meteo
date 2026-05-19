/* Fichier : Capteurs.ino */

// --- Variables partagées (lues par Communication) ---
float angleActuel = 0.0;
float temperatureActuelle = 0.0;
float humiditeActuelle = 0.0;
float pluieMM = 0.0;
bool ilPleut = false;

// --- Paramètres Girouette ---
const int AS5600_ADDR = 0x36;

// --- Paramètres DHT20 ---
DHT20 dht20;

// --- Paramètres Pluviomètre ---
DFRobot_RainfallSensor_I2C rain(&Wire);
float baselinePluie = 0.0;
float dernierePluieBrute = 0.0;
unsigned long dernierTempsPluie = 0;
const unsigned long TIMEOUT_PLUIE = 5UL * 60UL * 1000UL; // 5 minutes

// --- Paramètres Anémomètre ---
const int analogPin = A0;
const float rayonCM = 3.5;
const float pi_val = 3.14159;
const float facteurEtalonnage = 2.5; // Constante aérodynamique standard
const float facteurVitesse = (2 * pi_val * (rayonCM / 100.0)) * 3.6 * facteurEtalonnage;
int seuilDetection = 600;
bool auDessusDuSeuil = false;
unsigned long compteImpulsions = 0;

void setupCapteurs() {
  // Setup Pluie
  while (!rain.begin()) {
    Serial.println(F("⚠️ Capteur pluie non détecté, nouvelle tentative..."));
    delay(1000);
    yield();
  }
  rain.setRainAccumulatedValue(0.2794);
  Serial.println(F("✅ Capteur pluie I2C initialisé."));

  // Setup DHT20
  dht20.begin();
  Serial.println(F("✅ DHT20 initialisé."));

  // Initialisation Baseline Pluie
  baselinePluie = rain.getRainfall();
  dernierePluieBrute = baselinePluie;
  Serial.print(F("📊 Baseline pluie initialisée à : "));
  Serial.print(baselinePluie, 2);
  Serial.println(F(" mm (ignoré)"));

  // Faire une première lecture DHT pour avoir des valeurs initiales
  lireDHT20();
}

void lectureAnemometre() {
  int lecture = analogRead(analogPin);
  if (lecture > seuilDetection && !auDessusDuSeuil) {
    compteImpulsions++;
    auDessusDuSeuil = true;
  } else if (lecture < (seuilDetection - 100)) {
    auDessusDuSeuil = false;
  }
}

void lireGirouette() {
  Wire.beginTransmission(AS5600_ADDR);
  Wire.write(0x0E);
  Wire.endTransmission();
  Wire.requestFrom(AS5600_ADDR, 2);

  if (Wire.available() == 2) {
    int hi = Wire.read();
    int lo = Wire.read();
    int raw = (hi << 8) | lo;
    angleActuel = (raw * 360.0) / 4096.0;
  }
}

void lireDHT20() {
  dht20.read();
  temperatureActuelle = dht20.getTemperature();
  humiditeActuelle = dht20.getHumidity();

  Serial.print(F("[Météo] Temp: "));
  Serial.print(temperatureActuelle, 1);
  Serial.print(F(" °C | Humidité: "));
  Serial.print(humiditeActuelle, 1);
  Serial.println(F(" %"));
}

void calculerPluie() {
  float pluieBrute = rain.getRainfall();
  pluieMM = pluieBrute - baselinePluie;

  // Si de nouvelles gouttes tombent
  if (pluieBrute > dernierePluieBrute) {
    dernierePluieBrute = pluieBrute;
    dernierTempsPluie = millis();
  }

  // Premier démarrage avec pluie déjà accumulée
  if (dernierTempsPluie == 0 && pluieMM > 0.0) {
    dernierTempsPluie = millis();
  }

  // Reset auto si 5 min de temps sec
  if (dernierTempsPluie != 0 && millis() - dernierTempsPluie >= TIMEOUT_PLUIE) {
    baselinePluie = pluieBrute;
    dernierePluieBrute = pluieBrute;
    dernierTempsPluie = 0;
    pluieMM = 0.0;
    Serial.println(F("🔄 Pluie réinitialisée (5 min sans pluie)"));
  }

  ilPleut = (pluieMM > 0.0);
}