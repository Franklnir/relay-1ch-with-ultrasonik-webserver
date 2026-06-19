#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

// =====================================================
// SMART LAMP ESP8266 + HC-SR04 + RELAY
// =====================================================
// Pin sensor ultrasonik HC-SR04:
// TRIG HC-SR04  -> D7 ESP8266
// ECHO HC-SR04  -> D6 ESP8266
// VCC HC-SR04   -> VIN / 5V
// GND HC-SR04   -> GND
//
// Pin relay:
// IN Relay      -> D1 ESP8266
// VCC Relay     -> VIN / 5V
// GND Relay     -> GND
//
// WiFi:
// Nama WiFi     -> SmartLamp
// Alamat Web    -> http://192.168.4.1
// =====================================================

#define TRIG  D7
#define ECHO  D6
#define RELAY D1

// Jika relay kamu terbalik, ubah bagian ini:
// Normal: HIGH = ON, LOW = OFF
#define RELAY_ON  HIGH
#define RELAY_OFF LOW

ESP8266WebServer server(80);

// =====================================================
// VARIABEL UTAMA
// =====================================================
bool relayState = false;

// mode:
// 0 = Manual
// 1 = Sensor
int mode = 0;

// Jarak pemicu default 10 cm
int triggerDistance = 10;

// sensorBehaviour:
// 0 = AUTO
//     Lampu ON jika objek lebih dekat dari triggerDistance
//
// 1 = OBJECT PASS
//     Lampu ON jika objek lebih jauh dari triggerDistance
int sensorBehaviour = 0;

float distance = 0;

// Timer pembacaan sensor
unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 150;

// =====================================================
// HALAMAN WEB
// =====================================================
const char webpage[] PROGMEM = R"====(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">

<style>
body{
  font-family:Arial, sans-serif;
  background:#f5f6fa;
  text-align:center;
  margin:0;
  padding:0;
}

.card{
  background:white;
  max-width:420px;
  margin:auto;
  margin-top:20px;
  padding:20px;
  border-radius:14px;
  box-shadow:0 3px 12px rgba(0,0,0,0.15);
}

h2{
  margin-top:0;
}

.info{
  background:#f1f2f6;
  padding:12px;
  border-radius:10px;
  margin-bottom:12px;
  text-align:left;
}

.info p{
  margin:8px 0;
}

button{
  width:100%;
  padding:16px;
  font-size:18px;
  border:none;
  border-radius:10px;
  margin-top:10px;
  cursor:pointer;
}

input{
  width:100%;
  padding:14px;
  font-size:18px;
  box-sizing:border-box;
  border:1px solid #ccc;
  border-radius:10px;
  text-align:center;
}

.on{
  background:#2ecc71;
  color:white;
}

.off{
  background:#e74c3c;
  color:white;
}

.manual{
  background:#f1c40f;
  color:black;
}

.sensor{
  background:#3498db;
  color:white;
}

.save{
  background:#2c3e50;
  color:white;
}

.auto{
  background:#16a085;
  color:white;
}

.pass{
  background:#8e44ad;
  color:white;
}

.small{
  font-size:13px;
  color:#555;
  margin-top:15px;
}
</style>
</head>

<body>

<div class="card">

<h2>SMART LAMP</h2>

<div class="info">
  <p>Distance : <b><span id="dist">0</span> cm</b></p>
  <p>Relay : <b><span id="relay">OFF</span></b></p>
  <p>Mode : <b><span id="mode">MANUAL</span></b></p>
  <p>Trigger : <b><span id="trigger">10</span> cm</b></p>
  <p>Behaviour : <b><span id="behaviour">AUTO</span></b></p>
</div>

<button id="relayBtn">Lamp OFF</button>
<button id="modeBtn">Manual Mode</button>

<div id="sensorSettings" style="display:none">

<br>

<p><b>Trigger Distance</b></p>
<input id="distInput" type="number" min="2" max="200" placeholder="Masukkan jarak cm">

<button class="save" onclick="saveDistance()">Save Distance</button>

<br><br>

