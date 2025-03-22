// -----------------------------------------------------------------------------------------------
// --      Copyright - Juan Francisco Catalina - https://github.com/jufracaqui/todo             --
// -----------------------------------------------------------------------------------------------
// --      EDIT THE FOLLOWING VARIABLES AS NEEDED FOR YOUR TANK/SETUP                           --
// -----------------------------------------------------------------------------------------------

#define DEBUG 0
#define LOOP_INTERVAL 5000  // Milliseconds

#define WIFI_SSID "Panic at the Cisco"
#define WIFI_PASSWORD "Nabucodonosor"

#define TELEGRAM_SEND_STATUS_INTERVAL 3600000    // Send status every 1 hour. Set as 0 to disable
#define TELEGRAM_BOT_TOKEN "1234:Nabucodonosor"  // Create with "BotFather"
#define TELEGRAM_CHAT_ID "1234"                  // Use @myidbot (IDBot) to find your chat ID

// Tank variables (set as 0 to disable)
#define MAX_TEMP_ALLOWED 26.0  // At this temp the cooler will run
#define MIN_TEMP_ALLOWED 23.5  // At this temp the heater will run

#define DHTTYPE DHT11

// -----------------------------------------------------------------------------------------------
// --      IN CASE YOU NEED TO CHANGE PINS TO ANOTHER CONFIGURATION                             --
// -----------------------------------------------------------------------------------------------

// Analog PINS
#define PH_PIN 35   // PH sensor
#define TDS_PIN 34  // TDS sensor

// Digital PINS
#define TEMP_1_PIN 15           // Temp sensor 1
#define DHTPIN 2                // Ambient DHT
#define WATER_LEVEL_PIN 23      // Water level switch
#define RELEE_1_COOLER 5        // Relee 1
#define RELEE_2_HEATER 18       // Relee 2
#define RELEE_3_AUTO_REFILL 19  // Relee 3

// -----------------------------------------------------------------------------------------------
// --      MAKE CHANGES IF YOU KNOW WHAT YOU ARE DOING                                          --
// --      YOU WILL BE RESPONSIBLE IF ANYTHING STOPS WORKING OR BREAKS                          --
// --      THERE'S NOTHING FOR YOUR HERE. ONLY DEATH                                            --
// -----------------------------------------------------------------------------------------------

#include <WiFi.h>
#include <WiFiClientSecure.h>
// https://github.com/jufracaqui/Universal-Arduino-Telegram-Bot
#include <UniversalTelegramBot.h>
// https://github.com/bblanchon/ArduinoJson
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
// https://github.com/adafruit/DHT-sensor-library
#include <DHT.h>
// https://github.com/GreenPonik/DFRobot_ESP_PH_BY_GREENPONIK/tree/master
#include <DFRobot_ESP_PH.h>
#include <EEPROM.h>

// -----------------------------------------------------------------------------------------------

WiFiClientSecure client;
UniversalTelegramBot telegramBot(TELEGRAM_BOT_TOKEN, client);

// -----------------------------------------------------------------------------------------------

OneWire tempWire(TEMP_1_PIN);
DallasTemperature tempSensor(&tempWire);
DHT dht(DHTPIN, DHTTYPE);
DFRobot_ESP_PH phReader;

// -----------------------------------------------------------------------------------------------

#if DEBUG
#define D_SerialBegin(...) Serial.begin(__VA_ARGS__);
#define D_print(...) Serial.print(__VA_ARGS__)
#define D_write(...) Serial.write(__VA_ARGS__)
#define D_println(...) Serial.println(__VA_ARGS__)
#else
#define D_SerialBegin(...)
#define D_print(...)
#define D_write(...)
#define D_println(...)
#endif

// -----------------------------------------------------------------------------------------------

unsigned long bot_lasttime;

void sendTelegramMessage(String message, String parseMode, bool silent) {
  D_print("Sending bot message: \n");
  D_print(message + "\n");
  D_print("Mode: " + parseMode + "\n");

  if (telegramBot.sendMessage(TELEGRAM_CHAT_ID, message, parseMode, 0, silent)) {
    D_println("Message sent");
  } else {
    D_println("Message could not be sent");
  }
}

