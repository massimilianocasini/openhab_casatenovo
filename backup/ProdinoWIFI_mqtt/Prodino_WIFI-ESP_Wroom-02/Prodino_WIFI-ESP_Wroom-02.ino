//Codice per scheda PRODINo WIFI-ESP WROOM-02
// https://kmpelectronics.eu/tutorials-examples/prodino-wifi-tutorial/
// Autore chatGPT da idea di Andrea Montefusco (RIP), 20250820
// Board manager, URL da aggiungere http://arduino.esp8266.com/stable/package_esp8266com_index.json, dovresti trovare esp8266 by ESP8266 Community" and then press "Install"
// Selecting Tools/Board: "xxxxxx", "Generic ESP8266 Module"
// Installing the board library:
// 1 Downloading and save the file "PRODINoESP8266.zip"on your desktop.
// 2 Opening your Arduino IDE. From the menu select: Sketch > Include Library > Add .ZIP Library...
// 3 Select the zip file "PRODINoESP8266.zip" from your desktop and click Open.

#include <KMPCommon.h>
#include <KMPDinoWiFiESP.h>
#include <MqttTopicHelper.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// =======================
// Configurazione WiFi
// =======================
const char* ssid = "MisiSpot";
const char* password = "multiactivity";

// =======================
// Configurazione MQTT
// =======================
const char* mqtt_server = "cm4home.local";
const int mqtt_port = 1883;
const char* mqtt_user = "openhabian";
const char* mqtt_password = "openhabian";

// =======================
// Oggetto Prodino
// =======================
KMPDinoWiFiESPClass DinoWiFi;

// =======================
// Topics MQTT
// =======================
const char* inputTopics[4]  = {"prodino/input1", "prodino/input2", "prodino/input3", "prodino/input4"};
const char* outputTopics[4] = {"prodino/output1", "prodino/output2", "prodino/output3", "prodino/output4"};

// Stato precedente ingressi
int lastInputState[4] = {HIGH, HIGH, HIGH, HIGH};

// MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

// =======================
// Intervallo pubblicazione periodica (secondi)
// =======================
const unsigned long publishInterval = 10; 
unsigned long lastPublishTime = 0;

// =======================
// Funzione pubblica ingressi
// =======================
void publishInputs() {
  Serial.println("[INFO] Pubblicazione ingressi...");
  for (int i = 0; i < 4; i++) {  // zero-based
    bool stato = DinoWiFi.GetOptoInState(i);   // legge ingresso i
    Serial.print("[INPUT] Ingresso ");
    Serial.print(i+1);  // numerazione umana
    Serial.print(": ");
    Serial.println(stato ? "HIGH" : "LOW");

    client.publish(inputTopics[i], stato ? "1" : "0", true);
  }
}

// =======================
// Callback MQTT (comando relè)
// =======================
void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String message = String((char*)payload);
  Serial.print("[MQTT] Messaggio ricevuto sul topic ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(message);

  for (int i = 1; i <= 4; i++) {
    if (String(topic) == outputTopics[i-1]) {
      if (message == "1") {
        DinoWiFi.SetRelayState(i-1, true);   // accende il relè
        Serial.print("[RELAY] Relè ");
        Serial.print(i);
        Serial.println(" acceso");
      } else {
        DinoWiFi.SetRelayState(i-1, false);  // spegne il relè
        Serial.print("[RELAY] Relè ");
        Serial.print(i);
        Serial.println(" spento");
      }
    }
  }
}

// =======================
// Riconnessione MQTT
// =======================
void reconnect() {
  while (!client.connected()) {
    Serial.println("[MQTT] Tentativo di connessione...");
    if (client.connect("ProdinoClient", mqtt_user, mqtt_password)) {
      Serial.println("[MQTT] Connesso!");
      for (int i = 0; i < 4; i++) {
        client.subscribe(outputTopics[i]);
        Serial.print("[MQTT] Sottoscritto al topic: ");
        Serial.println(outputTopics[i]);
      }
    } else {
      Serial.print("[MQTT] Connessione fallita, rc=");
      Serial.println(client.state());
      Serial.println("[MQTT] Ritento tra 5 secondi...");
      delay(5000);
    }
  }
}

// =======================
// Setup
// =======================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("[SETUP] Inizializzazione Prodino...");
  DinoWiFi.init();
  Serial.println("[SETUP] Prodino inizializzato");

  // Connessione WiFi
  Serial.print("[SETUP] Connessione a WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[SETUP] WiFi connesso!");
  Serial.print("[SETUP] IP: ");
  Serial.println(WiFi.localIP());

  // Config MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

// =======================
// Loop
// =======================
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

// =======================
// Loop: controllo cambiamenti ingressi
// =======================
for (int i = 0; i < 4; i++) {   // zero-based
  bool reading = DinoWiFi.GetOptoInState(i);
  if (reading != lastInputState[i]) {
    lastInputState[i] = reading;

    Serial.print("[INPUT CHANGE] Ingresso ");
    Serial.print(i+1);
    Serial.print(": ");
    Serial.println(reading ? "HIGH" : "LOW");

    client.publish(inputTopics[i], reading ? "1" : "0", true);
  }
}

  // Pubblicazione periodica
  unsigned long now = millis();
  if (now - lastPublishTime >= publishInterval * 1000UL) {
    lastPublishTime = now;
    publishInputs();
  }
}


