#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "secrets.h"

#define PINO_SENSOR 4
#define PINO_RELE   26
#define PINO_BUZZER 27
#define LED_R 25
#define LED_G 33
#define LED_B 32

#define RELE_LIGA      HIGH
#define RELE_DESLIGA   LOW
#define BUZZER_LIGA    LOW
#define BUZZER_DESLIGA HIGH

const char* MQTT_BROKER  = "broker.hivemq.com";
const int   MQTT_PORTA   = 8883;
const char* TOPICO_DADOS = "acude/t02n/dados";
const char* TOPICO_CMD   = "acude/t02n/cmd";

float limiteQuente  = 30.0;
float limiteRetorno = 28.0;
const float LIMITE_FRIO = 20.0;

OneWire oneWire(PINO_SENSOR);
DallasTemperature sensor(&oneWire);
Adafruit_SSD1306 tela(128, 64, &Wire, -1);
WiFiClientSecure wifiClient;
PubSubClient mqtt(wifiClient);

bool aeradorLigado = false;
bool modoManual = false;
bool comandoAerador = false;
String estado = "INICIANDO";
float tempAtual = 0;

unsigned long ultimaLeitura = 0;
unsigned long ultimoEnvio = 0;
unsigned long ultimaTentativa = 0;

void corLED(bool r, bool g, bool b) {
  digitalWrite(LED_R, r);
  digitalWrite(LED_G, g);
  digitalWrite(LED_B, b);
}

void mostrarNaTela() {
  tela.clearDisplay();
  tela.setTextSize(1);
  tela.setCursor(0, 0);
  tela.println(estado);

  tela.setTextSize(3);
  tela.setCursor(0, 20);
  if (tempAtual == DEVICE_DISCONNECTED_C) tela.println("ERRO");
  else { tela.print(tempAtual, 1); tela.println("C"); }

  tela.setTextSize(1);
  tela.setCursor(0, 48);
  tela.print(modoManual ? "MANUAL " : "AUTO ");
  tela.println(aeradorLigado ? "Aer:ON" : "Aer:OFF");
  tela.setCursor(0, 56);
  tela.print("WiFi:");
  tela.print(WiFi.status() == WL_CONNECTED ? "OK " : "-- ");
  tela.print("MQTT:");
  tela.println(mqtt.connected() ? "OK" : "--");
  tela.display();
}

void aoReceber(char* topico, byte* payload, unsigned int tamanho) {
  if (tamanho > 200) {
    Serial.println("CMD rejeitado: muito grande");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, payload, tamanho)) {
    Serial.println("CMD rejeitado: JSON invalido");
    return;
  }

  if (!doc["token"].is<const char*>() || String(doc["token"].as<const char*>()) != CMD_TOKEN) {
    Serial.println("CMD rejeitado: token invalido");
    return;
  }

  if (doc["modo"].is<const char*>()) {
    String m = doc["modo"].as<String>();
    if (m == "manual") modoManual = true;
    else if (m == "auto") modoManual = false;
    else { Serial.println("CMD rejeitado: modo invalido"); return; }
  }

  if (doc["aerador"].is<bool>()) {
    comandoAerador = doc["aerador"];
  }

  if (doc["limite"].is<float>()) {
    float l = doc["limite"];
    if (l < 22 || l > 35) {
      Serial.println("CMD rejeitado: limite fora de 22-35");
      return;
    }
    limiteQuente = l;
    limiteRetorno = l - 2;
  }
  Serial.println("CMD aceito");
}

void manterConexoes() {
  if (millis() - ultimaTentativa < 5000) return;
  ultimaTentativa = millis();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi caiu, tentando reconectar...");
    WiFi.reconnect();
    return;
  }
  if (!mqtt.connected()) {
    String id = "acude-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    if (mqtt.connect(id.c_str())) {
      mqtt.subscribe(TOPICO_CMD);
      Serial.println("MQTT conectado (TLS)");
    } else {
      Serial.println("MQTT falhou, tenta de novo em 5 s");
    }
  }
}

void enviarDados() {
  if (!mqtt.connected()) return;
  JsonDocument doc;
  doc["id"] = "acude1";
  doc["temp"] = round(tempAtual * 10) / 10.0;
  doc["estado"] = estado;
  doc["aerador"] = aeradorLigado;
  doc["modo"] = modoManual ? "manual" : "auto";
  doc["limite"] = limiteQuente;
  doc["ts"] = millis() / 1000;
  char buffer[200];
  serializeJson(doc, buffer);
  mqtt.publish(TOPICO_DADOS, buffer);
  Serial.print("Enviado: ");
  Serial.println(buffer);
}

void lerEDecidir() {
  sensor.requestTemperatures();
  tempAtual = sensor.getTempCByIndex(0);

  if (tempAtual == DEVICE_DISCONNECTED_C) {
    estado = "FALHA NO SENSOR";
    aeradorLigado = false;
    digitalWrite(PINO_RELE, RELE_DESLIGA);
    digitalWrite(PINO_BUZZER, BUZZER_DESLIGA);
    corLED(1, 0, 1);
    return;
  }

  if (modoManual) {
    aeradorLigado = comandoAerador;
  } else {
    if (tempAtual >= limiteQuente)       aeradorLigado = true;
    else if (tempAtual <= limiteRetorno) aeradorLigado = false;
  }

  bool alarme = false;
  if (tempAtual >= limiteQuente)      { estado = "ALERTA: AGUA QUENTE"; corLED(1,0,0); alarme = true; }
  else if (tempAtual <= LIMITE_FRIO)  { estado = "ALERTA: AGUA FRIA";   corLED(0,0,1); alarme = true; }
  else if (aeradorLigado)             { estado = "RESFRIANDO";          corLED(1,1,0); }
  else                                { estado = "NORMAL";              corLED(0,1,0); }

  digitalWrite(PINO_RELE, aeradorLigado ? RELE_LIGA : RELE_DESLIGA);
  digitalWrite(PINO_BUZZER, alarme ? BUZZER_LIGA : BUZZER_DESLIGA);
}

void setup() {
  Serial.begin(115200);
  pinMode(PINO_RELE, OUTPUT);
  pinMode(PINO_BUZZER, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  digitalWrite(PINO_RELE, RELE_DESLIGA);
  digitalWrite(PINO_BUZZER, BUZZER_DESLIGA);

  sensor.begin();
  if (!tela.begin(SSD1306_SWITCHCAPVCC, 0x3C)) Serial.println("ERRO: display");
  tela.setTextColor(SSD1306_WHITE);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_SENHA);
  wifiClient.setInsecure();
  mqtt.setServer(MQTT_BROKER, MQTT_PORTA);
  mqtt.setCallback(aoReceber);
}

void loop() {
  manterConexoes();
  mqtt.loop();

  if (millis() - ultimaLeitura >= 1000) {
    ultimaLeitura = millis();
    lerEDecidir();
    mostrarNaTela();
  }
  if (millis() - ultimoEnvio >= 10000) {
    ultimoEnvio = millis();
    enviarDados();
  }
}