<p><b>Sensor Behaviour</b></p>
<button class="auto" onclick="setBehaviour(0)">AUTO</button>
<button class="pass" onclick="setBehaviour(1)">OBJECT PASS</button>

</div>

<p class="small">
Sambungkan HP/Laptop ke WiFi <b>SmartLamp</b>, lalu buka browser:
<br>
<b>192.168.4.1</b>
</p>

</div>

<script>
let relay = false;
let mode = "MANUAL";

function toggleRelay(){
  if(mode == "SENSOR"){
    alert("Matikan Sensor Mode dulu jika ingin kontrol manual.");
    return;
  }

  fetch(relay ? "/off" : "/on");
}

function toggleMode(){
  fetch(mode == "MANUAL" ? "/sensor" : "/manual");
}

function saveDistance(){
  let v = document.getElementById("distInput").value;

  if(v == ""){
    alert("Masukkan jarak terlebih dahulu.");
    return;
  }

  fetch("/set?d=" + v)
  .then(() => {
    alert("Trigger distance disimpan: " + v + " cm");
  });
}

function setBehaviour(v){
  fetch("/behaviour?b=" + v);
}

document.getElementById("relayBtn").onclick = toggleRelay;
document.getElementById("modeBtn").onclick = toggleMode;

setInterval(() => {
  fetch("/data")
  .then(r => r.json())
  .then(d => {

    document.getElementById("dist").innerHTML = d.distance;
    document.getElementById("relay").innerHTML = d.relay;
    document.getElementById("mode").innerHTML = d.mode;
    document.getElementById("trigger").innerHTML = d.trigger;
    document.getElementById("behaviour").innerHTML = d.behaviourText;

    relay = d.relay == "ON";
    mode = d.mode;

    let rbtn = document.getElementById("relayBtn");
    let mbtn = document.getElementById("modeBtn");

    if(relay){
      rbtn.className = "on";
      rbtn.innerHTML = "Lamp ON";
    }else{
      rbtn.className = "off";
      rbtn.innerHTML = "Lamp OFF";
    }

    if(mode == "MANUAL"){
      mbtn.className = "manual";
      mbtn.innerHTML = "Manual Mode";
      document.getElementById("sensorSettings").style.display = "none";
    }else{
      mbtn.className = "sensor";
      mbtn.innerHTML = "Sensor Mode";
      document.getElementById("sensorSettings").style.display = "block";
    }

  });
}, 800);
</script>

</body>
</html>
)====";

// =====================================================
// FUNGSI RELAY
// =====================================================
void setRelay(bool state) {
  relayState = state;

  if (relayState) {
    digitalWrite(RELAY, RELAY_ON);
  } else {
    digitalWrite(RELAY, RELAY_OFF);
  }
}

// =====================================================
// SIMPAN DATA KE EEPROM
// =====================================================
void saveData() {
  EEPROM.write(0, relayState ? 1 : 0);
  EEPROM.write(1, mode);
  EEPROM.write(2, triggerDistance);
  EEPROM.write(3, sensorBehaviour);
  EEPROM.commit();
}

// =====================================================
// LOAD DATA DARI EEPROM
// =====================================================
void loadData() {
  relayState = EEPROM.read(0);
  mode = EEPROM.read(1);
  triggerDistance = EEPROM.read(2);
  sensorBehaviour = EEPROM.read(3);

  if (relayState != 0 && relayState != 1) {
    relayState = false;
  }

  if (mode < 0 || mode > 1) {
    mode = 0;
  }

  if (triggerDistance < 2 || triggerDistance > 200) {
    triggerDistance = 10;
  }

  if (sensorBehaviour < 0 || sensorBehaviour > 1) {
    sensorBehaviour = 0;
  }
}

// =====================================================
// BACA JARAK SENSOR ULTRASONIK
// =====================================================
float readDistance() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH, 30000);

  if (duration == 0) {
    return distance;
  }

  float d = duration * 0.034 / 2;

  if (d <= 0 || d > 400) {
    return distance;
  }

  return d;
}

