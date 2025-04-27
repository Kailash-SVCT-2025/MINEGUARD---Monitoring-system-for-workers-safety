#include <SPI.h>
#include <LoRa.h>
#include <ESP8266WiFi.h>
#include <EMailSender.h>
#include <ThingSpeak.h>  // ThingSpeak library

// LoRa Pin Definitions
#define LORA_SCK 14  // D5
#define LORA_MISO 12 // D6
#define LORA_MOSI 13 // D7
#define LORA_SS 15   // D8
#define LORA_RST 16  // D0
#define LORA_DIO0 5  // D1

// Thresholds
const float BTEMP_THRESHOLD = 100.0;
const float TEMP_THRESHOLD = 39.0;
const float HUMIDITY_THRESHOLD = 100.0;
const float CO2_THRESHOLD = 1000;
const float NH3_THRESHOLD = 400;
const float FALL_THRESHOLD = 15000;

// WiFi & Email credentials
const char* ssid = "mineguard";
const char* password = "9876543210";
EMailSender emailSend("projectwork56789", "sbxh gjqm xirb djdv");

// ThingSpeak
WiFiClient client;
unsigned long myChannelNumber = 2930851; // e.g., 1234567
const char * myWriteAPIKey = "6KKLM3LGW3VRXFT4";     // e.g., "XYZ123ABC456"

// Setup
void setup() {
  Serial.begin(115200);
  while (!Serial);

  // LoRa setup
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  Serial.println("📡 Starting LoRa Receiver...");
  if (!LoRa.begin(433E6)) {
    Serial.println("❌ LoRa init failed. Check your connections.");
    while (true);
  }
  Serial.println("✅ LoRa Receiver is ready.");

  // WiFi setup
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected to WiFi");

  // ThingSpeak setup
  ThingSpeak.begin(client);
}

// Email function
void sendEmail(const String& subject, const String& message) {
  EMailSender::EMailMessage emailMessage;
  emailMessage.subject = subject;
  emailMessage.message = message;
  EMailSender::Response resp = emailSend.send("kailashsenthilnathan92@gmail.com", emailMessage);

  Serial.print("📧 Email Status: ");
  Serial.println(resp.status);
  Serial.println(resp.code);
  Serial.println(resp.desc);
}

// Main loop
void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.println("📥 Packet received!");

    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }

    Serial.println("📦 Raw Data: " + incoming);

    float bodyTemp, temp, humidity, co2, nh3;
    int ax, ay, az;
    int fireDetected, buttonPressed;

    int parsed = sscanf(incoming.c_str(), "%f,%f,%f,%f,%f,%d,%d,%d,%d,%d",
                        &bodyTemp, &temp, &humidity, &co2, &nh3,
                        &ax, &ay, &az, &fireDetected, &buttonPressed);

    if (parsed == 10) {
      String emailMessage = "🚨 Emergency Alerts:\n";
      bool alertTriggered = false;

      Serial.printf("Body Temp (F): %.2f\n", bodyTemp);
      if (bodyTemp > BTEMP_THRESHOLD) {
        emailMessage += "High Body Temperature: " + String(bodyTemp) + "°F\n";
        alertTriggered = true;
      }

      Serial.printf("Env Temp (C): %.2f\n", temp);
      if (temp > TEMP_THRESHOLD) {
        emailMessage += "High Environmental Temperature: " + String(temp) + "°C\n";
        alertTriggered = true;
      }

      Serial.printf("Humidity: %.2f%%\n", humidity);
      if (humidity > HUMIDITY_THRESHOLD) {
        emailMessage += "High Humidity: " + String(humidity) + "%\n";
        alertTriggered = true;
      }

      Serial.printf("CO2 (ppm): %.2f\n", co2);
      if (co2 > CO2_THRESHOLD) {
        emailMessage += "High CO2 Level: " + String(co2) + " ppm\n";
        alertTriggered = true;
      }

      Serial.printf("NH3 Resistance: %.2f\n", nh3);
      if (nh3 > NH3_THRESHOLD) {
        emailMessage += "High NH3 Level: " + String(nh3) + " ppm\n";
        alertTriggered = true;
      }

      Serial.printf("Acceleration → ax: %d, ay: %d, az: %d\n", ax, ay, az);
      long totalAccel = abs(ax) + abs(ay) + abs(az);
      if (totalAccel > FALL_THRESHOLD) {
        emailMessage += "Fall Detected!\n";
        alertTriggered = true;
      }

      Serial.printf("Fire Detected: %s\n", fireDetected ? "YES" : "NO");
      if (fireDetected) {
        emailMessage += "🔥 FIRE ALERT!\n";
        alertTriggered = true;
      }

      Serial.printf("Emergency Button: %s\n", buttonPressed ? "PRESSED" : "NOT PRESSED");
      if (buttonPressed) {
        emailMessage += "🚨 EMERGENCY BUTTON PRESSED!\n";
        alertTriggered = true;
      }

      // Send Email if any alert
      if (alertTriggered) {
        sendEmail("⚠ Miner Safety Alert!", emailMessage);
      }

      // Upload to ThingSpeak
      ThingSpeak.setField(1, temp);
      ThingSpeak.setField(2, humidity);
      ThingSpeak.setField(3, co2);
      ThingSpeak.setField(4, nh3);
      ThingSpeak.setField(5, bodyTemp);
      ThingSpeak.setField(6, fireDetected);
      
      int statusCode = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
      if (statusCode == 200) {
        Serial.println("✅ Data pushed to ThingSpeak.");
      } else {
        Serial.print("⚠️ ThingSpeak push failed. Code: ");
        Serial.println(statusCode);
      }

    } else {
      Serial.println("❌ Parsing failed. Check data format!");
    }

    Serial.println("─────────────────────────────");
  }
  
}


