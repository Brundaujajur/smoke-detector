#include <SPI.h>
#include <LoRa.h>
#include <DHT.h>

// === LoRa Pins ===
#define LORA_SS    4
#define LORA_RST   14
#define LORA_DIO0  2

// === DHT11 ===
#define DHTPIN     15
#define DHTTYPE    DHT11
DHT dht(DHTPIN, DHTTYPE);

// === MQ-2 (LPG) ===
#define MQ2PIN     32

// === Smoke Sensor ===
#define SMOKE_PIN  32

const float RL = 10.0;       // Load resistor in kΩ
const float Vc = 3.3;        // Supply voltage

int packetCounter = 0;

// --- Rs/R0 variables ---
float R0 = 10.0;

// =====================================================
// MQ-2 Calibration
// =====================================================
float calibrateR0(int mqPin, int samples = 50) {

  Serial.println("Calibrating MQ-2 in clean air...");

  float Rs_sum = 0;

  for (int i = 0; i < samples; i++) {

    int raw = analogRead(mqPin);

    float Vout = raw * (Vc / 4095.0);

    // Avoid division by zero
    if (Vout <= 0.01) {
      Vout = 0.01;
    }

    float Rs = RL * ((Vc - Vout) / Vout);

    Rs_sum += Rs;

    delay(100);
  }

  float R0_calibrated = Rs_sum / samples;

  Serial.print("Calibrated R0: ");
  Serial.println(R0_calibrated);

  return R0_calibrated;
}

// =====================================================
// Convert Rs/R0 to LPG PPM
// =====================================================
float getLPGppm(float Rs_R0) {

  float m = -0.45;
  float b = 0.45;

  float logppm = (log10(Rs_R0) - b) / m;

  return pow(10, logppm);
}

// =====================================================
// SETUP
// =====================================================
void setup() {

  Serial.begin(115200);

  while (!Serial);

  dht.begin();

  // ADC configuration
  analogSetAttenuation(ADC_11db);

  // Smoke sensor input
  pinMode(SMOKE_PIN, INPUT);

  Serial.println("\n--- LoRa Weather + Gas + Smoke TX ---");

  // LoRa configuration
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(866E6)) {

    Serial.println("LoRa init failed. Check connections.");

    while (true);
  }

  LoRa.setSyncWord(0xF3);

  Serial.println("LoRa init succeeded!");

  // Auto-calibrate MQ-2
  R0 = calibrateR0(MQ2PIN, 50);
}

// =====================================================
// LOOP
// =====================================================
void loop() {

  delay(2000);

  // ===================================================
  // DHT11
  // ===================================================
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isnan(humidity))
    humidity = 0.0;

  if (isnan(temperature))
    temperature = 0.0;

  // ===================================================
  // MQ-2 LPG SENSOR
  // ===================================================
  int mq2Raw = analogRead(MQ2PIN);

  float Vout = mq2Raw * (Vc / 4095.0);

  if (Vout <= 0.01)
    Vout = 0.01;

  float Rs = RL * ((Vc - Vout) / Vout);

  float Rs_R0 = Rs / R0;

  float lpgPPM = getLPGppm(Rs_R0);

  if (isnan(lpgPPM) || lpgPPM < 0)
    lpgPPM = 0.0;

  // ===================================================
  // SMOKE SENSOR
  // ===================================================
  int smokeRaw = analogRead(SMOKE_PIN);

  // Convert ADC value to voltage
  float smokeVoltage = smokeRaw * (Vc / 4095.0);

  // ===================================================
  // SMOKE LEVEL
  // ===================================================
  String smokeStatus;

  if (smokeRaw < 1000) {
    smokeStatus = "SAFE";
  }
  else if (smokeRaw < 2000) {
    smokeStatus = "LOW";
  }
  else if (smokeRaw < 3000) {
    smokeStatus = "MEDIUM";
  }
  else {
    smokeStatus = "HIGH";
  }

  // ===================================================
  // PREPARE LoRa MESSAGE
  // ===================================================
  String message =
    "Temp: " + String(temperature, 1) + "C | " +
    "Humidity: " + String(humidity, 1) + "% | " +
    "LPG: " + String(lpgPPM, 0) + " ppm | " +
    "Smoke: " + String(smokeRaw) + " | " +
    "SmokeV: " + String(smokeVoltage, 2) + "V | " +
    "Status: " + smokeStatus;

  // ===================================================
  // SEND OVER LoRa
  // ===================================================
  LoRa.beginPacket();

  LoRa.print(message);

  LoRa.endPacket();

  // ===================================================
  // SERIAL MONITOR
  // ===================================================
  Serial.println("--------------------------------");

  Serial.print("Temperature : ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("Humidity    : ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("LPG         : ");
  Serial.print(lpgPPM);
  Serial.println(" ppm");

  Serial.print("Smoke ADC   : ");
  Serial.println(smokeRaw);

  Serial.print("Smoke Volt  : ");
  Serial.print(smokeVoltage);
  Serial.println(" V");

  Serial.print("Smoke Status: ");
  Serial.println(smokeStatus);

  Serial.println("Sent: " + message);

  packetCounter++;
}