// =====================================================
// HALAMAN UTAMA
// =====================================================
void handleRoot() {
  server.send_P(200, "text/html", webpage);
}

// =====================================================
// DATA JSON UNTUK WEB
// =====================================================
void handleData() {
  String behaviourText = sensorBehaviour == 0 ? "AUTO" : "OBJECT PASS";

  String json = "{";
  json += "\"distance\":";
  json += String(distance, 1);
  json += ",\"relay\":\"";
  json += relayState ? "ON" : "OFF";
  json += "\",\"mode\":\"";
  json += mode == 0 ? "MANUAL" : "SENSOR";
  json += "\",\"trigger\":";
  json += triggerDistance;
  json += ",\"behaviour\":";
  json += sensorBehaviour;
  json += ",\"behaviourText\":\"";
  json += behaviourText;
  json += "\"}";

  server.send(200, "application/json", json);
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("==================================");
  Serial.println("SMART LAMP ESP8266 STARTING...");
  Serial.println("==================================");

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(RELAY, OUTPUT);

  EEPROM.begin(512);
  loadData();

  setRelay(relayState);

  // IP Access Point
  IPAddress IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IP, gateway, subnet);
  WiFi.softAP("SmartLamp");

  Serial.println("[WiFi] Access Point aktif");
  Serial.println("[WiFi] SSID  : SmartLamp");
  Serial.println("[WiFi] IP    : 192.168.4.1");
  Serial.println("[WEB]  Buka  : http://192.168.4.1");

  server.on("/", handleRoot);
  server.on("/data", handleData);

  server.on("/on", []() {
    if (mode == 0) {
      setRelay(true);
      saveData();
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/off", []() {
    if (mode == 0) {
      setRelay(false);
      saveData();
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/manual", []() {
    mode = 0;
    saveData();
    server.send(200, "text/plain", "OK");
  });

  server.on("/sensor", []() {
    mode = 1;
    saveData();
    server.send(200, "text/plain", "OK");
  });

  server.on("/set", []() {
    if (server.hasArg("d")) {
      triggerDistance = server.arg("d").toInt();

      if (triggerDistance < 2) {
        triggerDistance = 2;
      }

      if (triggerDistance > 200) {
        triggerDistance = 200;
      }

      saveData();

      Serial.print("[SET] Trigger Distance: ");
      Serial.print(triggerDistance);
      Serial.println(" cm");
    }

    server.send(200, "text/plain", "OK");
  });

  server.on("/behaviour", []() {
    if (server.hasArg("b")) {
      sensorBehaviour = server.arg("b").toInt();

      if (sensorBehaviour < 0 || sensorBehaviour > 1) {
        sensorBehaviour = 0;
      }

      saveData();

      Serial.print("[SET] Sensor Behaviour: ");
      Serial.println(sensorBehaviour == 0 ? "AUTO" : "OBJECT PASS");
    }

    server.send(200, "text/plain", "OK");
  });

  server.begin();

  Serial.println("[WEB] Server aktif");
  Serial.println("==================================");
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  server.handleClient();

  unsigned long now = millis();

  if (now - lastSensorRead >= sensorInterval) {
    lastSensorRead = now;

    distance = readDistance();

    if (mode == 1) {
      if (sensorBehaviour == 0) {
        // AUTO:
        // Jika objek lebih dekat dari triggerDistance, lampu ON
        setRelay(distance < triggerDistance);
      } else {
        // OBJECT PASS:
        // Jika objek lebih jauh dari triggerDistance, lampu ON
        setRelay(distance > triggerDistance);
      }
    }

    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.print(" cm | Relay: ");
    Serial.print(relayState ? "ON" : "OFF");
    Serial.print(" | Mode: ");
    Serial.print(mode == 0 ? "MANUAL" : "SENSOR");
    Serial.print(" | Trigger: ");
    Serial.print(triggerDistance);
    Serial.print(" cm | Behaviour: ");
    Serial.println(sensorBehaviour == 0 ? "AUTO" : "OBJECT PASS");
  }
}
