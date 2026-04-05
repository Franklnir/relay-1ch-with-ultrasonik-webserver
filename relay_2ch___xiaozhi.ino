#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

#define TRIG D7
#define ECHO D6
#define RELAY D1

ESP8266WebServer server(80);

bool relayState=false;
int mode=0;
int triggerDistance=10;
int sensorBehaviour=0;

float distance=0;

const char webpage[] PROGMEM = R"====(
<!DOCTYPE html>
<html>
<head>

<meta name="viewport" content="width=device-width, initial-scale=1">

<style>

body{
font-family:Arial;
background:#f5f6fa;
text-align:center;
}

.card{
background:white;
max-width:420px;
margin:auto;
margin-top:20px;
padding:20px;
border-radius:12px;
box-shadow:0 3px 12px rgba(0,0,0,0.15);
}

button{
width:100%;
padding:16px;
font-size:18px;
border:none;
border-radius:10px;
margin-top:10px;
}

.on{background:#2ecc71;color:white;}
.off{background:#e74c3c;color:white;}

.manual{background:#f1c40f;color:black;}
.sensor{background:#3498db;color:white;}

</style>

</head>

<body>

<div class="card">

<h2>SMART LAMP</h2>

<p>Distance : <b><span id="dist">0</span> cm</b></p>
<p>Relay : <b><span id="relay">OFF</span></b></p>
<p>Mode : <b><span id="mode">MANUAL</span></b></p>

<button id="relayBtn">Lamp</button>

<button id="modeBtn">Mode</button>

<div id="sensorSettings" style="display:none">

<br>

Trigger Distance

<input id="distInput" type="number">

<button onclick="saveDistance()">Save Distance</button>

<br><br>

Sensor Behaviour

<button onclick="setBehaviour(0)">AUTO</button>
<button onclick="setBehaviour(1)">OBJECT PASS</button>

</div>

</div>

<script>

let relay=false
let mode="MANUAL"

function toggleRelay(){

fetch(relay?"/off":"/on")

}

function toggleMode(){

fetch(mode=="MANUAL"?"/sensor":"/manual")

}

function saveDistance(){

let v=document.getElementById("distInput").value

fetch("/set?d="+v)

}

function setBehaviour(v){

fetch("/behaviour?b="+v)

}

document.getElementById("relayBtn").onclick=toggleRelay
document.getElementById("modeBtn").onclick=toggleMode

setInterval(()=>{

fetch("/data")
.then(r=>r.json())
.then(d=>{

document.getElementById("dist").innerHTML=d.distance
document.getElementById("relay").innerHTML=d.relay
document.getElementById("mode").innerHTML=d.mode

relay=d.relay=="ON"
mode=d.mode

let rbtn=document.getElementById("relayBtn")
let mbtn=document.getElementById("modeBtn")

if(relay){
rbtn.className="on"
rbtn.innerHTML="Lamp ON"
}else{
rbtn.className="off"
rbtn.innerHTML="Lamp OFF"
}

if(mode=="MANUAL"){

mbtn.className="manual"
mbtn.innerHTML="Manual Mode"

document.getElementById("sensorSettings").style.display="none"

}
else{

mbtn.className="sensor"
mbtn.innerHTML="Sensor Mode"

document.getElementById("sensorSettings").style.display="block"

}

})

},800)

</script>

</body>
</html>
)====";

void saveData(){

EEPROM.write(0,relayState);
EEPROM.write(1,mode);
EEPROM.write(2,triggerDistance);
EEPROM.write(3,sensorBehaviour);

EEPROM.commit();

}

void loadData(){

relayState=EEPROM.read(0);
mode=EEPROM.read(1);
triggerDistance=EEPROM.read(2);
sensorBehaviour=EEPROM.read(3);

if(triggerDistance<2 || triggerDistance>200)
triggerDistance=10;

}

float readDistance(){

digitalWrite(TRIG,LOW);
delayMicroseconds(2);

digitalWrite(TRIG,HIGH);
delayMicroseconds(10);
digitalWrite(TRIG,LOW);

long duration=pulseIn(ECHO,HIGH,30000);

float d=duration*0.034/2;

if(d==0 || d>400) return distance;

return d;

}

void handleRoot(){

server.send_P(200,"text/html",webpage);

}

void handleData(){

String json="{\"distance\":";
json+=distance;
json+=",\"relay\":\"";
json+=relayState?"ON":"OFF";
json+="\",\"mode\":\"";
json+=mode==0?"MANUAL":"SENSOR";
json+="\",\"behaviour\":";
json+=sensorBehaviour;
json+="}";

server.send(200,"application/json",json);

}

void setup(){

pinMode(TRIG,OUTPUT);
pinMode(ECHO,INPUT);
pinMode(RELAY,OUTPUT);

Serial.begin(115200);

EEPROM.begin(512);

loadData();

digitalWrite(RELAY,relayState);

WiFi.softAP("SmartLamp");

IPAddress IP(192,168,4,1);
IPAddress gateway(192,168,4,1);
IPAddress subnet(255,255,255,0);

WiFi.softAPConfig(IP,gateway,subnet);

server.on("/",handleRoot);
server.on("/data",handleData);

server.on("/on",[](){

relayState=true;
digitalWrite(RELAY,HIGH);
saveData();
server.send(200,"text/plain","OK");

});

server.on("/off",[](){

relayState=false;
digitalWrite(RELAY,LOW);
saveData();
server.send(200,"text/plain","OK");

});

server.on("/manual",[](){

mode=0;
saveData();
server.send(200,"text/plain","OK");

});

server.on("/sensor",[](){

mode=1;
saveData();
server.send(200,"text/plain","OK");

});

server.on("/set",[](){

triggerDistance=server.arg("d").toInt();

if(triggerDistance<2) triggerDistance=2;
if(triggerDistance>200) triggerDistance=200;

saveData();

server.send(200,"text/plain","OK");

});

server.on("/behaviour",[](){

sensorBehaviour=server.arg("b").toInt();

saveData();

server.send(200,"text/plain","OK");

});

server.begin();

}

void loop(){

server.handleClient();

distance=readDistance();

if(mode==1){

if(sensorBehaviour==0){

relayState=(distance<triggerDistance);

}
else{

relayState=(distance>triggerDistance);

}

digitalWrite(RELAY,relayState);

}

delay(150);

}