void handleNewMessages(int numNewMessages) {
  D_print("handleNewMessages ");
  D_println(numNewMessages);

  String answer;
  for (int i = 0; i < numNewMessages; i++) {
    telegramMessage &msg = telegramBot.messages[i];
    D_println("Received " + msg.text);
    if (msg.text == "/help") {
      answer = "Available commands:\n/calibratePH4 (If no value is provided, set the default: 2032.44)\n/calibratePH7 (If no value is provided, set the default: 1500.0)\n/status";
    } else if (msg.text.startsWith("/calibratePH4")) {
      if (msg.text.length() == 13) {
        EEPROM.writeFloat(PHVALUEADDR + sizeof(float), 2032.44);
        EEPROM.commit();
        phReader.begin();
        answer = "*PH4* set as default value.\nIt now reads: 🧪 " + String(calculatePH());
      } else {
        String value = msg.text.substring(14);
        float floatValue = value.toFloat();

        if (floatValue == 0.0) {
          answer = "Wrong value provided: " + value;
        } else {
          EEPROM.writeFloat(PHVALUEADDR + sizeof(float), floatValue);
          EEPROM.commit();
          phReader.begin();
          answer = "*PH4* calibrated.\nIt now reads: 🧪 " + String(calculatePH());
        }
      }
    } else if (msg.text.startsWith("/calibratePH7")) {
      if (msg.text.length() == 13) {
        EEPROM.writeFloat(PHVALUEADDR, 1500.0);
        EEPROM.commit();
        phReader.begin();
        answer = "*PH7* set as default value.\nIt now reads: 🧪 " + String(calculatePH());
      } else {
        String value = msg.text.substring(14);
        float floatValue = value.toFloat();
        if (floatValue == 0.0) {
          answer = "Wrong value provided: " + value;
        } else {
          EEPROM.writeFloat(PHVALUEADDR, floatValue);
          EEPROM.commit();
          phReader.begin();
          answer = "*PH7* calibrated.\nIt now reads: 🧪 " + String(calculatePH());
        }
      }
    } else if (msg.text == "/status") {
      answer = buildStatusMessage();
    } else {
      continue;
    }

    telegramBot.sendMessage(msg.chat_id, answer, "Markdown", false);
  }
}

void bot_setup() {
  const String commands = F("["
                            "{\"command\":\"help\",  \"description\":\"Get bot usage help\"},"
                            "{\"command\":\"calibratePH4\", \"description\":\"Calibrate your PH sensor for PH4\"},"
                            "{\"command\":\"calibratePH7\", \"description\":\"Calibrate your PH sensor for PH7\"},"
                            "{\"command\":\"status\",\"description\":\"Get current status\"}"  // no comma on last command
                            "]");
  telegramBot.setMyCommands(commands);
}

void setup() {
  D_SerialBegin(9600);
  EEPROM.begin(32);

  delay(1000);
  D_println("Connecting to WiFi");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    D_print(".");
  }

  D_print("\n");
  D_println(WiFi.localIP());
  D_print("\n");

  bot_setup();

  sendTelegramMessage("🤖Aquarium connected!🤖", "", false);

  tempSensor.begin();

  pinMode(WATER_LEVEL_PIN, INPUT);

  pinMode(RELEE_1_COOLER, OUTPUT);
  pinMode(RELEE_2_HEATER, OUTPUT);
  pinMode(RELEE_3_AUTO_REFILL, OUTPUT);

  dht.begin();

  phReader.begin();
}

float waterTemperature = 0;
float tds = 0;
float ambientHumidity = 0;
float ambientTemperature = 0;
float ambientHeatIndex = 0;
bool waterLevelOk = true;
float ph = 0;

bool coolerOn = false;
long coolerRunningSince = 0;
long coolerLastOnTime = 0;
bool heaterOn = false;
long heaterRunningSince = 0;
long heaterLastOnTime = 0;
bool refillPumpOn = false;
long refillPumpRunningSince = 0;
long refillPumpLastOnTime = 0;

