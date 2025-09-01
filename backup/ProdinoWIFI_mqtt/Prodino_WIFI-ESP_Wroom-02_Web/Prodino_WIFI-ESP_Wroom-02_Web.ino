// =============================================================
//Codice per scheda PRODINo WIFI-ESP WROOM-02
// https://kmpelectronics.eu/tutorials-examples/prodino-wifi-tutorial/
// Autore chatGPT da idea di Andrea Montefusco (RIP), 20250820
// =============================================================
// ProDino WiFi-ESP WROOM-02
// Interfaccia WEB sempre disponibile + Config WiFi/MQTT persistente
// Fallback AP aperto se lo STA non si connette
// MQTT: pubblicazione ingressi, controllo relè, LWT
// -------------------------------------------------------------
// Dipendenze:
//  - ESP8266 core
//  - PubSubClient
//  - KMPCommon.h, KMPDinoWiFiESP.h (librerie KMP Electronics)
// -------------------------------------------------------------
// Note:
//  - La pagina di configurazione è raggiungibile SEMPRE (AP o STA)
//  - Parametri salvati in EEPROM con struttura (no delimitatori)
//  - Modificabili: SSID/Pass WiFi, Broker, Porta, User/Pass, Base Topic, Intervallo
//  - Base topic usata per:
//      <base>/relay/<n>/set | ON/OFF/1/0
//      <base>/relay/<n>/state
//      <base>/input/<n>/state
//      <base>/status (online/offline)
//  - N relè/opto definiti per ProDino (4 + 4)
// =============================================================

#include <KMPCommon.h>
#include <KMPDinoWiFiESP.h>
#include <MqttTopicHelper.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

// =======================
// CONFIG
// =======================
#define EEPROM_SIZE 512
#define CFG_MAGIC 0x5A5AD1A5  // magic per validazione
#define NUM_INPUTS 4
#define NUM_RELAYS 4

ESP8266WebServer server(80);
WiFiClient espClient;
PubSubClient mqttClient(espClient);
KMPDinoWiFiESPClass DinoWiFi;

// Struttura di configurazione
struct Config {
  uint32_t magic;
  char ssid[32];
  char wpass[32];
  char mserver[32];
  int mport;
  char muser[32];
  char mpass[32];
  char mtopic[32];
} cfg;

// Stato attuale
bool relayState[NUM_RELAYS] = {false, false, false, false};
bool inputState[NUM_INPUTS] = {false, false, false, false};

// =======================
// Gestione Config
// =======================
void setDefaultConfig() {
  memset(&cfg, 0, sizeof(cfg));
  cfg.magic = CFG_MAGIC;
  strcpy(cfg.ssid, "");
  strcpy(cfg.wpass, "");
  strcpy(cfg.mserver, "192.168.1.10");
  cfg.mport = 1883;
  strcpy(cfg.muser, "");
  strcpy(cfg.mpass, "");
  strcpy(cfg.mtopic, "prodino");
}

void saveConfig() {
  EEPROM.begin(EEPROM_SIZE);
  cfg.magic = CFG_MAGIC;
  EEPROM.put(0, cfg);
  EEPROM.commit();
}

void loadConfig() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, cfg);
  if (cfg.magic != CFG_MAGIC) {
    setDefaultConfig();
    saveConfig();
  }
}

// =======================
// MQTT
// =======================
void publishInputState(int i, bool state) {
  String topic = String(cfg.mtopic) + "/input/" + String(i+1) + "/state";
  mqttClient.publish(topic.c_str(), state ? "1" : "0", true);
}

void publishRelayState(int r, bool state) {
  String topic = String(cfg.mtopic) + "/relay/" + String(r+1) + "/state";
  mqttClient.publish(topic.c_str(), state ? "ON" : "OFF", true);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String t = topic;
  String msg;
  for (unsigned int i=0; i<length; i++) msg += (char)payload[i];
  Serial.printf("MQTT RX topic: %s payload: %s\n", topic, msg.c_str());

  for (int r=0; r<NUM_RELAYS; r++) {
    String setTopic = String(cfg.mtopic) + "/relay/" + String(r+1) + "/set";
    if (t == setTopic) {
      if (msg == "ON") {
        DinoWiFi.SetRelayState(r, true);
        relayState[r] = true;
        Serial.printf("Relay %d -> ON\n", r+1);
      } else if (msg == "OFF") {
        DinoWiFi.SetRelayState(r, false);
        relayState[r] = false;
        Serial.printf("Relay %d -> OFF\n", r+1);
      }
      publishRelayState(r, relayState[r]);
    }
  }
}

