# 🐟 Monitoramento de Temperatura do Açude (IoT)

Trabalho final da disciplina de **Internet das Coisas** — 2º semestre / 2026.

Sistema IoT que mede a temperatura da água de um açude de criação de peixes, **decide localmente** quando ligar o aerador, avisa com alarme sonoro e luminoso e envia os dados por **Wi-Fi + MQTT (com TLS)** para um **painel web** com gráfico e comandos remotos.

## Integrantes

* *ANTHONY VINICIUS VIEIRA DA SILVA*
* *VINICIUS ESSER MARQUES*
* *PEDRO HENRIQUE DE ALMEIDA*

## Arquitetura (4 camadas)

|Camada|Componentes|
|-|-|
|Percepção|Sensor DS18B20 à prova d'água|
|Rede|Wi-Fi 2,4 GHz + MQTT sobre TLS (broker.hivemq.com:8883)|
|Processamento|ESP32: regra com histerese (liga ≥ 30 °C, desliga ≤ 28 °C, alerta de frio ≤ 20 °C)|
|Aplicação|Painel web (`painel/painel\_acude.html`), display OLED e LED RGB|

## Hardware

|Componente|Pino ESP32|
|-|-|
|DS18B20 (DATA + resistor 4,7 kΩ para 3V3)|GPIO 4|
|OLED SSD1306 I2C (SDA / SCL)|GPIO 21 / GPIO 22|
|Relé 3 V (IN1) — jumper em H|GPIO 26|
|Buzzer ativo (I/O) — aciona em LOW|GPIO 27|
|LED RGB catodo comum (R / G / B)|GPIO 25 / 33 / 32|
|Ventoinha 5 V (simula o aerador)|Relé: COM → VIN, NO → + da ventoinha|

## Estrutura do repositório

```
firmware/acude/acude.ino          código do ESP32
firmware/acude/secrets.example.h  modelo de credenciais
painel/painel\_acude.html          painel web (aplicação)
.gitignore                        impede o envio do secrets.h
```

## Como executar

### 1\. Firmware (ESP32)

1. Instale o **Arduino IDE 2** e o pacote **esp32** (Espressif) pelo Gerenciador de Placas.
2. Instale as bibliotecas: `OneWire`, `DallasTemperature`, `Adafruit SSD1306` (com `Adafruit GFX`), `PubSubClient`, `ArduinoJson`.
3. Na pasta `firmware/acude`, copie `secrets.example.h` para **`secrets.h`** e preencha:

```cpp
   #define WIFI\_SSID  "nome\_da\_rede"     // rede 2,4 GHz
   #define WIFI\_SENHA "senha\_da\_rede"
   #define CMD\_TOKEN  "token\_secreto"
   ```

4. Abra `acude.ino`, selecione a placa **ESP32 Dev Module** e a porta COM, e carregue.

   * Se a porta não aparecer, instale o driver **CP210x** (Silicon Labs).
5. No Monitor Serial (115200) deve aparecer `MQTT conectado (TLS)` e `Enviado: {...}` a cada 10 s.

### 2\. Painel

1. Abra `painel/painel\_acude.html` em qualquer navegador com internet.
2. Os dados aparecem em até 10 s. Para enviar comandos, digite o **token** definido no `secrets.h`.

## Comunicação MQTT

|Tópico|Direção|Conteúdo|
|-|-|-|
|`acude/t02n/dados`|ESP32 → painel|leitura a cada 10 s|
|`acude/t02n/cmd`|painel → ESP32|comandos com token|

**Exemplo real de payload de dados:**

```json
{"id":"acude1","temp":31,"estado":"ALERTA: AGUA QUENTE","aerador":true,"modo":"auto","limite":30,"ts":104}
```

**Exemplos de comando:**

```json
{"token":"<token>","modo":"manual","aerador":true}
{"token":"<token>","modo":"auto"}
{"token":"<token>","limite":31}
```

O ESP32 rejeita comandos sem token válido, JSON inválido, mensagens com mais de 200 bytes e limites fora de 22–35 °C.

## Segurança

* Credenciais fora do repositório (`secrets.h` no `.gitignore`, com `secrets.example.h` como modelo).
* Comunicação criptografada: MQTT sobre TLS (8883) no ESP32 e WSS (8884) no painel.
* Comandos autenticados por token (correção derivada do checklist OWASP IoT Top 10).
* Estado seguro: relé e buzzer iniciam desligados; falha do sensor desliga o aerador.
* Limitação conhecida: `setInsecure()` criptografa, mas não valida o certificado do broker.

## Uso de IA

Ferramentas de IA foram usadas como apoio na definição de componentes, na montagem e na geração de trechos de código. Todo o código foi testado no hardware pelo grupo; os detalhes estão na seção correspondente do relatório.

