// -----------------------------------------------------------------------------------------------
// --      Copyright - Juan Francisco Catalina - https://github.com/jufracaqui/aquabot          --
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
#define MAX_TEMP_ALLOWED 26.0          // At this temp the cooler will run
#define MIN_TEMP_ALLOWED 23.5          // At this temp the heater will run
#define MIN_WATER_LEVEL_ALLOWED 33.0   // At this level the refill pump will run
#define WATER_LEVEL_TOTAL_HEIGHT 34.5  // tank water level max height
#define WATER_LEVEL_SENSOR_DISTANCE 2  // distance from sensor to optimal water level (min 2 cms)

#define PH_CALIBRATION_VALUE 21.02  // PH calibration. Destilled water == 7 PH

#define DHTTYPE DHT11

// -----------------------------------------------------------------------------------------------
// --      IN CASE YOU NEED TO CHANGE PINS TO ANOTHER CONFIGURATION                             --
// -----------------------------------------------------------------------------------------------

// Analog PINS
#define PH_PIN 34   // PH sensor
#define TDS_PIN 35  // TDS sensor

// Digital PINS
#define TEMP_1_PIN 15           // Temp sensor 1
#define TEMP_2_PIN 2            // Temp sensor 2
#define DHTPIN 0                // Ambient DHT
#define WATER_LEVEL_PIN 4       // Water level switch
#define WATER_LEVEL_TRIGGER 16  // Water level sensor trigger
#define WATER_LEVEL_ECHO 17     // Water level sensor echo
#define RELEE_1_COOLER 5        // Relee 1
#define RELEE_2_HEATER 18       // Relee 2
#define RELEE_3_AUTO_REFILL 19  // Relee 3
#define RELEE_4 21              // Relee 4

// -----------------------------------------------------------------------------------------------
// --      MAKE CHANGES IF YOU KNOW WHAT YOU ARE DOING                                          --
// --      YOU WILL BE RESPONSIBLE IF ANYTHING STOPS WORKING OR BREAKS                          --
// --      THERE'S NOTHING FOR YOUR HERE. ONLY DEATH                                            --
// -----------------------------------------------------------------------------------------------

#include <WiFi.h>
#include <WiFiClientSecure.h>
// https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
#include <UniversalTelegramBot.h>
// https://github.com/bblanchon/ArduinoJson
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
// https://github.com/jufracaqui/GravityTDS
#include <GravityTDS.h>
// https://github.com/adafruit/DHT-sensor-library
#include <DHT.h>
#include <NewPing.h>

// -----------------------------------------------------------------------------------------------

WiFiClientSecure client;
UniversalTelegramBot telegramBot(TELEGRAM_BOT_TOKEN, client);

// -----------------------------------------------------------------------------------------------

OneWire tempWire(TEMP_1_PIN);
DallasTemperature tempSensor(&tempWire);
GravityTDS gravityTds;
DHT dht(DHTPIN, DHTTYPE);
NewPing sonar(WATER_LEVEL_TRIGGER, WATER_LEVEL_ECHO, 200);

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

void sendTelegramMessage(String message, String parseMode) {
  D_print("Sending bot message: \n");
  D_print(message + "\n");
  D_print("Mode: " + parseMode + "\n");

  telegramBot.sendMessage(TELEGRAM_CHAT_ID, message, parseMode);
}

void setup() {
  D_SerialBegin(9600);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  D_print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    D_print(".");
  }

  D_print("\n");
  D_println(WiFi.localIP());
  D_print("\n");

  sendTelegramMessage("🤖Aquarium connected!🤖", "");

  tempSensor.begin();

  // TDS sensor
  gravityTds.setPin(TDS_PIN);
  gravityTds.setAref(3.3);
  gravityTds.setAdcRange(1024);  // 4096 for 12bit, 1024 for 10bit
  gravityTds.begin();

  // Water level switch
  pinMode(WATER_LEVEL_PIN, INPUT_PULLUP);

  // Water level sensor
  pinMode(WATER_LEVEL_TRIGGER, OUTPUT);  //pin como salida
  pinMode(WATER_LEVEL_ECHO, INPUT);      //pin como entrada

  // Relee
  pinMode(RELEE_1_COOLER, OUTPUT);
  pinMode(RELEE_2_HEATER, OUTPUT);
  pinMode(RELEE_3_AUTO_REFILL, OUTPUT);
  pinMode(RELEE_4, OUTPUT);

  // Ambient
  dht.begin();
}

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

int calculateWaterTDS(float waterTemperature) {
  gravityTds.setTemperature(waterTemperature);  // compensation
  gravityTds.update();
  return gravityTds.getTdsValue();
}

bool isWaterLevelOk() {
  return digitalRead(WATER_LEVEL_PIN) == HIGH;
}

float calculateWaterLevel(float humidity, float temperature) {
  float echoTime = sonar.ping();
  float vsound = 331.3 + (0.606 * temperature) + (0.0124 * humidity);
  float sensorToWaterDistance = (echoTime / 2.0) * vsound / 10000;

  if (sensorToWaterDistance < 2) {
    sensorToWaterDistance = 2;
  }

  return WATER_LEVEL_TOTAL_HEIGHT + WATER_LEVEL_SENSOR_DISTANCE - sensorToWaterDistance;
}

