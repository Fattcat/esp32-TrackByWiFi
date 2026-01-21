// === VZDELÁVACÍ PROJEKT: WiFi Sniffer + GPS + SD Logging (ESP8266) ===
// Autor: ANDY (vzdelávacie účely)
// Poznámka: ESP8266 má obmedzený sniffer mód – žiadny plný radiotap, ale Probe/Beacon ide

#include <ESP8266WiFi.h>
#include <SD.h>
#include <SPI.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>

// --- PINY ---
#define SD_CS_PIN     15   // D8
#define GPS_RX_PIN    5    // D1 (GPS TX → ESP RX)
#define GPS_TX_PIN    4    // D2 (GPS RX → ESP TX) – nepoužíva sa, ale SoftwareSerial ho vyžaduje

// --- GPS ---
SoftwareSerial gpsSerial(GPS_RX_PIN, GPS_TX_PIN);
TinyGPSPlus gps;
bool hasValidGPS = false;
float lastLat = 0.0, lastLon = 0.0;

// --- SD FILE ---
const char* LOG_FILENAME = "/wifi_log.txt";
#define MAX_LOG_SIZE (5 * 1024 * 1024) // 5 MB (ESP8266 má menej RAM)

// --- DEDUPLIKÁCIA ---
#define HISTORY_SIZE 30
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
  history[histIndex].clientMac = clientMac;
  history[histIndex].ssid = ssid;
  history[histIndex].apMac = apMac;
  history[histIndex].isProbe = isProbe;
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

  if (SD.exists(LOG_FILENAME)) {
    File f = SD.open(LOG_FILENAME, "r");
    if (f.size() > MAX_LOG_SIZE) {
      f.close();
      SD.remove(LOG_FILENAME);
      Serial.println("Log reset (veľkosť).");
    } else {
      f.close();
    }
  }

  File logFile = SD.open(LOG_FILENAME, "a");
  if (logFile) {
    logFile.println(line);
    logFile.flush();
    logFile.close();
  }
}

// ESP8266 sniffer callback
extern "C" void sniffer_callback(uint8_t *buf, uint16_t len) {
  if (len < 24) return;

  uint16_t frame_control = buf[0] | (buf[1] << 8);
  uint8_t subtype = (frame_control >> 4) & 0x0F;

  String timestamp = String(millis());

  if (subtype == 0x04) { // Probe Request
    uint8_t src_mac[6];
    memcpy(src_mac, buf + 10, 6);
    String clientMac = macToString(src_mac);

    const uint8_t *tags = buf + 24;
    String ssid = "";
    uint8_t i = 0;
    while (i < 100 && (i + 2) < (len - 24)) {
      uint8_t tag_num = tags[i];
      if (tag_num == 0) {
        uint8_t len_ssid = tags[i + 1];
        if (len_ssid > 0 && len_ssid <= 32 && (i + 2 + len_ssid) <= (len - 24)) {
          ssid = String((char*)&tags[i + 2], len_ssid);
        }
        break;
      }
      i += 2 + tags[i + 1];
    }

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
    memcpy(bssid, buf + 10, 6);
    String apMac = macToString(bssid);

    // Beacon body: po 24 B idú timestamp (8), interval (2), caps (2) → tagy od 36
    if (len < 38) return;
    const uint8_t *tags = buf + 36;
    String ssid = "";
    uint8_t i = 0;
    while (i < 100 && (i + 2) < (len - 36)) {
      uint8_t tag_num = tags[i];
      if (tag_num == 0) {
        uint8_t len_ssid = tags[i + 1];
        if (len_ssid > 0 && len_ssid <= 32 && (i + 2 + len_ssid) <= (len - 36)) {
          ssid = String((char*)&tags[i + 2], len_ssid);
        }
        break;
      }
      i += 2 + tags[i + 1];
    }

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
  Serial.println("\n=== ESP8266 WiFi Sniffer + GPS Logger (VZDELÁVACIE ÚČELY) ===");

  // SD karta
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("Chyba: SD karta nenájdená!");
    while (1) delay(1000);
  }
  Serial.println("SD karta OK");

  // GPS
  gpsSerial.begin(9600);
  Serial.println("GPS (SoftSerial) inicializované @ 9600");

  // Wi-Fi sniffer
  wifi_set_opmode(STATION_MODE);
  wifi_promiscuous_enable(0);
  wifi_set_promiscuous_rx_cb(sniffer_callback);
  wifi_promiscuous_enable(1);
  Serial.println("Wi-Fi sniffer aktívny");

  hasValidGPS = false;
  lastLat = lastLon = 0.0;
}

void loop() {
  // Čítaj GPS
  while (gpsSerial.available()) {
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

  delay(50); // uvoľni CPU
}
