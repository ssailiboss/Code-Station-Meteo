/*
  Fichier : Ecran.ino
  Gère la communication série avec l'écran Nextion sur les pages spécifiques.
*/

#include <SoftwareSerial.h>
#include <NTPClient.h>
extern NTPClient timeClient; // Permet à l'écran de lire l'heure du fichier principal
extern float pressureHistory[3]; // Permet à l'écran de lire l'historique des pressions

// RX = D3, TX = D4 pour la communication avec le Nextion
SoftwareSerial nextion(D3, D4); 

unsigned long dernierUpdateEcran = 0;

// --- Fonctions d'envoi formatées pour Nextion ---
void envoyerNextionTxt(String page, String composant, String texte) {
  nextion.print(page + "." + composant + ".txt=\"");
  nextion.print(texte);
  nextion.print("\"");
  nextion.write(0xFF); nextion.write(0xFF); nextion.write(0xFF);
}

void envoyerNextionVal(String page, String composant, int valeur) {
  nextion.print(page + "." + composant + ".val=");
  nextion.print(valeur);
  nextion.write(0xFF); nextion.write(0xFF); nextion.write(0xFF);
}

void envoyerNextionPic(String page, String composant, int idPic) {
  nextion.print(page + "." + composant + ".pic=");
  nextion.print(idPic);
  nextion.write(0xFF); nextion.write(0xFF); nextion.write(0xFF);
}

// --- Initialisation ---
void setupEcran() {
  nextion.begin(9600);
}

