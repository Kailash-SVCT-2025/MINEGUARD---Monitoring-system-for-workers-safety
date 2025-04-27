#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Wire.h>
#include <MPU6050.h>
#include <DHT.h>
#include <MQ135.h>

// Define pins
#define DHTPIN D4
#define DHTTYPE DHT11
#define MQ135_PIN A0
#define FLAME_SENSOR_PIN D0
#define BUTTON_PIN D7
#define LM35_PIN A0
#define BUZZER_PIN D3
#define SCL_PIN D1
#define SDA_PIN D2

// Thresholds
const float BTEMP_THRESHOLD = 120.0;
const float TEMP_THRESHOLD = 39.0;
const float HUMIDITY_THRESHOLD = 100.0;
const float CO2_THRESHOLD = 1000;
const float NH3_THRESHOLD = 1000;
const float FALL_THRESHOLD = 15000;

// Sensor objects
DHT dht(DHTPIN, DHTTYPE);
MQ135 mq135(MQ135_PIN);
MPU6050 mpu;
bool mpuConnected = false;

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

struct_message dataToSend;

uint8_t receiverMAC[] = {0x40, 0x91, 0x51, 0x55, 0x9E, 0xFE};

void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("Send Status: ");
  Serial.println(sendStatus == 0 ? "Success" : "Failed");
}

float readLM35() {
  int analogValue = analogRead(LM35_PIN);
  float voltage = analogValue * (3.3 / 1023.0);
  float temperatureC = voltage * 100.0;
  return (temperatureC * 4.0 / 6.0) + 60.0 ; // Adjusted
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  dht.begin();
  mpu.initialize();

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(FLAME_SENSOR_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  if (mpu.testConnection()) {
    Serial.println("✅ MPU6050 connected");
    mpuConnected = true;
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != 0) {
    Serial.println("❌ ESP-NOW init failed");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(OnDataSent);
  esp_now_add_peer(receiverMAC, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);

  Serial.println("🚀 Safety Node Started");
}

void loop() {
  bool alert = false;

  dataToSend.bodyTemp = readLM35();
  dataToSend.temperature = dht.readTemperature();
  dataToSend.humidity = dht.readHumidity();
  dataToSend.co2 = mq135.getPPM();
  dataToSend.nh3 = mq135.getResistance();
  dataToSend.fireDetected = digitalRead(FLAME_SENSOR_PIN) == LOW;
  dataToSend.buttonPressed = digitalRead(BUTTON_PIN) == HIGH;

  if (mpuConnected) {
    mpu.getAcceleration(&dataToSend.ax, &dataToSend.ay, &dataToSend.az);
  } else {
    dataToSend.ax = dataToSend.ay = dataToSend.az = 0;
  }

  // Print values to Serial Monitor
  Serial.println("---------- Sensor Data ----------");
  Serial.print("Body Temp: "); Serial.println(dataToSend.bodyTemp);
  Serial.print("DHT Temp: "); Serial.println(dataToSend.temperature);
  Serial.print("Humidity: "); Serial.println(dataToSend.humidity);
  Serial.print("CO2: "); Serial.println(dataToSend.co2);
  Serial.print("NH3 Resistance: "); Serial.println(dataToSend.nh3);
  Serial.print("Accel X: "); Serial.println(dataToSend.ax);
  Serial.print("Accel Y: "); Serial.println(dataToSend.ay);
  Serial.print("Accel Z: "); Serial.println(dataToSend.az);
  Serial.print("Fire Detected: "); Serial.println(dataToSend.fireDetected);
  Serial.print("Button Pressed: "); Serial.println(dataToSend.buttonPressed);
  Serial.println("----------------------------------");

  // Alert conditions
  if (dataToSend.bodyTemp > BTEMP_THRESHOLD) {
    Serial.println("⚠️ High Body Temperature Detected!");
    alert = true;
  }
  if (dataToSend.temperature > TEMP_THRESHOLD) {
    Serial.println("⚠️ High Temperature Detected!");
    alert = true;
  }
  if (dataToSend.humidity > HUMIDITY_THRESHOLD) {
    Serial.println("⚠️ High Humidity Detected!");
    alert = true;
  }
  if (dataToSend.co2 > CO2_THRESHOLD) {
    Serial.println("⚠️ High CO2 Level Detected!");
    alert = true;
  }
  if (dataToSend.nh3 > NH3_THRESHOLD) {
    Serial.println("⚠️ High NH3 Level Detected!");
    alert = true;
  }

  int fallValue = abs(dataToSend.ax) + abs(dataToSend.ay) + abs(dataToSend.az);
  if (fallValue > FALL_THRESHOLD) {
    Serial.println("⚠️ Fall Detected!");
    alert = true;
  }

  if (dataToSend.fireDetected) {
    Serial.println("🔥 Fire Detected!");
    alert = true;
  }

  if (dataToSend.buttonPressed) {
    Serial.println("🆘 Emergency Button Pressed!");
    alert = true;
  }

  // Buzzer control
  digitalWrite(BUZZER_PIN, alert ? HIGH : LOW);

  // Send data
  esp_now_send(receiverMAC, (uint8_t *)&dataToSend, sizeof(dataToSend));
  delay(2000);
}