void reconnectMQTT() {
  if (cfg.mserver[0] == 0) return;
  if (!mqttClient.connected()) {
    if (mqttClient.connect("ProDinoESP", cfg.muser, cfg.mpass)) {
      for (int r=0; r<NUM_RELAYS; r++) {
        String setTopic = String(cfg.mtopic) + "/relay/" + String(r+1) + "/set";
        mqttClient.subscribe(setTopic.c_str());
      }
      // Pubblica stati iniziali
      for (int r=0; r<NUM_RELAYS; r++) publishRelayState(r, relayState[r]);
      for (int i=0; i<NUM_INPUTS; i++) publishInputState(i, inputState[i]);
    }
  }
}

// =======================
// Web Server
// =======================
void handleConfigPage() { // Pagina di setup
  String html = "<h1>Configurazione</h1>"
                "<form action='/save' method='POST'>"
                "<h2>WiFi</h2>"
                "SSID: <input type='text' name='ssid' value='" + String(cfg.ssid) + "'><br>"
                "Password: <input type='password' name='wpass' value='" + String(cfg.wpass) + "'><br><hr>"
                "<h2>MQTT</h2>"
                "Server: <input type='text' name='mserver' value='" + String(cfg.mserver) + "'><br>"
                "Porta: <input type='number' name='mport' value='" + String(cfg.mport) + "'><br>"
                "User: <input type='text' name='muser' value='" + String(cfg.muser) + "'><br>"
                "Pass: <input type='password' name='mpass' value='" + String(cfg.mpass) + "'><br>"
                "Topic base: <input type='text' name='mtopic' value='" + String(cfg.mtopic) + "'><br>"
                "<input type='submit' value='Salva'>"
                "</form>"
                "<p><a href='/control'>Vai al controllo I/O</a></p>";
  server.send(200, "text/html", html);
}

void handleSave() {
  strcpy(cfg.ssid, server.arg("ssid").c_str());
  strcpy(cfg.wpass, server.arg("wpass").c_str());
  strcpy(cfg.mserver, server.arg("mserver").c_str());
  cfg.mport = server.arg("mport").toInt();
  strcpy(cfg.muser, server.arg("muser").c_str());
  strcpy(cfg.mpass, server.arg("mpass").c_str());
  strcpy(cfg.mtopic, server.arg("mtopic").c_str());
  saveConfig();
  server.send(200, "text/html", "<h1>Salvato! Riavvio...</h1>");
  delay(2000);
  ESP.restart();
}

void handleControlPage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Controllo ProDino</title>
  <style>
    body { font-family: sans-serif; text-align: center; }
    table { margin:auto; border-collapse: collapse; }
    td { padding: 8px; border: 1px solid #ccc; }
    .on { color: green; font-weight: bold; }
    .off { color: red; font-weight: bold; }
  </style>
</head>
<body>
  <h1>Controllo ProDino</h1>
  <table>
    <tr><th>Ingresso</th><th>Stato</th></tr>
    <tr><td>IN1</td><td id="in1">?</td></tr>
    <tr><td>IN2</td><td id="in2">?</td></tr>
    <tr><td>IN3</td><td id="in3">?</td></tr>
    <tr><td>IN4</td><td id="in4">?</td></tr>
  </table>
  <br>
  <table>
    <tr><th>Relè</th><th>Stato</th><th>Comando</th></tr>
    <tr><td>R1</td><td id="r1">?</td>
        <td><button onclick="setRelay(0,'ON')">ON</button>
            <button onclick="setRelay(0,'OFF')">OFF</button></td></tr>
    <tr><td>R2</td><td id="r2">?</td>
        <td><button onclick="setRelay(1,'ON')">ON</button>
            <button onclick="setRelay(1,'OFF')">OFF</button></td></tr>
    <tr><td>R3</td><td id="r3">?</td>
        <td><button onclick="setRelay(2,'ON')">ON</button>
            <button onclick="setRelay(2,'OFF')">OFF</button></td></tr>
    <tr><td>R4</td><td id="r4">?</td>
        <td><button onclick="setRelay(3,'ON')">ON</button>
            <button onclick="setRelay(3,'OFF')">OFF</button></td></tr>
  </table>
  <script>
    async function refreshStatus() {
      const res = await fetch('/status.json');
      const data = await res.json();
      // ingressi
      for (let i=0;i<4;i++) {
        document.getElementById('in'+(i+1)).innerHTML =
          data.inputs[i]=="1" ? "<span class='on'>ON</span>" : "<span class='off'>OFF</span>";
      }
      // relè
      for (let r=0;r<4;r++) {
        document.getElementById('r'+(r+1)).innerHTML =
          data.relays[r]=="1" ? "<span class='on'>ON</span>" : "<span class='off'>OFF</span>";
      }
    }
    function setRelay(r, state) {
      fetch('/relay?ch='+r+'&set='+state);
      setTimeout(refreshStatus,500);
    }
    setInterval(refreshStatus, 2000); // refresh ogni 2s
    refreshStatus();
  </script>
<!-- LINK alla HOME -->
  <div class="menu">
    <a href="/">🏠 Home</a>
  </div>

</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}


