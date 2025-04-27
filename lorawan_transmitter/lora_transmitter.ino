#include <ESP8266WiFi.h>
#include <espnow.h>
#include <LoRa.h>

// LoRa Pins
#define SS    15
#define RST   16
#define DIO0   2

// Threshold Constants
const float BTEMP_THRESHOLD = 100.0;
const float TEMP_THRESHOLD = 39.0;
const float HUMIDITY_THRESHOLD = 100.0;
const float CO2_THRESHOLD = 1000;
const float NH3_THRESHOLD = 250;
const float FALL_THRESHOLD = 15000;

// Structure of incoming ESP-NOW data
typedef struct struct_message {
  float bodyTemp;
  float temperature;
  float humidity;
  float co2;
  float nh3;
  int16_t ax, ay, az;
  bool fireDetected;
  bool buttonPressed;
} struct_message;

struct_message latestData;
bool newDataAvailable = false;

// ESP-NOW receive callback
void OnDataRecv(uint8_t *mac, uint8_t *incomingDataRaw, uint8_t len) {
  if (len == sizeof(struct_message)) {
    memcpy(&latestData, incomingDataRaw, sizeof(latestData));
    newDataAvailable = true;
  } else {
    Serial.println("❌ Received data size mismatch!");
  }
}

// Send formatted data via LoRa
void sendLoRaPacket(struct_message data) {
  String payload = String(data.bodyTemp, 1) + "," +
                   String(data.temperature, 1) + "," +
                   String(data.humidity, 1) + "," +
                   String(data.co2, 1) + "," +
                   String(data.nh3, 1) + "," +
                   String(data.ax) + "," + String(data.ay) + "," + String(data.az) + "," +
                   (data.fireDetected ? "1" : "0") + "," +
                   (data.buttonPressed ? "1" : "0");
  


  Serial.println("📡 Sending via LoRa:");
  Serial.println(payload);

  LoRa.beginPacket();
  LoRa.print(payload);
  LoRa.endPacket();
}

void setup() {
  Serial.begin(115200);
  Serial.println("🔌 Receiver Starting...");

  // Setup WiFi as Station for ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Init ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("❌ ESP-NOW Init Failed");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);

  // Init LoRa
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("❌ LoRa Init Failed");
    while (1);
  }

  Serial.println("✅ ESP-NOW and LoRa Initialized");
}

void loop() {
  if (newDataAvailable) {
    Serial.println("📥 New ESP-NOW Data:");
    Serial.print("🌡 Body Temp: "); Serial.println(latestData.bodyTemp);
    Serial.print("🔥 Temp: "); Serial.println(latestData.temperature);
    Serial.print("💧 Humidity: "); Serial.println(latestData.humidity);
    Serial.print("🌿 CO2: "); Serial.println(latestData.co2);
    Serial.print("☠ NH3: "); Serial.println(latestData.nh3);
    Serial.print("📉 Accel X/Y/Z: ");
    Serial.print(latestData.ax); Serial.print(", ");
    Serial.print(latestData.ay); Serial.print(", ");
    Serial.println(latestData.az);
    Serial.print("🔥 Fire: "); Serial.println(latestData.fireDetected ? "Yes" : "No");
    Serial.print("🚨 Button: "); Serial.println(latestData.buttonPressed ? "Pressed" : "Not Pressed");
    Serial.println("================================");

    sendLoRaPacket(latestData);  // Send regular data

    // 🚨 Check for alerts
    String alertMsg = "";

    if (latestData.bodyTemp > BTEMP_THRESHOLD) {
      alertMsg += "⚠️ High Body Temp!\n";
    }
    if (latestData.temperature > TEMP_THRESHOLD) {
      alertMsg += "⚠️ High Temperature!\n";
    }
    if (latestData.humidity > HUMIDITY_THRESHOLD) {
      alertMsg += "⚠️ High Humidity!\n";
    }
    if (latestData.co2 > CO2_THRESHOLD) {
      alertMsg += "⚠️ High CO2 Level!\n";
    }
    if (latestData.nh3 > NH3_THRESHOLD) {
      alertMsg += "⚠️ High NH3 Level!\n";
    }

    long totalAccel = abs(latestData.ax) + abs(latestData.ay) + abs(latestData.az);
    if (totalAccel > FALL_THRESHOLD) {
      alertMsg += "🛑 Possible Fall Detected!\n";
    }

    if (latestData.fireDetected) {
      alertMsg += "🔥 Fire Detected!\n";
    }

    if (latestData.buttonPressed) {
      alertMsg += "🚨 Emergency Button Pressed!\n";
    }

    // Send alert via LoRa if any
    if (alertMsg != "") {
      Serial.println("🚨 Sending ALERT via LoRa:");
      Serial.println(alertMsg);

      LoRa.beginPacket();
      LoRa.print("ALERT:\n" + alertMsg);
      LoRa.endPacket();
    }

    newDataAvailable = false;
  }
}
