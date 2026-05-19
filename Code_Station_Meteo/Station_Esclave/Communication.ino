/* Fichier : Communication.ino */

// Import de la librairie native de l'ESP8266 pour forcer la fréquence radio
extern "C" {
  #include <user_interface.h>
}

// --- Structure ESP-NOW ---
typedef struct struct_message {
  float vitesseVent;
  float angleVent;
  float temperature;
  float humidite;
  bool  ilPleut;
  float pluieMm;
} struct_message;

struct_message donneesVent;

// --- NOUVEAU : Variables pour le scan Auto-Canal ---
uint8_t canalActuel = 1;
bool canalTrouve = false;
int echecsConsecutifs = 0;

void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  if (sendStatus == 0) {
    if (!canalTrouve) {
      Serial.print(F("✅ Maître trouvé et verrouillé sur le canal "));
      Serial.println(canalActuel);
    }
    canalTrouve = true;
    echecsConsecutifs = 0; // Réinitialise le compteur
  } else {
    canalTrouve = false;
    echecsConsecutifs++; // Compte les échecs pour déclencher la recherche
  }
}

void setupCommunication() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); // CRUCIAL : L'esclave ne se connecte à AUCUNE box Wi-Fi !

  if (esp_now_init() != 0) {
    Serial.println(F("❌ Erreur init ESP-NOW"));
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(OnDataSent);
  
  // On enregistre le Maître sur le canal 1 par défaut pour commencer
  esp_now_add_peer(macMaitre, ESP_NOW_ROLE_SLAVE, canalActuel, NULL, 0);

  Serial.println(F("📡 Mode Scanner Auto-Canal activé. Totalement indépendant du SSID !"));
}

String getDirectionTexte(float angle) {
  int index = (int)((angle + 22.5) / 45.0) % 8;
  const char* directions[] = {"Nord", "Nord-Est", "Est", "Sud-Est", "Sud", "Sud-Ouest", "Ouest", "Nord-Ouest"};
  return String(directions[index]);
}

void traiterEtEnvoyerDonnees() {
  // 1. Calculer Vitesse Vent (puis remise à zéro des impulsions)
  float vitesseKMH = (compteImpulsions / 3.0) * facteurVitesse;
  compteImpulsions = 0; 

  // 2. Mettre à jour la pluie
  calculerPluie();

  // 3. NOUVEAU : Gestion de la recherche du canal (Auto-Hopping)
  if (!canalTrouve && echecsConsecutifs > 0) {
    // On efface l'ancienne configuration radio
    esp_now_del_peer(macMaitre);
    
    // On passe au canal radio suivant (boucle de 1 à 13)
    canalActuel++;
    if (canalActuel > 13) canalActuel = 1;

    // Force la puce Wi-Fi de l'ESP8266 sur la nouvelle fréquence
    wifi_promiscuous_enable(1);
    wifi_set_channel(canalActuel);
    wifi_promiscuous_enable(0);

    // Ré-enregistre le Maître avec le nouveau canal
    esp_now_add_peer(macMaitre, ESP_NOW_ROLE_SLAVE, canalActuel, NULL, 0);

    Serial.print(F("Recherche Maître... test du canal "));
    Serial.println(canalActuel);
  }

  // 4. Préparer le paquet
  donneesVent.vitesseVent = vitesseKMH;
  donneesVent.angleVent   = angleActuel;
  donneesVent.temperature = temperatureActuelle;
  donneesVent.humidite    = humiditeActuelle;
  donneesVent.ilPleut     = ilPleut;
  donneesVent.pluieMm     = pluieMM;

  // 5. Envoyer
  esp_now_send(macMaitre, (uint8_t *) &donneesVent, sizeof(donneesVent));

  // 6. Affichage Série
  Serial.print(F("[Vent] Vitesse: "));
  Serial.print(vitesseKMH, 2);
  Serial.print(F(" km/h | Dir: "));
  Serial.print(angleActuel, 1);
  Serial.print(F("° ("));
  Serial.print(getDirectionTexte(angleActuel));
  Serial.print(F(") | Pluie: "));
  Serial.print(ilPleut ? F("OUI") : F("NON"));
  Serial.print(F(" ("));
  Serial.print(pluieMM, 2);
  Serial.println(F(" mm)"));
}
