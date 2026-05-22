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

#define LED_WIFI 2

#define WATCHDOG_TIMEOUT 10

const char* ssid = "Jaringangay";
const char* password = "cartas259";

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
bool wifiConectadoAntes = false;

float temperaturaAtual = 0.0;

unsigned long ultimoToggle1 = 0;
unsigned long ultimoToggle2 = 0;
unsigned long ultimoToggle3 = 0;

unsigned long ultimoEnvioMQTT = 0;
unsigned long ultimoTentativaWiFi = 0;
unsigned long ultimoTentativaMQTT = 0;
unsigned long ultimaLeituraSensor = 0;
unsigned long ultimaComunicacaoMQTT = 0;

const unsigned long TIMEOUT_MODO_MANUAL = 60000;

int falhasDHT = 0;
const int LIMITE_FALHAS_DHT = 999;

void ligarRele(int relePin, bool &estadoAtual) {
  digitalWrite(relePin, LOW);
  estadoAtual = true;
}

void desligarRele(int relePin, bool &estadoAtual) {
  digitalWrite(relePin, HIGH);
  estadoAtual = false;
}

void publicarEstadoModo() {
  if (!mqtt.connected()) return;
  mqtt.publish("granja/galpao1/modo", modoAutomatico ? "AUTO" : "MANUAL");
}

void publicarEstadosReles() {
  if (!mqtt.connected()) return;

  mqtt.publish("granja/galpao1/reles/rele1", estadoRele1 ? "ON" : "OFF");
  mqtt.publish("granja/galpao1/reles/rele2", estadoRele2 ? "ON" : "OFF");
  mqtt.publish("granja/galpao1/reles/rele3", estadoRele3 ? "ON" : "OFF");
}

void callback(char* topic, byte* payload, unsigned int length) {
  String topico = String(topic);
  String mensagem = "";

  for (unsigned int i = 0; i < length; i++) {
    mensagem += (char)payload[i];
  }

  mensagem.trim();
  ultimaComunicacaoMQTT = millis();

  if (topico == "granja/galpao1/comandos/modo") {

    if (mensagem == "AUTO") {
      modoAutomatico = true;
      Serial.println("Modo alterado para AUTO");
      publicarEstadoModo();
    }

    else if (mensagem == "MANUAL") {
      modoAutomatico = false;
      Serial.println("Modo alterado para MANUAL");
      publicarEstadoModo();
    }

    else {
      Serial.print("Comando de modo invalido: ");
      Serial.println(mensagem);
    }

    return;
  }

  if (modoAutomatico) {
    Serial.println("Comando de rele ignorado: sistema em AUTO");
    return;
  }

  if (falhaSensor || alarmeTemperatura) {
    Serial.println("Comando manual ignorado: seguranca ativa");
    return;
  }

  if (topico == "granja/galpao1/comandos/rele1") {
    if (mensagem == "ON") ligarRele(RELE1, estadoRele1);
    else if (mensagem == "OFF") desligarRele(RELE1, estadoRele1);
  }

  else if (topico == "granja/galpao1/comandos/rele2") {
    if (mensagem == "ON") ligarRele(RELE2, estadoRele2);
    else if (mensagem == "OFF") desligarRele(RELE2, estadoRele2);
  }

  else if (topico == "granja/galpao1/comandos/rele3") {
    if (mensagem == "ON") ligarRele(RELE3, estadoRele3);
    else if (mensagem == "OFF") desligarRele(RELE3, estadoRele3);
  }

  publicarEstadosReles();
}

void conectarWiFi() {
  static bool tentativaEmAndamento = false;

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_WIFI, HIGH);

    if (!wifiConectadoAntes) {
      Serial.println("WiFi conectado");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      wifiConectadoAntes = true;
    }

    tentativaEmAndamento = false;
    return;
  }

  digitalWrite(LED_WIFI, LOW);
  wifiConectadoAntes = false;

  if (!tentativaEmAndamento) {
    WiFi.begin(ssid, password);
    tentativaEmAndamento = true;
    ultimoTentativaWiFi = millis();
    Serial.println("Tentando conectar ao WiFi...");
    return;
  }

  if (millis() - ultimoTentativaWiFi >= 15000) {
    WiFi.disconnect();
    tentativaEmAndamento = false;
    Serial.println("Reiniciando tentativa WiFi...");
  }
}