float calculateWaterTemperature() {
  tempSensor.requestTemperatures();

  return tempSensor.getTempCByIndex(0);
}

float calculateWaterTDS() {
  unsigned long int avgValue;
  int buf[10], temp;

  for (int i = 0; i < 10; i++) {
    buf[i] = analogRead(TDS_PIN);
    delay(10);
  }
  for (int i = 0; i < 9; i++) {  //sort the analog from small to large
    for (int j = i + 1; j < 10; j++) {
      if (buf[i] > buf[j]) {
        temp = buf[i];
        buf[i] = buf[j];
        buf[j] = temp;
      }
    }
  }
  avgValue = 0;
  for (int i = 2; i < 8; i++) avgValue += buf[i];  //take the average value of 6 center sample

  float voltage = (float)avgValue / 6 * 3.3 / 4096;

  float compensationCoefficient = 1.0 + 0.02 * (waterTemperature - 25.0);
  float compensationVoltage = voltage / compensationCoefficient;

  return (133.42 * compensationVoltage * compensationVoltage * compensationVoltage - 255.86 * compensationVoltage * compensationVoltage + 857.39 * compensationVoltage) * 0.5;
}

bool isWaterLevelOk() {
  return digitalRead(WATER_LEVEL_PIN) != HIGH;
}

void updateCoolerState(float waterTemperature) {
  if (MAX_TEMP_ALLOWED == 0) {
    coolerOn = false;
    coolerRunningSince = 0;
    return;
  }

  if (waterTemperature <= 0) {  // Something is wrong with the temp sensor
    coolerOn = false;
    coolerRunningSince = 0;
    return;
  }

  if (waterTemperature >= MAX_TEMP_ALLOWED) {
    coolerOn = true;
    if (coolerRunningSince == 0) {
      coolerRunningSince = millis();
    }
    coolerLastOnTime = millis();
  } else if (waterTemperature < MAX_TEMP_ALLOWED - 1) {  // cool until water is one level below max
    coolerOn = false;
    coolerRunningSince = 0;
  }
}

void updateHeaterState(float waterTemperature) {
  if (MIN_TEMP_ALLOWED == 0) {
    heaterOn = false;
    heaterRunningSince = 0;
    return;
  }

  if (waterTemperature <= 0) {  // Something is wrong with the temp sensor
    heaterOn = false;
    heaterRunningSince = 0;
    return;
  }

  if (waterTemperature <= MIN_TEMP_ALLOWED) {
    heaterOn = true;
    if (heaterRunningSince == 0) {
      heaterRunningSince = millis();
    }
    heaterLastOnTime = millis();
  } else if (waterTemperature > MIN_TEMP_ALLOWED + 0.25) {  // cool until water is 1/4 above max
    heaterOn = false;
    heaterRunningSince = 0;
  }
}

void updateRefillPumpState() {
  if (refillPumpRunningSince > 0 && millis() - refillPumpRunningSince >= 3 * 60 * 1000) {  // If pump has been running for more than 3 mins, stop it
    refillPumpOn = false;
    refillPumpRunningSince = 0;
    return;
  }

  if (millis() - refillPumpLastOnTime < (5 + 3) * 60 * 1000) {  // Wait 5 mins since the last time the pump was running plus the 3 mins it was on until we make it run again
    refillPumpOn = false;
    refillPumpRunningSince = 0;
    return;
  }

  if (waterLevelOk) {
    refillPumpOn = false;
    refillPumpRunningSince = 0;
  } else {
    refillPumpOn = true;
    if (refillPumpRunningSince == 0) {
      refillPumpRunningSince = millis();
    }
    refillPumpLastOnTime = millis();
    sendTelegramMessage("⛲Refill pump ON!⛲", "", false);
  }
}

