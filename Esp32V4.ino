#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_task_wdt.h>
#include <ArduinoOTA.h>

float TEMP_LIGAR_1 = 22.0;
float TEMP_LIGAR_2 = 24.0;
float TEMP_LIGAR_3 = 26.0;

float HISTERESIS = 1.5;
unsigned long TEMPO_MINIMO = 30000;

#define DHTPIN 4
#define DHTTYPE DHT22

#define RELE1 18
#define RELE2 19
#define RELE3 21

#define WATCHDOG_TIMEOUT 10

const char* ssid = "cocojumbo";
const char* password = "cartas456";

const char* mqtt_server = "192.168.3.38";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient mqtt(espClient);

DHT dht(DHTPIN, DHTTYPE);

bool estadoRele1 = false;
bool estadoRele2 = false;
bool estadoRele3 = false;

bool modoAutomatico = true;
bool falhaSensor = false;
bool alarmeTemperatura = false;

unsigned long ultimoToggle1 = 0;
unsigned long ultimoToggle2 = 0;
unsigned long ultimoToggle3 = 0;

unsigned long ultimoEnvioMQTT = 0;
unsigned long ultimoTentativaWiFi = 0;
unsigned long ultimoTentativaMQTT = 0;

void ligarRele(int relePin, bool &estadoAtual) {
  digitalWrite(relePin, LOW);
  estadoAtual = true;
}

void desligarRele(int relePin, bool &estadoAtual) {
  digitalWrite(relePin, HIGH);
  estadoAtual = false;
}

void conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  if (millis() - ultimoTentativaWiFi >= 10000) {
    ultimoTentativaWiFi = millis();
    WiFi.begin(ssid, password);
    Serial.println("Tentando conectar ao WiFi...");
  }
}

void conectarMQTT() {
  if (mqtt.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;

  if (millis() - ultimoTentativaMQTT >= 5000) {
    ultimoTentativaMQTT = millis();

    if (mqtt.connect("ESP32_GALPAO_1")) {
      Serial.println("MQTT conectado");
    } else {
      Serial.print("Falha MQTT: ");
      Serial.println(mqtt.state());
    }
  }
}

void configurarOTA() {
  ArduinoOTA.setHostname("ESP32-GALPAO-1");

  ArduinoOTA.onStart([]() {
    Serial.println("Iniciando atualização OTA");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("Atualização OTA finalizada");
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.print("Erro OTA: ");
    Serial.println(error);
  });

  ArduinoOTA.begin();
}

void publicarMQTT(float temperatura) {
  if (!mqtt.connected()) return;

  mqtt.publish("granja/galpao1/sensores/temperatura", String(temperatura).c_str());
  mqtt.publish("granja/galpao1/reles/rele1", estadoRele1 ? "ON" : "OFF");
  mqtt.publish("granja/galpao1/reles/rele2", estadoRele2 ? "ON" : "OFF");
  mqtt.publish("granja/galpao1/reles/rele3", estadoRele3 ? "ON" : "OFF");
  mqtt.publish("granja/galpao1/alarmes/falhaSensor", falhaSensor ? "TRUE" : "FALSE");
  mqtt.publish("granja/galpao1/alarmes/temperaturaAlta", alarmeTemperatura ? "TRUE" : "FALSE");
  mqtt.publish("granja/galpao1/modo", modoAutomatico ? "AUTO" : "MANUAL");
}

void failSafeSensor() {
  ligarRele(RELE1, estadoRele1);
  ligarRele(RELE2, estadoRele2);
  ligarRele(RELE3, estadoRele3);

  falhaSensor = true;

  Serial.println("FALHA SENSOR - FAILSAFE ATIVADO");
}

void controlarRele(
  float temperatura,
  float tempLigar,
  int relePin,
  bool &estadoAtual,
  unsigned long &ultimoToggle
) {
  unsigned long agora = millis();

  if (agora - ultimoToggle < TEMPO_MINIMO) {
    return;
  }

  if (!estadoAtual && temperatura >= tempLigar) {
    ligarRele(relePin, estadoAtual);
    ultimoToggle = agora;
  }

  else if (estadoAtual && temperatura <= (tempLigar - HISTERESIS)) {
    desligarRele(relePin, estadoAtual);
    ultimoToggle = agora;
  }
}

void setup() {
  Serial.begin(115200);

  dht.begin();

  pinMode(RELE1, OUTPUT);
  pinMode(RELE2, OUTPUT);
  pinMode(RELE3, OUTPUT);

  desligarRele(RELE1, estadoRele1);
  desligarRele(RELE2, estadoRele2);
  desligarRele(RELE3, estadoRele3);

  esp_task_wdt_init(WATCHDOG_TIMEOUT, true);
  esp_task_wdt_add(NULL);

  WiFi.mode(WIFI_STA);
  conectarWiFi();

  mqtt.setServer(mqtt_server, mqtt_port);

  configurarOTA();

  Serial.println("Sistema iniciado");
}

void loop() {
  esp_task_wdt_reset();

  conectarWiFi();
  conectarMQTT();

  mqtt.loop();
  ArduinoOTA.handle();

  float temperatura = dht.readTemperature();

  if (isnan(temperatura)) {
    failSafeSensor();

    if (millis() - ultimoEnvioMQTT >= 5000) {
      publicarMQTT(-1);
      ultimoEnvioMQTT = millis();
    }

    delay(1000);
    return;
  }

  falhaSensor = false;

  Serial.print("Temperatura: ");
  Serial.print(temperatura);
  Serial.println(" °C");

  alarmeTemperatura = temperatura >= 30.0;

  if (modoAutomatico) {
    controlarRele(temperatura, TEMP_LIGAR_1, RELE1, estadoRele1, ultimoToggle1);
    controlarRele(temperatura, TEMP_LIGAR_2, RELE2, estadoRele2, ultimoToggle2);
    controlarRele(temperatura, TEMP_LIGAR_3, RELE3, estadoRele3, ultimoToggle3);
  }

  if (millis() - ultimoEnvioMQTT >= 5000) {
    publicarMQTT(temperatura);
    ultimoEnvioMQTT = millis();
  }

  delay(1000);
}