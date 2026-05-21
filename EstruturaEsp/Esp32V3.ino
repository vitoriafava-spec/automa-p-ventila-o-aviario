#include <DHT.h>
#include <WiFi.h>
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

void ligarRele(int relePin, bool &estadoAtual) {
  digitalWrite(relePin, LOW);
  estadoAtual = true;
}

void desligarRele(int relePin, bool &estadoAtual) {
  digitalWrite(relePin, HIGH);
  estadoAtual = false;
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

  Serial.println("Sistema iniciado");
}

void loop() {

  esp_task_wdt_reset();

  float temperatura = dht.readTemperature();

  if (isnan(temperatura)) {

    failSafeSensor();

    delay(5000);
    return;
  }

  falhaSensor = false;

  Serial.print("Temperatura: ");
  Serial.print(temperatura);
  Serial.println(" °C");

  if (temperatura >= 30.0) {
    alarmeTemperatura = true;
  } else {
    alarmeTemperatura = false;
  }

  if (modoAutomatico) {

    controlarRele(
      temperatura,
      TEMP_LIGAR_1,
      RELE1,
      estadoRele1,
      ultimoToggle1
    );

    controlarRele(
      temperatura,
      TEMP_LIGAR_2,
      RELE2,
      estadoRele2,
      ultimoToggle2
    );

    controlarRele(
      temperatura,
      TEMP_LIGAR_3,
      RELE3,
      estadoRele3,
      ultimoToggle3
    );
  }

  delay(5000);
}