void checkCoolerHeaterRefillPump() {
  updateCoolerState(waterTemperature);
  if (coolerOn) {
    digitalWrite(RELEE_1_COOLER, HIGH);
    D_print("Cooler ON\n");
  } else {
    digitalWrite(RELEE_1_COOLER, LOW);
    D_print("Cooler OFF\n");
  }
  if (coolerRunningSince > 0) {
    D_print("Cooler has been running for ");
    D_print((millis() - coolerRunningSince) / 1000 / 60);  // minutes
    D_print(" minutes\n");
  } else {
    D_print("Last time cooler was on was ");
    D_print((millis() - coolerLastOnTime) / 1000 / 60);  // minutes
    D_print(" minutes ago\n");
  }

  updateHeaterState(waterTemperature);
  if (heaterOn) {
    digitalWrite(RELEE_1_COOLER, HIGH);
    D_print("Heater ON\n");
  } else {
    digitalWrite(RELEE_1_COOLER, LOW);
    D_print("Heater OFF\n");
  }
  if (heaterRunningSince > 0) {
    D_print("Heater has been running for ");
    D_print((millis() - heaterRunningSince) / 1000 / 60);  // minutes
    D_print(" minutes\n");
  } else {
    D_print("Last time heater was on was ");
    D_print((millis() - heaterLastOnTime) / 1000 / 60);  // minutes
    D_print(" minutes ago\n");
  }

  updateRefillPumpState();
  if (refillPumpOn) {
    digitalWrite(RELEE_1_COOLER, LOW);
    D_print("Refill Pump ON\n");
  } else {
    digitalWrite(RELEE_1_COOLER, HIGH);
    D_print("Refill Pump OFF\n");
  }
  if (refillPumpRunningSince > 0) {
    D_print("Refill pump has been running for ");
    D_print((millis() - refillPumpRunningSince) / 1000 / 60);  // minutes
    D_print(" minutes\n");
  } else {
    D_print("Last time refill pump was on was ");
    D_print((millis() - refillPumpLastOnTime) / 1000 / 60);  // minutes
    D_print(" minutes ago\n");
  }
}

float calculateAmbientHumidity() {
  float h = dht.readHumidity();

  if (isnan(h)) {
    return 0;
  }

  return h;
}

float calculateAmbientTemperature() {
  float t = dht.readTemperature();

  if (isnan(t)) {
    return 0;
  }

  return t;
}

float calculateAmbientHeatIndex() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float hic = dht.computeHeatIndex(t, h, false);
  if (isnan(h) || isnan(t) || isnan(hic)) {
    return 0;
  }

  return hic;
}

float calculatePH() {
  float voltage = analogRead(PH_PIN) / 4096.0 * 3300;
  float phValue = phReader.readPH(voltage, waterTemperature);

  return phValue;
}

long telegramStatusLastSend = 0;

String buildStatusMessage() {
  String message = "🛋️ *Ambient*\n";
  message += "🌡️ Temperature: " + String(ambientTemperature) + " ºC\n";
  message += "🌫️ Humidity: " + String(ambientHumidity) + " %\n";
  message += "🫠 Heat Index: " + String(ambientHeatIndex) + " ºC";
  message += "\n\n";

  message += "🌊 *Tank*\n";
  message += "🌡️ Temperature: " + String(waterTemperature) + " ºC\n";
  message += "🚿 TDS: " + String(tds) + " ppm\n";
  if (waterLevelOk) {
    message += "🛟 Water Level: *OK*\n";
  } else {
    message += "🛟 Water Level: *KO*\n";
  }
  message += "🧪 PH: " + String(ph);
  message += "\n\n";

  if (coolerOn) {
    message += "𖣘💨 Cooler is *ON*<\nIt has been ON for " + String((millis() - coolerRunningSince) / 1000 / 60) + " minutes\n";
  } else if (coolerLastOnTime != 0) {
    message += "𖣘 Cooler is *OFF*\nWas last ON " + String((millis() - coolerLastOnTime) / 1000 / 60) + " minutes ago\n";
  }

  message += "\n";

  if (heaterOn) {
    message += "♨️ Heater is *ON*\nIt has been ON for " + String((millis() - heaterRunningSince) / 1000 / 60) + " minutes\n";
  } else if (heaterRunningSince != 0) {
    message += "♨ Heater is *OFF*\nWas last ON " + String((millis() - heaterRunningSince) / 1000 / 60) + " minutes ago\n";
  }

  message += "\n";

  if (refillPumpOn) {
    message += "🟢 Refill pump is *ON*\nIt has been ON for " + String((millis() - refillPumpRunningSince) / 1000 / 60) + " minutes";
  } else if (refillPumpLastOnTime != 0) {
    message += "🔴 Refill pump is *OFF*\nWas last ON " + String((millis() - refillPumpRunningSince) / 1000 / 60) + " minutes ago\n";
  }

  return message;
}