// --- Mise à jour de l'écran (Toutes les 1 seconde) ---
void majEcran() {
  if (millis() - dernierUpdateEcran > 1000) {
    
    // Le symbole degré (°) pur pour éviter le Â sur le Nextion
    String degreC = String(char(176)) + "C";

    // ================= 0. GESTION DE L'HEURE =================
    // Récupère l'heure au format "HH:MM:SS" et coupe pour n'avoir que "HH:MM"
    String heureCourante = timeClient.getFormattedTime().substring(0, 5);
    envoyerNextionTxt("page0", "tHeure", heureCourante);
    envoyerNextionTxt("page1", "tHeure", heureCourante);
    envoyerNextionTxt("page2", "tHeure", heureCourante);
    envoyerNextionTxt("page3", "tHeure", heureCourante);
    envoyerNextionTxt("page4", "tHeure", heureCourante);
    envoyerNextionTxt("page5", "tHeure", heureCourante);
    envoyerNextionTxt("page6", "tHeure", heureCourante);

    // 1. Température Extérieure (page0)
    envoyerNextionTxt("page0", "tTemp", String(extTemp, 1) + degreC);
    int jTempExt = map(extTemp, -20, 50, 0, 100);
    envoyerNextionVal("page0", "j0", constrain(jTempExt, 0, 100));

    // 2. Température Intérieure (page5)
    envoyerNextionTxt("page5", "tTemp", String(lastTemp, 1) + degreC);
    int jTempInt = map(lastTemp, 0, 50, 0, 100);
    envoyerNextionVal("page5", "j0", constrain(jTempInt, 0, 100));

    // 3. Humidité Intérieure (page2)
    envoyerNextionTxt("page2", "tHum", String(lastHumidity, 0) + "%");
    envoyerNextionVal("page2", "j0", (int)lastHumidity);

    // 4. Humidité Extérieure + Pluie (page6)
    envoyerNextionTxt("page6", "tHum", String(extHum, 0) + "%");
    envoyerNextionTxt("page6", "tpluie", String(rainVolume, 1) + " mm");
    envoyerNextionVal("page6", "j0", (int)extHum);
    int idIconePluie = 20; // ID par défaut dans Nextion (Nuage sec)
    
    if (rainVolume > 0.0 && rainVolume <= 1.5) {
        idIconePluie = 21; // Pluie faible
    } else if (rainVolume > 1.5 && rainVolume <= 5.0) {
        idIconePluie = 22; // Pluie modérée
    } else if (rainVolume > 5.0) {
        idIconePluie = 23; // Pluie forte / Déluge
    }
    
    // Assure-toi de remplacer les numéros (12, 13, 14, 15) par les vrais IDs 
    // de tes images dans ton logiciel Nextion Editor !
    envoyerNextionPic("page6", "p2", idIconePluie);
    
    // Envoi du texte d'intensité de la pluie (ex: Averses, Bruine...)
    String rainEcran = getRainText(rainVolume);
    rainEcran.replace("ê", "e");
    rainEcran.replace("è", "e");
    rainEcran.replace("é", "e");
    envoyerNextionTxt("page6", "tIntens", rainEcran);

    // 5. Pression Atmosphérique (page1)
    envoyerNextionTxt("page1", "tPres", String(lastPressure, 0) + "hPa");
    
    float presLimitee = constrain(lastPressure, 900.0, 1100.0);
    int amplitudeAngle = map(presLimitee, 900, 1100, 0, 260);
    int anglePres = (320 + amplitudeAngle) % 360;
    envoyerNextionVal("page1", "z0", anglePres);

    // Prévision météo sans accents
    String premissionMeteo = "Variable"; 
    if (pressureHistory[0] > 0) {
      float deltaPression = pressureHistory[2] - pressureHistory[0]; // Pression actuelle - Pression d'il y a 3h
      if (deltaPression < -1.5) {
        premissionMeteo = "Pluie";
      } else if (deltaPression > 1.5) {
        premissionMeteo = "Beau temps";
      } else {
        premissionMeteo = "Variable";
      }
    }
    envoyerNextionTxt("page1", "tPrev", premissionMeteo);

    // 6. Vent (page3)
    envoyerNextionTxt("page3", "tVent", String(lastWindSpeed, 0) + " km/h");
    // Le capteur donne le Nord à 0°. Le Nextion a son 0° à l'Ouest (à gauche).
    // On ajoute donc 90° dans le sens horaire pour corriger le décalage.
    int angleNextionVent = ((int)lastWindAngle + 90) % 360;
    envoyerNextionVal("page3", "z0", angleNextionVent); 
    
    // Icônes du vent
    int idIconeVent = (lastWindSpeed < 10) ? 6 : (lastWindSpeed < 30 ? 7 : 8);
    envoyerNextionPic("page3", "p1", idIconeVent);

    // CORRECTION : On utilise bien "tBft" pour correspondre à ton design !
    String beaufortEcran = getBeaufortText(lastWindSpeed);
    beaufortEcran.replace("ê", "e");
    beaufortEcran.replace("è", "e");
    beaufortEcran.replace("é", "e");
    envoyerNextionTxt("page3", "tBft", beaufortEcran); 

    // 7. Qualité de l'air / CO2 (page4)
    String airLabel, airDesc;
    if (lastCo2 < 600) { 
    airLabel = "Excellente"; airDesc = "Air bien ventile"; 
  } else if (lastCo2 <= 1000) { 
    airLabel = "Bonne"; airDesc = "Renouvellement acceptable"; 
  } else if (lastCo2 <= 1500) { 
    airLabel = "Moyenne"; airDesc = "Aerer rapidement"; 
  } else if (lastCo2 <= 2000) { 
    airLabel = "Mauvaise"; airDesc = "Sensation de lourdeur"; 
  } else { 
    airLabel = "Critique"; airDesc = "Risque de maux de tete"; 
  }
    
    // Envoi des valeurs désassemblées
    envoyerNextionTxt("page4", "tPpm", String(lastCo2) + "Ppm"); 
    envoyerNextionTxt("page4", "tGaz", airLabel);        
    envoyerNextionTxt("page4", "tDesc", airDesc);
    
    // Remplissage de la jauge avec la fonction Crop du Nextion
    int ratioCo2 = map(lastCo2, 0, 2000, 0, 100); 
    envoyerNextionVal("page4", "j0", constrain(ratioCo2, 0, 100));

    dernierUpdateEcran = millis();
  }
}