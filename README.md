# aquabot

![aquabot](misc/aquabot.png)

Aquarium bot for checking parameters and automation

---

Configure your `AquaBot` and upload it to an ESP32 microcontroller.

`AquaBot` will connect to you WIFI and send messages to a Telegram bot bot data logging.

If `DEBUG` is set to `1`, many logs will be sent to Serial output at `9600` baud rate.

```c
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
```

It can send notifications to a Telegram bot and listen to commands

* `/help` -> Get Aquabot Help
* `/calibratePH4` -> Set the milivolts for PH4 readings
* `/calibratePH7` -> Set the milivolts for PH7 reading
* `/calibrateTemperature` -> Set the current temperature for calibration
* `/status` -> Get current status for the tank

---

PIN connections are as follow. Change them to your needs.

```c
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
```