void updateCoolerState(float waterTemperature) {
  if (MAX_TEMP_ALLOWED == 0) {
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
    coolerOn = false;
    heaterRunningSince = 0;
    return;
  }

  if (waterTemperature <= MIN_TEMP_ALLOWED) {
    heaterOn = true;
    if (coolerRunningSince > 0) {
      heaterRunningSince = millis();
    }
    heaterLastOnTime = millis();
  } else if (waterTemperature > MIN_TEMP_ALLOWED + 0.25) {  // cool until water is 1/4 above max
    heaterOn = false;
    heaterRunningSince = 0;
  }
}

void updateRefillPumpState(bool waterLevelOk, float waterLevel) {
  if (MIN_WATER_LEVEL_ALLOWED == 0 || WATER_LEVEL_TOTAL_HEIGHT == 0 || WATER_LEVEL_SENSOR_DISTANCE == 0) {
    refillPumpOn = false;
    refillPumpRunningSince = 0;
    return;
  }

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
    return;
  }

  if (waterLevel < MIN_WATER_LEVEL_ALLOWED - 1) {  // better safe than sorry when pumping water
    refillPumpOn = true;
    sendTelegramMessage("⛲Refill pump ON!⛲", "");  // water can be a really big problem. Alert about refilling every time
    refillPumpRunningSince = millis();
  } else if (waterLevel >= MIN_WATER_LEVEL_ALLOWED) {
    refillPumpOn = false;
    refillPumpRunningSince = 0;
  }
}

void checkCoolerHeaterRefillPump(float waterTemperature, bool waterLevelOk, float waterLevel) {
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

  updateRefillPumpState(waterLevelOk, waterLevel);
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
  if (isnan(h) || isnan(t) || !isnan(hic)) {
    return 0;
  }

  return hic;
}

float calculatePH() {
  unsigned long int avgValue;
  int buf[10], temp;

  for (int i = 0; i < 10; i++) {
    buf[i] = analogRead(PH_PIN);
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

  float phValue = (float)avgValue * 5.0 / 1024 / 6;  //convert the analog into millivolt
  return -5.70 * phValue + PH_CALIBRATION_VALUE;     //convert the millivolt into pH value
}

long telegramStatusLastSend = 0;

void sendStatusToTelegram(float waterTemperature, int tds, float ambientHumidity, float ambientTemperature, float ambientHeatIndex, bool waterLevelOk, float waterLevel, float ph) {
  String message = "<b>🛋️ Ambient</b>\n";
  message += "<pre>";
  message += "🌡️ Temperature: " + String(ambientTemperature) + " ºC\n";
  message += "🌫️ Humidity: " + String(ambientHumidity) + " %\n";
  message += "🫠 Heat Index: " + String(ambientHeatIndex) + " ºC";
  message += "</pre>\n\n";

  message += "<b>🌊 Water<b>\n";
  message += "<pre>";
  message += "🌡️ Temperature: " + String(waterTemperature) + " ºC\n";
  message += "🚿 TDS: " + String(tds) + " ppm\n";
  message += "🛟 Water Level: " + String(waterLevel) + " cm\n";
  message += "🧪 PH: " + String(ph);
  message += "<pre>\n\n";

  if (coolerOn) {
    message += "𖣘💨 Cooler is <b>ON</b>\nIt has been ON for " + String((millis() - coolerRunningSince) / 1000 / 60) + " minutes\n";
  } else {
    message += "Cooler is <b>OFF</b>\nWas last ON " + String((millis() - coolerLastOnTime) / 1000 / 60) + " minutes ago\n";
  }

  message += "\n";

  if (heaterOn) {
    message += "🔥 Heater is <b>ON</b>\nIt has been ON for " + String((millis() - heaterRunningSince) / 1000 / 60) + " minutes\n";
  } else {
    message += "Heater is <b>OFF</b>\nWas last ON " + String((millis() - heaterRunningSince) / 1000 / 60) + " minutes ago\n";
  }

  message += "\n";

  if (refillPumpOn) {
    message += "🚰 Refill pump is ON\nIt has been ON for " + String((millis() - heaterRunningSince) / 1000 / 60) + " minutes";
  } else {
    message += "Refill pump is OFF\nWas last ON " + String((millis() - heaterRunningSince) / 1000 / 60) + " minutes ago\n";
  }

  sendTelegramMessage(message, "HTML");
}


void loop() {
  float waterTemperature = calculateWaterTemperature();
  int tds = calculateWaterTDS(waterTemperature);
  float ambientHumidity = calculateAmbientHumidity();
  float ambientTemperature = calculateAmbientTemperature();
  float ambientHeatIndex = calculateAmbientHeatIndex();
  bool waterLevelOk = isWaterLevelOk();
  float waterLevel = calculateWaterLevel(ambientHumidity, ambientTemperature);
  float ph = calculatePH();

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
  D_print("Índice de calor: ");
  D_print(ambientHeatIndex);
  D_print(" *C\n");

  if (waterLevelOk) {
    D_print("Water level: OK\n");
  } else {
    D_print("Water level: KO\n");
  }

  D_print("Nivel agua: ");
  D_print(waterLevel);
  D_print(" cm\n");

  checkCoolerHeaterRefillPump(waterTemperature, waterLevelOk, waterLevel);

  D_print("PH: ");
  D_print(ph);
  D_print("\n");

  D_println("------------");

  if (TELEGRAM_SEND_STATUS_INTERVAL != 0 && millis() - telegramStatusLastSend >= TELEGRAM_SEND_STATUS_INTERVAL) {
    sendStatusToTelegram(waterTemperature, tds, ambientHumidity, ambientTemperature, ambientHeatIndex, waterLevelOk, waterLevel, ph);
  }

  delay(LOOP_INTERVAL);
}
