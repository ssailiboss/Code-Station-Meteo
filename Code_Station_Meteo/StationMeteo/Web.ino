/*
  Fichier : Web.ino
  Gère l'interface web, le téléchargement CSV, et le formatage des données temporelles.
  Mise à jour : Algorithme de prévision météo basé sur la tendance de la pression.
*/

// ================= HELPERS UTILS =================

String getDirectionTexte(float angle) {
  int index = (int)((angle + 22.5) / 45.0) % 8;
  const char* directions[] = {"Nord", "Nord-Est", "Est", "Sud-Est", "Sud", "Sud-Ouest", "Ouest", "Nord-Ouest"};
  return directions[index];
}

String getBeaufortText(float speed) {
  if (speed < 1.0) return "Calme";
  if (speed <= 19.0) return "Brises";
  if (speed <= 28.0) return "Jolie brise";
  if (speed <= 49.0) return "Vent frais";
  if (speed <= 74.0) return "Coup de vent";
  if (speed <= 102.0) return "Tempête";
  return "Violente tempête";
}

String getRainText(float mm) {
  if (mm < 0.5) return "Bruine";
  if (mm <= 1.5) return "Pluie faible";
  if (mm <= 3.5) return "Pluie modérée";
  if (mm <= 10.0) return "Averses";
  if (mm <= 50.0) return "Pluie intense";
  return "Pluie torrentielle";
}

// --- NOUVEAU : Algorithme de Prévision Météo ---
String getWeatherPrediction(float current, float history) {
  float delta = current - history;
  
  // Tendance forte (Baisse)
  if (delta <= -2.0) return "Dégradation rapide / Orage ⛈️";
  if (delta <= -0.5) {
      if (current < 1015) return "Pluie à venir 🌧️";
      return "Couvert / Dégradation ☁️";
  }
  
  // Tendance forte (Hausse)
  if (delta >= 2.0) return "Amélioration rapide 🌤️";
  if (delta >= 0.5) {
      if (current > 1015) return "Vers le beau temps ☀️";
      return "Éclaircies ⛅";
  }
  
  // Tendance stable (Delta entre -0.5 et 0.5)
  if (current > 1022) return "Beau temps stable ☀️";
  if (current < 1010) return "Dépression / Mauvais 🌧️";
  return "Temps variable ⛅";
}

String getDateString() {
  time_t rawTime = timeClient.getEpochTime();
  struct tm* ti = gmtime(&rawTime);
  char buf[11];
  sprintf(buf, "%04d-%02d-%02d", ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday);
  return String(buf);
}

String getTimeString() {
  time_t rawTime = timeClient.getEpochTime();
  struct tm* ti = gmtime(&rawTime);
  char buf[9];
  sprintf(buf, "%02d:%02d:%02d", ti->tm_hour, ti->tm_min, ti->tm_sec);
  return String(buf);
}

// ================= WEB HANDLERS =================

void handleLogo() {
  if (!sdOk || !SD.exists("/logo.png")) { 
    server.send(404, "text/plain", "Introuvable");
    return; 
  }
  File file = SD.open("/logo.png", FILE_READ);
  server.streamFile(file, "image/png");
  file.close();
}

void handleDownload() {
  if (!sdOk || !SD.exists("data.csv")) { 
    server.send(404, "text/plain", "Fichier introuvable");
    return; 
  }
  File file = SD.open("data.csv", FILE_READ);
  server.sendHeader("Content-Disposition", "attachment; filename=data.csv");
  server.streamFile(file, "text/csv");
  file.close();
}

