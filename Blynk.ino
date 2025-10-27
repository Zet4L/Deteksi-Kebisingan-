#define BLYNK_TEMPLATE_ID "TMPL6lbfIVY3Q"
#define BLYNK_TEMPLATE_NAME "Monitoring Kebisingan"
#define BLYNK_AUTH_TOKEN "zSfUK0A7oswGT1dmxBPjef_mNwIYpQ8j"

#include <WiFi.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <BlynkSimpleEsp32.h>


// ================== Konfigurasi WiFi & Blynk =====================
const char* ssid = "Zet4L";
const char* password = "Salman123";

char auth[] = "zSfUK0A7oswGT1dmxBPjef_mNwIYpQ8j"; // Token dari Blynk

// ================== Konfigurasi CallMeBot =======================
String phoneNumber = "6282350446130";  // Nomor WhatsApp 
String apiKey = "b5f7grK2d3e8";             // API Key dari CallMeBot

// ================== LCD dan Sensor ========================
LiquidCrystal_I2C lcd(0x27, 16, 2);
const int soundSensorPin = 34;

bool sudahKirimPeringatan = false;
bool sudahKirimTinggi = false;

unsigned long lastCheck = 0;
const unsigned long checkInterval = 1000;

unsigned long lastSentTime = 0;
const unsigned long kirimInterval = 30000; // 30 Detik

// ================== Statistik ==========================
unsigned long totalSentPackets = 0;
unsigned long totalLostPackets = 0;
unsigned long totalLatency = 0;

// ================== Fungsi Membership Fuzzy =====================
float muTenang(float x) {
  if (x <= 50) return 1.0;
  else if (x < 55) return (55.0 - x) / 5.0;
  else return 0.0;
}

float muPeringatan(float x) {
  if (x <= 50 || x >= 70) return 0.0;
  else if (x <= 60) return (x - 50) / 10.0;
  else return (70 - x) / 10.0;
}

float muTinggi(float x) {
  if (x <= 65) return 0.0;
  else if (x < 70) return (x - 65) / 5.0;
  else return 1.0;
}

// ================== Kirim WhatsApp ==============================
/*void kirimWhatsApp(String pesan) {
 .GET();
    http.end(); if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.callmebot.com/whatsapp.php?phone=" + phoneNumber +
                 "&text=" + urlencode(pesan) + "&apikey=" + apiKey;
    http.begin(url);
    http
  }
}*/
// ================== Kirim WhatsApp via TextMeBot ==============================
void kirimWhatsApp(String pesan) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.textmebot.com/send.php?recipient=" + phoneNumber +
                 "&apikey=" + apiKey + "&text=" + urlencode(pesan);
    http.begin(url);
    int httpResponseCode = http.GET();
    Serial.println("Status kirim WA: " + String(httpResponseCode));
    http.end();
  }
}


// ================== Encode URL ==============================
String urlencode(String str) {
  String encoded = "";
  char c;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c)) {
      encoded += c;
    } else {
      char code0 = (c >> 4) & 0xf;
      char code1 = c & 0xf;
      code0 += (code0 > 9) ? 'A' - 10 : '0';
      code1 += (code1 > 9) ? 'A' - 10 : '0';
      encoded += '%';
      encoded += code0;
      encoded += code1;
    }
  }
  return encoded;
}

// ================== Setup ==============================
void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0); lcd.print("Menghubungkan WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("WiFi Terhubung");

  Blynk.begin(auth, ssid, password);
  delay(1000);
}

// ================== Loop ==============================
void loop() {
  Blynk.run();
  unsigned long now = millis();
  if (now - lastCheck >= checkInterval) {
    lastCheck = now;

    int sensorValue = analogRead(soundSensorPin);
    float db = -0.000007 * sensorValue * sensorValue + 0.0611 * sensorValue - 41.433;

    float μTenang = muTenang(db);
    float μPeringatan = muPeringatan(db);
    float μTinggi = muTinggi(db);

    String kondisi = "Tidak Diketahui";

    if (μTenang > μPeringatan && μTenang > μTinggi) {
      kondisi = "Tenang";
      sudahKirimPeringatan = false;
      sudahKirimTinggi = false;
    } else if (μPeringatan > μTenang && μPeringatan > μTinggi) {
      kondisi = "Peringatan";
      if (!sudahKirimPeringatan && (now - lastSentTime >= kirimInterval)) {
        kirimWhatsApp("⚠️ Peringatan! Suara mencapai " + String(db, 1) + " dB");
        sudahKirimPeringatan = true;
        sudahKirimTinggi = false;
        lastSentTime = now;
      }
    } else if (μTinggi > μTenang && μTinggi > μPeringatan) {
      kondisi = "Tinggi";
      if (!sudahKirimTinggi && (now - lastSentTime >= kirimInterval)) {
        kirimWhatsApp("🚨 Kebisingan Tinggi! Suara mencapai " + String(db, 1) + " dB");
        sudahKirimTinggi = true;
        sudahKirimPeringatan = false;
        lastSentTime = now;
      }
    }

    // Statistik Latency dan Packet Loss
    totalSentPackets = 0;
    totalLostPackets = 0;
    totalLatency = 0;

    for (int i = 0; i < 10; i++) {
      unsigned long start = millis();

      Blynk.virtualWrite(V0, db);
      Blynk.virtualWrite(V1, kondisi);

      unsigned long end = millis();

      if (WiFi.status() == WL_CONNECTED) {
        totalSentPackets++;
        totalLatency += (end - start);
      } else {
        totalLostPackets++;
      }
      delay(100); // jeda antar kiriman
    }

    float averageLatency = (totalSentPackets > 0) ? (totalLatency / (float)totalSentPackets) : 0;
    float packetLoss = ((totalSentPackets + totalLostPackets) > 0) ?
                       (totalLostPackets / (float)(totalSentPackets + totalLostPackets)) * 100 : 0;

    // LCD
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("dB: "); lcd.print(db, 1);
    lcd.setCursor(0, 1); lcd.print("Kond: "); lcd.print(kondisi);

    // Serial Monitor
    Serial.printf("Kondisi: %s | dB: %.1f | Latency: %.2f ms | Packet Loss: %.2f%%\n",
                  kondisi.c_str(), db, averageLatency, packetLoss);
  }
}

