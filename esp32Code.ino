// === VZDELÁVACÍ PROJEKT: WiFi Sniffer + GPS + SD Logging ===
// Platforma: ESP32
// Funkcie: Zachytávanie Probe Request & Beacon, GPS, ukladanie na SD bez duplikátov

#include <WiFi.h>
#include <SD.h>
#include <SPI.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>

// --- PINY ---
#define SD_CS_PIN     5
#define GPS_RX_PIN    16
#define GPS_TX_PIN    17

// --- GPS ---
HardwareSerial gpsSerial(2); // UART2
TinyGPSPlus gps;
bool hasValidGPS = false;
float lastLat = 0.0, lastLon = 0.0;

// --- SD FILE ---
File logFile;
const char* LOG_FILENAME = "/wifi_log.txt";
#define MAX_LOG_SIZE (10 * 1024 * 1024) // 10 MB max

// --- DEDUPLIKÁCIA ---
// Jednoduchý buffer posledných 50 záznamov (možno rozšíriť na hash)
#define HISTORY_SIZE 50
struct Record {
  String clientMac;
  String ssid;
  String apMac;
  bool isProbe;
};
Record history[HISTORY_SIZE];
uint8_t histIndex = 0;
uint8_t histCount = 0;

bool isDuplicate(const String& clientMac, const String& ssid, const String& apMac, bool isProbe) {
  for (uint8_t i = 0; i < histCount; i++) {
    if (history[i].isProbe == isProbe &&
        history[i].clientMac == clientMac &&
        history[i].ssid == ssid &&
        history[i].apMac == apMac) {
      return true;
    }
  }
  // Pridaj nový záznam
  history[histIndex].clientMac = clientMac;
  history[histIndex].ssid = ssid;
  history[histIndex].apMac = apMac;
  history[histIndex].is_probe = isProbe;
  histIndex = (histIndex + 1) % HISTORY_SIZE;
  if (histCount < HISTORY_SIZE) histCount++;
  return false;
}

String macToString(const uint8_t* mac) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

void logToSD(const String& line) {
  if (!SD.exists("/")) return;

  // Skontroluj veľkosť súboru
  if (SD.exists(LOG_FILENAME)) {
    File f = SD.open(LOG_FILENAME, FILE_READ);
    if (f.size() > MAX_LOG_SIZE) {
      f.close();
      SD.remove(LOG_FILENAME);
      Serial.println("Log file reset due to size limit.");
    } else {
      f.close();
    }
  }

  logFile = SD.open(LOG_FILENAME, FILE_APPEND);
  if (logFile) {
    logFile.println(line);
    logFile.flush();
    logFile.close();
  }
}

void sniffer_callback(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;

  const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
  const uint8_t *payload = ppkt->payload;

  uint16_t frame_control = payload[0] | (payload[1] << 8);
  uint8_t subtype = (frame_control >> 4) & 0x0F;

  String timestamp = "";
  {
    unsigned long now = millis();
    timestamp = String(now);
  }

  if (subtype == 0x04) { // Probe Request
    uint8_t src_mac[6];
    memcpy(src_mac, payload + 10, 6);
    String clientMac = macToString(src_mac);

    // Parse SSID z tagov
    const uint8_t *tags = payload + 24;
    String ssid = "";
    uint8_t i = 0;
    while (i < 100) {
      uint8_t tag_num = tags[i];
      if (tag_num == 0) {
        uint8_t len = tags[i + 1];
        if (len > 0 && len <= 32) {
          ssid = String((char*)&tags[i + 2], len);
        }
        break;
      }
      i += 2 + tags[i + 1];
      if (i >= 100) break;
    }

    // V Probe Request nie je AP MAC – použijeme "unknown"
    String apMac = "unknown";

    if (isDuplicate(clientMac, ssid, apMac, true)) return;

    String gpsStr = hasValidGPS ? String(lastLat, 6) + "," + String(lastLon, 6) : "NoGPS";
    String logLine = "--------\n" +
                     timestamp + " | TYPE: Probe | CLIENT_MAC: " + clientMac +
                     " | SSID: \"" + ssid + "\" | AP_MAC: " + apMac +
                     " | GPS: " + gpsStr;

    Serial.println(logLine);
    logToSD(logLine);

  } else if (subtype == 0x08) { // Beacon
    uint8_t bssid[6];
    memcpy(bssid, payload + 10, 6);
    String apMac = macToString(bssid);

    const uint8_t *tags = payload + 36;
    String ssid = "";
    uint8_t i = 0;
    while (i < 100) {
      uint8_t tag_num = tags[i];
      if (tag_num == 0) {
        uint8_t len = tags[i + 1];
        if (len > 0 && len <= 32) {
          ssid = String((char*)&tags[i + 2], len);
        }
        break;
      }
      i += 2 + tags[i + 1];
      if (i >= 100) break;
    }

    // Pre beacon je klient "broadcast" – neukladáme klienta
    String clientMac = "broadcast";

    if (isDuplicate(clientMac, ssid, apMac, false)) return;

    String gpsStr = hasValidGPS ? String(lastLat, 6) + "," + String(lastLon, 6) : "NoGPS";
    String logLine = "--------\n" +
                     timestamp + " | TYPE: Beacon | CLIENT_MAC: " + clientMac +
                     " | SSID: \"" + ssid + "\" | AP_MAC: " + apMac +
                     " | GPS: " + gpsStr;

    Serial.println(logLine);
    logToSD(logLine);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== WiFi Sniffer + GPS Logger (VZDELÁVACIE ÚČELY) ===");

  // --- SD KARTA ---
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("Chyba: SD karta nenájdená!");
    while (1) delay(1000);
  }
  Serial.println("SD karta OK");

  // --- GPS ---
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.println("GPS inicializované (UART2 @ 9600)");

  // --- Wi-Fi SNIFFER ---
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(sniffer_callback);
  Serial.println("Wi-Fi sniffer aktívny");

  hasValidGPS = false;
  lastLat = lastLon = 0.0;
}

void loop() {
  // Čítaj GPS dáta
  while (gpsSerial.available() > 0) {
    if (gps.encode(gpsSerial.read())) {
      if (gps.location.isValid() && gps.location.age() < 2000) {
        lastLat = gps.location.lat();
        lastLon = gps.location.lng();
        hasValidGPS = true;
      } else {
        hasValidGPS = false;
      }
    }
  }

  // Ak žiadne dáta, GPS sa stále snaží
  delay(100);
}