void handleReset() {
  if (sdOk) {
    SD.remove("data.csv");
    File file = SD.open("data.csv", FILE_WRITE);
    if (file) {
      file.println("Date,Heure,TempIn(C),HumIn(%),CO2(ppm),Pression(hPa),Vent(km/h),Dir(deg),TempExt(C),HumExt(%),Pluie(mm)");
      file.close();
    }
  }
  tempMax = -1000; tempMin = 1000;
  lastTemp = lastHumidity = lastPressure = lastWindSpeed = lastWindAngle = extTemp = extHum = rainVolume = 0;
  lastCo2 = 400;
  isRaining = false;
  
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleData() { 
  String dayFilter = server.hasArg("day") ? server.arg("day") : "";
  if (!sdOk || !SD.exists("data.csv")) { 
    server.send(200, "application/json", "{\"labels\":[],\"tempsIn\":[],\"tempsExt\":[],\"vents\":[],\"pluies\":[],\"days\":[],\"currentDay\":\"\"}"); 
    return;
  }

  File file = SD.open("data.csv", FILE_READ);
  file.readStringUntil('\n');
  
  String allDays = "", prevDay = "", lastDay = "";
  while (file.available()) {
    majEcran(); yield(); 
    String line = file.readStringUntil('\n'); line.trim();
    if (line.length() == 0) continue;
    
    int c1 = line.indexOf(',');
    if (c1 < 0) continue;
    String day = line.substring(0, c1);
    if (day != prevDay) {
      if (allDays.length() > 0) allDays += ",";
      allDays += "\"" + day + "\""; 
      prevDay = day; lastDay = day;
    }
  }
  file.close();

  if (dayFilter == "") dayFilter = lastDay;

  file = SD.open("data.csv", FILE_READ); 
  file.readStringUntil('\n');
  
  String labels = "", tempsIn = "", tempsExt = "", vents = "", pluies = ""; 
  bool first = true;
  
  while (file.available()) {
    majEcran(); yield(); 
    String line = file.readStringUntil('\n'); line.trim();
    if (line.length() == 0) continue;
    
    int c[10];
    c[0] = line.indexOf(',');
    for(int i=1; i<10; i++) c[i] = line.indexOf(',', c[i-1] + 1);
    
    if (c[0] < 0 || c[9] < 0 || line.substring(0, c[0]) != dayFilter) continue;

    if (!first) { labels += ","; tempsIn += ","; tempsExt += ","; vents += ","; pluies += ","; }
    
    labels += "\"" + line.substring(c[0] + 1, c[1]).substring(0, 5) + "\""; 
    tempsIn += line.substring(c[1] + 1, c[2]);                              
    vents += line.substring(c[5] + 1, c[6]); 
    tempsExt += line.substring(c[7] + 1, c[8]);                             
    pluies += line.substring(c[9] + 1); 
    first = false;
  }
  file.close();
  
  server.send(200, "application/json", "{\"labels\":[" + labels + "],\"tempsIn\":[" + tempsIn + "],\"tempsExt\":[" + tempsExt + "],\"vents\":[" + vents + "],\"pluies\":[" + pluies + "],\"days\":[" + allDays + "],\"currentDay\":\"" + dayFilter + "\"}");
}

void handleSummary() { 
  if (!sdOk || !SD.exists("data.csv")) { 
    server.send(200, "application/json", "[]");
    return; 
  }
  
  time_t now = timeClient.getEpochTime();
  struct tm* ti = gmtime(&now);
  int daysFromMonday = (ti->tm_wday == 0) ? 6 : ti->tm_wday - 1;
  String weekDates[7];

  for (int i = 0; i < 7; i++) {
    time_t t = now - ((daysFromMonday - i) * 86400L);
    struct tm* td = gmtime(&t); 
    char buf[11];
    sprintf(buf, "%04d-%02d-%02d", td->tm_year + 1900, td->tm_mon + 1, td->tm_mday);
    weekDates[i] = String(buf);
  }

  float sumTempIn[7]={0}, sumHumIn[7]={0}, minTempIn[7], maxTempIn[7];
  float sumTempExt[7]={0}, sumHumExt[7]={0}, minTempExt[7], maxTempExt[7];
  int count[7] = {0};

  for (int i = 0; i < 7; i++) { 
    minTempIn[i] = 1000.0; maxTempIn[i] = -1000.0;
    minTempExt[i] = 1000.0; maxTempExt[i] = -1000.0;
  }

  File file = SD.open("data.csv", FILE_READ); 
  file.readStringUntil('\n');
  
  while (file.available()) {
    majEcran(); yield(); 
    String line = file.readStringUntil('\n'); line.trim();
    if (line.length() == 0) continue;
    
    int c[10];
    c[0] = line.indexOf(',');
    for(int i=1; i<10; i++) c[i] = line.indexOf(',', c[i-1] + 1);
    if (c[0] < 0 || c[9] < 0) continue;
    
    String day = line.substring(0, c[0]);
    float tIn  = line.substring(c[1] + 1, c[2]).toFloat();
    float hIn  = line.substring(c[2] + 1, c[3]).toFloat();
    float tExt = line.substring(c[7] + 1, c[8]).toFloat();
    float hExt = line.substring(c[8] + 1, c[9]).toFloat();

    for (int i = 0; i < 7; i++) {
      if (day == weekDates[i]) {
        sumTempIn[i] += tIn; sumHumIn[i] += hIn;
        sumTempExt[i] += tExt; sumHumExt[i] += hExt;
        
        if (tIn < minTempIn[i]) minTempIn[i] = tIn;
        if (tIn > maxTempIn[i]) maxTempIn[i] = tIn;
        if (tExt < minTempExt[i]) minTempExt[i] = tExt;
        if (tExt > maxTempExt[i]) maxTempExt[i] = tExt;
        
        count[i]++; 
        break;
      }
    }
  }
  file.close();

  const char* dayNames[7] = {"Lundi","Mardi","Mercredi","Jeudi","Vendredi","Samedi","Dimanche"};
  String json = "[";
  for (int i = 0; i < 7; i++) {
    if (i > 0) json += ",";
    json += "{\"day\":\"" + String(dayNames[i]) + "\",\"date\":\"" + weekDates[i] + "\",";
    if (count[i] > 0) {
      json += "\"avgTempIn\":" + String(sumTempIn[i]/count[i], 1) + ",\"avgHumIn\":" + String(sumHumIn[i]/count[i], 1) + ",";
      json += "\"minTempIn\":" + String(minTempIn[i], 1) + ",\"maxTempIn\":" + String(maxTempIn[i], 1) + ",";
      json += "\"avgTempExt\":" + String(sumTempExt[i]/count[i], 1) + ",\"avgHumExt\":" + String(sumHumExt[i]/count[i], 1) + ",";
      json += "\"minTempExt\":" + String(minTempExt[i], 1) + ",\"maxTempExt\":" + String(maxTempExt[i], 1) + ",\"hasData\":true}";
    } else {
      json += "\"hasData\":false}";
    }
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleRoot() {
  
  String airLabel, airColor, airDesc;
  if (lastCo2 < 600) { 
    airLabel = "Très bonne"; airColor = "#2ecc71"; airDesc = "Air bien ventilé"; 
  } else if (lastCo2 <= 1000) { 
    airLabel = "Bonne"; airColor = "#27ae60"; airDesc = "Renouvellement acceptable"; 
  } else if (lastCo2 <= 1500) { 
    airLabel = "Vigilance"; airColor = "#f1c40f"; airDesc = "Aérer rapidement"; 
  } else if (lastCo2 <= 2000) { 
    airLabel = "Mauvaise"; airColor = "#e67e22"; airDesc = "Sensation de lourdeur"; 
  } else { 
    airLabel = "Très mauvaise"; airColor = "#e74c3c"; airDesc = "Risque de maux de tête"; 
  }

  float ratio = (float)lastCo2 / 2000.0 * 100.0;
  if (ratio > 100) ratio = 100;

  String rainStatus = isRaining ? getRainText(rainVolume) + " 🌧️" : "Sec ☀️";
  String rainColor  = isRaining ? "#e74c3c" : "#3498db";
  
  // --- NOUVEAU : Préparation de la prévision et de la flèche de tendance ---
  String previTexte = getWeatherPrediction(lastPressure, pressureHistory[2]);
  float delta3h = lastPressure - pressureHistory[2];
  String arrow = (delta3h > 0.5) ? "↗" : ((delta3h < -0.5) ? "↘" : "→");
  
  // Si la carte vient de démarrer et n'a pas encore 3 heures d'historique
  if (pressureHistory[2] == lastPressure) {
    arrow = "⏳";
    previTexte = "Acquisition en cours...";
  }

  String currentDate = getDateString();
  String currentTime = getTimeString();

  String html = F(R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta charset='UTF-8'>
  <meta http-equiv='refresh' content='35'>
  <title>Station Météo Mons</title>
  <link rel='stylesheet' href='https://unpkg.com/leaflet/dist/leaflet.css'/>
  <script src='https://unpkg.com/leaflet/dist/leaflet.js'></script>
  <script src='https://cdn.jsdelivr.net/npm/chart.js'></script>
  <style>
    body{font-family:Arial;margin:0;padding:0;text-align:center;background:#f4f4f4}
    header{background:#333;color:white;display:flex;align-items:center;justify-content:space-between;padding:10px 30px}
    header img{height:60px}
    button{padding:10px 20px;font-size:16px;margin:10px;cursor:pointer;border:none;border-radius:6px;transition:0.2s;}
    button:hover{opacity:0.8;}
    #map-container{display:flex;align-items:flex-start;margin-top:20px;justify-content:center}
    #map-col{display:flex;flex-direction:column}
    #map-title{font-size:24px;font-weight:bold;margin-bottom:5px;}
    #map-maxmin{font-size:18px;margin-bottom:10px}
    #map-datetime{font-size:14px;color:#666;margin-bottom:6px}
    #map{width:400px;height:400px;border:2px solid #333;border-radius:8px}
    #info-right{display:flex;flex-direction:row;margin-left:20px;align-items:flex-start}
    #cards-col{display:flex;flex-direction:column}
    
    #top-cards{display:flex;flex-direction:row;flex-wrap:wrap;gap:15px;justify-content:center;max-width:800px;}
    .card{background:white;border-radius:12px;padding:15px;box-shadow:0 2px 6px rgba(0,0,0,0.2);text-align:center;flex:1 1 160px;display:flex;flex-direction:column;justify-content:center;align-items:center;}
    
    .clickable{cursor:pointer; transition:transform 0.2s, box-shadow 0.2s; user-select:none;}
    .clickable:hover{transform:translateY(-3px); box-shadow:0 6px 12px rgba(0,0,0,0.3);}
    
    #air-quality-card{width:100%;height:auto;display:flex;flex-direction:column;justify-content:center;align-items:center;margin-top:15px;padding:15px 20px;box-sizing:border-box;}
    #chart-card{background:white;border-radius:12px;padding:15px;box-shadow:0 2px 6px rgba(0,0,0,0.2);width:100%;height:450px;margin-top:20px;display:flex;flex-direction:column;box-sizing:border-box;}
    
    .air-bar-bg{width:100%;height:16px;background:#e0e0e0;border-radius:8px;overflow:hidden;margin:8px 0 4px 0}
    .air-bar-fill{height:100%;border-radius:8px;transition:width 0.5s}
    .air-labels{display:flex;justify-content:space-between;font-size:11px;color:#999;width:100%}
    select#daySelect{padding:4px 8px;border-radius:6px;border:1px solid #ccc;font-size:14px}
    footer{background:#333;height:80px;margin-top:30px}
  </style>
</head>
<body>
)=====");

  html += "<header>";
  if (sdOk && SD.exists("/logo.png")) html += "<img src='/logo.png' alt='Logo'>";
  html += "<h1>Station Météo Mons</h1>";
  html += "<span style='font-size:14px;color:#ccc;'>" + currentDate + " – " + currentTime + "</span></header>";
  
  html += F("<p style='text-align:center;'><a href='/download'><button style='background:#27ae60;color:white;'>Télécharger CSV</button></a>");
  html += F("<button onclick='openSummary()' style='background:#3498db;color:white;'>Résumé de la semaine</button>");
  html += F("<a href='/reset' onclick=\"return confirm('Effacer toutes les données ?');\"><button style='background:#e74c3c;color:white;'>RESET DONNÉES</button></a></p>");

  html += F("<div id='map-container'><div id='map-col'>");
  html += "<div id='map-title'>Mons</div>";
  html += "<div id='map-maxmin'>Max ↑ " + String(tempMax, 1) + "°C &nbsp;/&nbsp; Min ↓ " + String(tempMin, 1) + "°C</div>";
  html += "<div id='map-datetime'>" + currentDate + " à " + currentTime.substring(0, 5) + "</div>";
  html += F("<div id='map'></div></div><div id='info-right'><div id='cards-col'><div id='top-cards'>");

  html += "<div class='card clickable' onclick='toggleTemp()'><h3 id='t-label' style='margin:0 0 8px 0;color:#e74c3c;'>Température (Int) 🔄</h3><p id='t-val' style='font-size:42px;font-weight:normal;margin:0;'>" + String(lastTemp, 1) + "°C</p></div>";
  html += "<div class='card clickable' onclick='toggleHum()'><h3 id='h-label' style='margin:0 0 8px 0;color:#3498db;'>Humidité (Int) 🔄</h3><p id='h-val' style='font-size:42px;font-weight:normal;margin:0;'>" + String(lastHumidity, 1) + "%</p></div>";
  
  // --- CARTE PRESSION MISE À JOUR AVEC PRÉVISION ---
  html += "<div class='card'><h3 style='margin:0 0 4px 0;'>Pression</h3><canvas id='pressureGauge' width='170' height='120'></canvas><p style='margin:2px 0 0 0;font-size:15px;font-weight:bold;'>" + String(lastPressure, 1) + " hPa " + arrow + "</p><p style='font-size:13px;color:#8e44ad;font-weight:bold;margin:5px 0 0 0;'>" + previTexte + "</p></div>";
  
  html += "<div class='card clickable' onclick='openDataModal(\"vent\")'><h3 style='margin:0 0 8px 0;color:#f39c12;'>Vent (Ext) 📈</h3><p style='font-size:32px;font-weight:bold;color:#f39c12;margin:0;'>" + String(lastWindSpeed, 1) + " km/h</p><p style='font-size:16px;margin:2px 0 0 0;'>" + getDirectionTexte(lastWindAngle) + " (" + String(lastWindAngle, 0) + "°)</p><p style='font-size:14px;color:#7f8c8d;font-style:italic;margin:5px 0 0 0;'>" + getBeaufortText(lastWindSpeed) + "</p></div>";
  html += "<div class='card clickable' onclick='openDataModal(\"pluie\")'><h3 style='margin:0 0 8px 0;color:#3498db;'>Pluie (Ext) 📈</h3><p style='font-size:32px;font-weight:bold;color:" + rainColor + ";margin:0;'>" + String(rainVolume, 1) + " mm</p><p style='font-size:15px;color:#7f8c8d;font-style:italic;margin:5px 0 0 0;'>" + rainStatus + "</p></div>";
  html += "</div>"; 

  html += "<div class='card' id='air-quality-card'><h3 style='margin:0 0 2px 0;'>Qualité de l'air (CO2)</h3><p style='margin:0;font-size:20px;font-weight:bold;color:" + airColor + ";'>" + airLabel + "</p>";
  html += "<div class='air-bar-bg'><div class='air-bar-fill' style='width:" + String(ratio, 0) + "%;background:" + airColor + ";'></div></div>";
  html += "<div class='air-labels'><span>400</span><span>1000</span><span>1500</span><span>2000+</span></div><p style='margin:8px 0 0 0;font-size:14px;color:#555;'>" + airDesc + " (" + String(lastCo2) + " ppm)</p></div>";
  
  html += F(R"=====(
    <div id='chart-card'>
      <div style='display:flex; justify-content:space-between; align-items:center; margin-bottom:10px;'>
        <h3 id='chart-title' style='margin:0;color:#e74c3c;'>Graphique (Intérieur)</h3>
        <div>
          <select id='daySelect' onchange='renderChart()'><option>Chargement...</option></select>
          <button id='chartToggleBtn' onclick='toggleChartMode()' style='background:#2c3e50;color:white;padding:5px 10px;margin-left:10px;'>Voir Extérieur 🔄</button>
        </div>
      </div>
      <div style='flex:1;position:relative;'><canvas id='tempChart' style='position:absolute;top:0;left:0;width:100%;height:100%;'></canvas></div>
    </div>
  </div></div>

  <div id='dataModal' style='display:none;position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,0.5);z-index:9999;justify-content:center;align-items:center;'>
    <div style='background:white;border-radius:16px;padding:30px;max-width:800px;width:90%;height:60vh;position:relative;display:flex;flex-direction:column;'>
      <button onclick='closeDataModal()' style='position:absolute;top:12px;right:16px;background:none;border:none;font-size:22px;cursor:pointer;color:black;'>✕</button>
      <h2 id='dataModalTitle' style='margin-top:0;'>Évolution</h2>
      <div style='flex:1;position:relative;'>
        <canvas id='dataModalChart' style='position:absolute;top:0;left:0;width:100%;height:100%;'></canvas>
      </div>
    </div>
  </div>

  <div id='summaryModal' style='display:none;position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,0.5);z-index:9999;justify-content:center;align-items:center;'>
    <div style='background:white;border-radius:16px;padding:30px;max-width:960px;width:90%;max-height:85vh;overflow-y:auto;position:relative;'>
      <button onclick='closeSummary()' style='position:absolute;top:12px;right:16px;background:none;border:none;font-size:22px;cursor:pointer;color:black;'>✕</button>
      
      <div style='display:flex; justify-content:space-between; align-items:center; margin-bottom:20px; margin-right:40px;'>
        <h2 id='summary-title' style='margin:0;color:#e74c3c;'>Résumé de la semaine (Intérieur)</h2>
        <button id='summaryToggleBtn' onclick='toggleSummaryMode()' style='background:#2c3e50;color:white;padding:8px 15px;'>Voir Extérieur 🔄</button>
      </div>

      <div id='summaryCards' style='display:flex;flex-wrap:nowrap;gap:8px;justify-content:center;'><p>Chargement...</p></div>
      <div style='margin-top:20px;background:#f9f9f9;border-radius:12px;padding:15px;'><canvas id='weekChart' height='80'></canvas></div>
    </div>
  </div>

  <script>
    var map=L.map('map').setView([50.4517,3.9591],12);
    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png',{attribution:'&copy; OpenStreetMap'}).addTo(map);
    var marker=L.marker([50.4517,3.9591]).addTo(map);
  )=====");
  
  html += "marker.bindPopup('<b>Station Mons</b><br>Max ↑ " + String(tempMax, 1) + "°C / Min ↓ " + String(tempMin, 1) + "°C').openPopup();";

  html += "var tempInt = " + String(lastTemp, 1) + "; var tempExt = " + String(extTemp, 1) + ";";
  html += "var humInt = " + String(lastHumidity, 1) + "; var humExt = " + String(extHum, 1) + ";";

  html += F(R"=====(
    (function(){
      var ctx2=document.getElementById('pressureGauge').getContext('2d');
      var val = )====="); html += String(lastPressure, 1); html += F(R"=====(;
      var minP=900,maxP=1100,ratio=Math.min(Math.max((val-minP)/(maxP-minP),0),1);
      var startAngle=Math.PI*0.8,endAngle=Math.PI*2.2,cx=80,cy=80,r=60;
      ctx2.beginPath();ctx2.arc(cx,cy,r,startAngle,endAngle);ctx2.strokeStyle='#e0e0e0';ctx2.lineWidth=12;ctx2.lineCap='round';ctx2.stroke();
      var grad=ctx2.createLinearGradient(cx-r,0,cx+r,0);grad.addColorStop(0,'#3498db');grad.addColorStop(1,'#e74c3c');
      ctx2.beginPath();ctx2.arc(cx,cy,r,startAngle,startAngle+ratio*(endAngle-startAngle));ctx2.strokeStyle=grad;ctx2.lineWidth=12;ctx2.lineCap='round';ctx2.stroke();
      ctx2.fillStyle='#999';ctx2.font='11px Arial';ctx2.fillText('900',2,120);ctx2.fillText('1100',140,120);
    })();
  
    var showIntTemp = true;
    function toggleTemp() {
      showIntTemp = !showIntTemp;
      document.getElementById('t-label').innerText = showIntTemp ? 'Température (Int) 🔄' : 'Température (Ext) 🔄';
      document.getElementById('t-label').style.color = showIntTemp ? '#e74c3c' : '#e67e22';
      document.getElementById('t-val').innerText = (showIntTemp ? tempInt : tempExt).toFixed(1) + '°C';
    }

    var showIntHum = true;
    function toggleHum() {
      showIntHum = !showIntHum;
      document.getElementById('h-label').innerText = showIntHum ? 'Humidité (Int) 🔄' : 'Humidité (Ext) 🔄';
      document.getElementById('h-label').style.color = showIntHum ? '#3498db' : '#2980b9';
      document.getElementById('h-val').innerText = (showIntHum ? humInt : humExt).toFixed(1) + '%';
    }

    var currentChart=null, currentDayData=null, chartMode='int';

    function toggleChartMode() {
      chartMode = (chartMode === 'int') ? 'ext' : 'int';
      document.getElementById('chartToggleBtn').innerText = (chartMode === 'int') ? 'Voir Extérieur 🔄' : 'Voir Intérieur 🔄';
      document.getElementById('chart-title').innerText = (chartMode === 'int') ? 'Graphique (Intérieur)' : 'Graphique (Extérieur)';
      document.getElementById('chart-title').style.color = (chartMode === 'int') ? '#e74c3c' : '#e67e22';
      renderChart();
    }

    function renderChart() {
      var day = document.getElementById('daySelect').value;
      if (!day || day === "Chargement...") return;

      fetch('/data?day=' + encodeURIComponent(day)).then(r=>r.json()).then(d => {
        currentDayData = d; 
        if(currentChart) currentChart.destroy();
        
        var dsData = (chartMode === 'int') ? d.tempsIn : d.tempsExt;
        var col = (chartMode === 'int') ? '#e74c3c' : '#e67e22';
        var bgCol = (chartMode === 'int') ? 'rgba(231,76,60,0.1)' : 'rgba(230,126,34,0.1)';

        currentChart=new Chart(document.getElementById('tempChart').getContext('2d'),{
          type:'line',
          data: {
            labels:d.labels,
            datasets:[{label:'Température (°C)', data:dsData.map(v=>parseFloat(parseFloat(v).toFixed(1))), borderColor:col, backgroundColor:bgCol, borderWidth:2, pointRadius:3, fill:true, tension:0.3}]
          },
          options:{responsive:true,maintainAspectRatio:false,layout:{padding:{bottom:10}},plugins:{legend:{display:false}},scales:{x:{ticks:{maxTicksLimit:8}},y:{min:-10,max:40,ticks:{callback:v=>v+'°C'}}}}
        });
      });
    }

    fetch('/data').then(r=>r.json()).then(d=>{
      var sel=document.getElementById('daySelect'); sel.innerHTML='';
      if(d.days.length===0){ sel.innerHTML='<option>Aucune donnée</option>'; return; }
      d.days.slice().reverse().forEach(day=>{
        var opt=document.createElement('option'); opt.value=day; opt.text=day;
        if(day===d.currentDay) opt.selected=true;
        sel.appendChild(opt);
      });
      renderChart();
    });

    var dataModalChart = null;
    function openDataModal(type) {
      if(!currentDayData) return;
      document.getElementById('dataModal').style.display='flex';
      var ctx = document.getElementById('dataModalChart').getContext('2d');
      if(dataModalChart) dataModalChart.destroy();

      var label, dataArr, color, bgColor, ycb;
      if(type === 'vent') {
          document.getElementById('dataModalTitle').innerText = 'Évolution du Vent (' + currentDayData.currentDay + ')';
          label = 'Vitesse (km/h)';
          dataArr = currentDayData.vents;
          color = '#f39c12';
          bgColor = 'rgba(243, 156, 18, 0.2)';
          ycb = v => v + ' km/h';
      } else if(type === 'pluie') {
          document.getElementById('dataModalTitle').innerText = 'Évolution de la Pluie (' + currentDayData.currentDay + ')';
          label = 'Pluie cumulée (mm)';
          dataArr = currentDayData.pluies;
          color = '#3498db';
          bgColor = 'rgba(52, 152, 219, 0.2)';
          ycb = v => v + ' mm';
      }

      dataModalChart = new Chart(ctx, {
          type: 'line',
          data: {
              labels: currentDayData.labels,
              datasets: [{
                  label: label,
                  data: dataArr.map(v => parseFloat(v)),
                  borderColor: color,
                  backgroundColor: bgColor,
                  borderWidth: 2,
                  pointRadius: 2,
                  fill: true,
                  tension: 0.3
              }]
          },
          options: {
              responsive: true,
              maintainAspectRatio: false,
              plugins:{legend:{display:false}},
              scales: {
                  x: {ticks: {maxTicksLimit: 8}},
                  y: {beginAtZero: true, ticks: {callback: ycb}}
              }
          }
      });
    }
    
    function closeDataModal() { document.getElementById('dataModal').style.display='none'; }

    var weekChart=null, summaryData=null, summaryMode='int';

    function toggleSummaryMode() {
      summaryMode = (summaryMode === 'int') ? 'ext' : 'int';
      document.getElementById('summaryToggleBtn').innerText = (summaryMode === 'int') ? 'Voir Extérieur 🔄' : 'Voir Intérieur 🔄';
      document.getElementById('summary-title').innerText = (summaryMode === 'int') ? 'Résumé de la semaine (Intérieur)' : 'Résumé de la semaine (Extérieur)';
      document.getElementById('summary-title').style.color = (summaryMode === 'int') ? '#e74c3c' : '#e67e22';
      renderSummary();
    }

    function openSummary(){
      document.getElementById('summaryModal').style.display='flex';
      if(!summaryData) {
        fetch('/summary').then(r=>r.json()).then(d=>{ summaryData=d; renderSummary(); });
      } else {
        renderSummary();
      }
    }

    function renderSummary() {
      if(!summaryData) return;
      var h='';
      var col = (summaryMode === 'int') ? '#e74c3c' : '#e67e22';
      
      summaryData.forEach(d=>{
        var bg=d.hasData?'#fff':'#f0f0f0', border=d.hasData?'#3498db':'#ccc';
        var avgT = (summaryMode === 'int') ? d.avgTempIn : d.avgTempExt;
        var minT = (summaryMode === 'int') ? d.minTempIn : d.minTempExt;
        var maxT = (summaryMode === 'int') ? d.maxTempIn : d.maxTempExt;
        var avgH = (summaryMode === 'int') ? d.avgHumIn : d.avgHumExt;

        h+=`<div style='background:${bg};border:2px solid ${border};border-radius:12px;padding:12px;flex:1;min-width:0;text-align:center;'>`;
        h+=`<div style='font-weight:bold;font-size:15px;'>${d.day}</div><div style='font-size:11px;color:#999;margin-bottom:8px;'>${d.date}</div>`;
        if(d.hasData) {
          h+=`<div style='font-size:22px;font-weight:bold;color:${col};'>${avgT}°C</div>`;
          h+=`<div style='font-size:11px;color:${col};margin-bottom:6px;'>↑${maxT}° ↓${minT}°</div>`;
          h+=`<div style='font-size:13px;color:#3498db;'>Hum. ${avgH}%</div>`;
        } else {
          h+=`<div style='color:#aaa;font-size:12px;margin-top:16px;'>Pas de<br>données</div>`;
        }
        h+='</div>';
      });
      document.getElementById('summaryCards').innerHTML=h;
      
      if(weekChart) weekChart.destroy();
      weekChart=new Chart(document.getElementById('weekChart').getContext('2d'),{
        type:'line',
        data:{
          labels: summaryData.map(d=>d.day),
          datasets:[
          {label:'Moyenne',data:summaryData.map(d=>d.hasData ? (summaryMode==='int'?d.avgTempIn:d.avgTempExt) : null), borderColor:col, backgroundColor:col, borderWidth:3, pointRadius:5, fill:false, tension:0.4},
          {label:'Max',data:summaryData.map(d=>d.hasData ? (summaryMode==='int'?d.maxTempIn:d.maxTempExt) : null), borderColor:'#f39c12', borderWidth:1.5, borderDash:[5,4], pointRadius:3, fill:false, tension:0.4},
          {label:'Min',data:summaryData.map(d=>d.hasData ? (summaryMode==='int'?d.minTempIn:d.minTempExt) : null), borderColor:'#3498db', borderWidth:1.5, borderDash:[5,4], pointRadius:3, fill:false, tension:0.4}
        ]},
        options:{responsive:true,maintainAspectRatio:true,plugins:{legend:{display:true,position:'top'}},scales:{y:{ticks:{callback:v=>v+'°C'}}}}
      });
    }
    
    function closeSummary(){ document.getElementById('summaryModal').style.display='none'; }
    
    window.addEventListener('click',e=>{ 
      if(e.target===document.getElementById('summaryModal')) closeSummary(); 
      if(e.target===document.getElementById('dataModal')) closeDataModal(); 
    });
  </script>
  <footer></footer>
</body>
</html>
  )=====");

  server.send(200, "text/html", html);
}