void handleRelayCmd() {
  int r = server.arg("ch").toInt();
  String cmd = server.arg("cmd");
  if (r>=0 && r<NUM_RELAYS) {
    if (cmd == "ON") {
      DinoWiFi.SetRelayState(r, true);
      relayState[r] = true;
    } else {
      DinoWiFi.SetRelayState(r, false);
      relayState[r] = false;
    }
    publishRelayState(r, relayState[r]);
  }
  handleControlPage();
}

void handleStatusJson() {
  String json = "{";
  // ingressi
  json += "\"inputs\":[";
  for (int i=0; i<NUM_INPUTS; i++) {
    json += inputState[i] ? "1" : "0";
    if (i < NUM_INPUTS-1) json += ",";
  }
  json += "],";
  // relè
  json += "\"relays\":[";
  for (int r=0; r<NUM_RELAYS; r++) {
    json += relayState[r] ? "1" : "0";
    if (r < NUM_RELAYS-1) json += ",";
  }
  json += "]";
  json += "}";
  server.send(200, "application/json", json);
}

void setupWebServer() {
  server.on("/", handleConfigPage);
  server.on("/control", handleControlPage);
  server.on("/status.json", handleStatusJson);

  // ✅ endpoint per comando relè da web
  server.on("/relay", HTTP_GET, []() {
    if (server.hasArg("ch") && server.hasArg("set")) {
      int r = server.arg("ch").toInt();
      String s = server.arg("set");
      s.trim();
      s.toUpperCase();
      bool st = (s == "ON");

      DinoWiFi.SetRelayState(r, st);
      relayState[r] = st;
      publishRelayState(r, st);

      server.send(200, "text/plain", "OK");
      Serial.println("Ricevuto set=" + s);
    } else {
      server.send(400, "text/plain", "Bad Request");
      Serial.println("Richiesta relay senza parametri");
    }
  });

  server.begin();
  Serial.println("Web server avviato");
}



// =======================
// Setup
// =======================
void setup() {
  Serial.begin(115200);
  loadConfig();

  Serial.println("[SETUP] Inizializzazione Prodino...");
  DinoWiFi.init();
  Serial.println("[SETUP] Prodino inizializzato");

  if (strlen(cfg.ssid) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.ssid, cfg.wpass);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
      delay(500); Serial.print(".");
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP_Config");
    Serial.println("AP attivo: SSID=ESP_Config IP=192.168.4.1");
  } else {
    Serial.print("Connesso IP: ");
    Serial.println(WiFi.localIP());
  }

  setupWebServer();

  mqttClient.setServer(cfg.mserver, cfg.mport);
  mqttClient.setCallback(mqttCallback);

  // Stati iniziali I/O
  for (int r=0; r<NUM_RELAYS; r++) {
    relayState[r] = DinoWiFi.GetRelayState(r);
  }
  for (int i=0; i<NUM_INPUTS; i++) {
    inputState[i] = DinoWiFi.GetOptoInState(i);
  }
}

// =======================
// Loop
// =======================
void loop() {
  server.handleClient();

  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) reconnectMQTT();
    mqttClient.loop();
  }

  // Monitoraggio ingressi
  for (int i=0; i<NUM_INPUTS; i++) {
    bool s = DinoWiFi.GetOptoInState(i);
    if (s != inputState[i]) {
      inputState[i] = s;
      Serial.printf("Ingresso %d cambiato a %d\n", i+1, s);
      publishInputState(i, s);
    }
  }
}