bool firstLoad = true;

void loop() {
  if (firstLoad) {  // Warm up sensors
    firstLoad = false;
    calculateWaterTemperature();
    calculateWaterTemperature();
    calculateWaterTemperature();
    calculateWaterTDS();
    calculateWaterTDS();
    calculateWaterTDS();
    calculateAmbientHumidity();
    calculateAmbientHumidity();
    calculateAmbientHumidity();
    calculateAmbientTemperature();
    calculateAmbientTemperature();
    calculateAmbientTemperature();
    calculateAmbientTemperature();
    calculateAmbientTemperature();
    calculateAmbientTemperature();
    calculatePH();
    calculatePH();
    calculatePH();
    return;
  }

  if (waterTemperature == 0) {
    waterTemperature = calculateWaterTemperature();
  } else {
    waterTemperature = (waterTemperature + calculateWaterTemperature()) / 2;
  }
  if (tds == 0) {
    tds = calculateWaterTDS();
  } else {
    tds = (tds + calculateWaterTDS()) / 2;
  }
  if (ambientHumidity == 0) {
    ambientHumidity = calculateAmbientHumidity();
  } else {
    ambientHumidity = (ambientHumidity + calculateAmbientHumidity()) / 2;
  }
  if (ambientTemperature == 0) {
    ambientTemperature = calculateAmbientTemperature();
  } else {
    ambientTemperature = (ambientTemperature + calculateAmbientTemperature()) / 2;
  }
  if (ambientHeatIndex == 0) {
    ambientHeatIndex = calculateAmbientHeatIndex();
  } else {
    ambientHeatIndex = (ambientHeatIndex + calculateAmbientHeatIndex()) / 2;
  }
  if (ph == 0) {
    ph = calculatePH();
  } else {
    ph = (ph + calculatePH()) / 2;
  }
  waterLevelOk = isWaterLevelOk();

  D_print("Water Temp: ");
  D_print(waterTemperature);
  D_print(" C\n");

  D_print("TDS: ");
  D_print(tds);
  D_print(" ppm\n");

  D_print("Humedad: ");
  D_print(ambientHumidity);
  D_print(" %\n");
  D_print("Temperatura: ");
  D_print(ambientTemperature);
  D_print(" ºC\n");
  D_print("Índice de calor: ");
  D_print(ambientHeatIndex);
  D_print(" ºC\n");

  if (waterLevelOk) {
    D_print("Water level: OK\n");
  } else {
    D_print("Water level: KO\n");
  }

  checkCoolerHeaterRefillPump();

  D_print("PH: ");
  D_print(ph);
  D_print("\n");

  D_println("------------");

  if (millis() - bot_lasttime > 1000) {
    int numNewMessages = telegramBot.getUpdates(telegramBot.last_message_received + 1);

    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = telegramBot.getUpdates(telegramBot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }

  if (TELEGRAM_SEND_STATUS_INTERVAL != 0 && (telegramStatusLastSend == 0 || millis() - telegramStatusLastSend >= TELEGRAM_SEND_STATUS_INTERVAL)) {
    telegramStatusLastSend = millis();
    sendTelegramMessage(buildStatusMessage(), "Markdown", true);
  }

  delay(LOOP_INTERVAL);
}