void conectarMQTT() {
  if (mqtt.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;

  if (millis() - ultimoTentativaMQTT >= 5000) {
    ultimoTentativaMQTT = millis();

    if (mqtt.connect("ESP32_GALPAO_1")) {
      Serial.println("MQTT conectado");

      ultimaComunicacaoMQTT = millis();

      mqtt.subscribe("granja/galpao1/comandos/modo");
      mqtt.subscribe("granja/galpao1/comandos/rele1");
      mqtt.subscribe("granja/galpao1/comandos/rele2");
      mqtt.subscribe("granja/galpao1/comandos/rele3");

      publicarEstadoModo();
      publicarEstadosReles();

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

  char tempStr[12];
  dtostrf(temperatura, 4, 2, tempStr);

  mqtt.publish("granja/galpao1/sensores/temperatura", tempStr);
  mqtt.publish("granja/galpao1/reles/rele1", estadoRele1 ? "ON" : "OFF");
  mqtt.publish("granja/galpao1/reles/rele2", estadoRele2 ? "ON" : "OFF");
  mqtt.publish("granja/galpao1/reles/rele3", estadoRele3 ? "ON" : "OFF");
  mqtt.publish("granja/galpao1/alarmes/falhaSensor", falhaSensor ? "TRUE" : "FALSE");
  mqtt.publish("granja/galpao1/alarmes/temperaturaAlta", alarmeTemperatura ? "TRUE" : "FALSE");
  mqtt.publish("granja/galpao1/modo", modoAutomatico ? "AUTO" : "MANUAL");
}

void failSafeSensor() {
  if (!falhaSensor) {
    ligarRele(RELE1, estadoRele1);
    ligarRele(RELE2, estadoRele2);
    ligarRele(RELE3, estadoRele3);

    falhaSensor = true;
    modoAutomatico = true;

    Serial.println("FALHA SENSOR - FAILSAFE ATIVADO");
  }
}

void forcarVentilacaoCritica() {
  ligarRele(RELE1, estadoRele1);
  ligarRele(RELE2, estadoRele2);
  ligarRele(RELE3, estadoRele3);
}

void verificarSegurancaModoManual() {
  if (!mqtt.connected() && !modoAutomatico) {
    modoAutomatico = true;
    Serial.println("MQTT desconectado: retornando para AUTO");
    return;
  }

  if (!modoAutomatico && millis() - ultimaComunicacaoMQTT >= TIMEOUT_MODO_MANUAL) {
    modoAutomatico = true;
    Serial.println("Timeout manual: retornando para AUTO");
    publicarEstadoModo();
  }
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

void configurarWatchdog() {
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WATCHDOG_TIMEOUT * 1000,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
    .trigger_panic = true
  };

  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
}

void setup() {
  Serial.begin(115200);

  dht.begin();

  pinMode(RELE1, OUTPUT);
  pinMode(RELE2, OUTPUT);
  pinMode(RELE3, OUTPUT);
  pinMode(LED_WIFI, OUTPUT);

  digitalWrite(LED_WIFI, LOW);

  desligarRele(RELE1, estadoRele1);
  desligarRele(RELE2, estadoRele2);
  desligarRele(RELE3, estadoRele3);

  configurarWatchdog();

  WiFi.mode(WIFI_STA);
  conectarWiFi();

  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(callback);

  configurarOTA();

  Serial.println("Sistema iniciado");
}

void loop() {
  esp_task_wdt_reset();

  conectarWiFi();
  conectarMQTT();

  mqtt.loop();
  ArduinoOTA.handle();

  verificarSegurancaModoManual();

  if (millis() - ultimaLeituraSensor >= 2000) {
    ultimaLeituraSensor = millis();

    float temperaturaLida = dht.readTemperature();

    if (isnan(temperaturaLida)) {
      falhasDHT++;

      Serial.print("Falha leitura DHT: ");
      Serial.println(falhasDHT);

      if (falhasDHT >= LIMITE_FALHAS_DHT) {
        failSafeSensor();
      }

    } else {
      falhasDHT = 0;
      falhaSensor = false;

      temperaturaAtual = temperaturaLida;
      alarmeTemperatura = temperaturaAtual >= 30.0;

      Serial.print("Temperatura: ");
      Serial.print(temperaturaAtual);
      Serial.println(" °C");
    }
  }

  if (falhaSensor) {
    forcarVentilacaoCritica();
  }

  else if (alarmeTemperatura) {
    forcarVentilacaoCritica();
  }

  else if (modoAutomatico) {
    controlarRele(temperaturaAtual, TEMP_LIGAR_1, RELE1, estadoRele1, ultimoToggle1);
    controlarRele(temperaturaAtual, TEMP_LIGAR_2, RELE2, estadoRele2, ultimoToggle2);
    controlarRele(temperaturaAtual, TEMP_LIGAR_3, RELE3, estadoRele3, ultimoToggle3);
  }

  if (millis() - ultimoEnvioMQTT >= 5000) {
    ultimoEnvioMQTT = millis();

    if (falhaSensor) {
      publicarMQTT(-1);
    } else {
      publicarMQTT(temperaturaAtual);
    }
  }
}