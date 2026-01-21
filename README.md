# esp32-TrackByWiFi

## ! FOR EDUCATIONAL PURPOSES ONLY !
esp32 code for capture prob req or beacon from router, save to sd card, make python map to visualize

## what AM I created for ?

- scanning probe reqs & beacons from routers,
- if GPS is available, then use it for GPS creds captured WiFi data to visualize GPS points on OpenStreet MAP using python
- You will see who has their WiFi turned ON, so it means who is constantly probing their previously connected WiFi routers like:
  - MyHome,
  - Home,
  - Familly WiFi,
  - ...

- so those credentials will be captured, saved and then displayed on PC Map via python with around ~5 meter GPS accuracy (depends on location and signal strenght).

# connection
## SD Card -> esp32
  - VCC -> 3.3v or VCC
  - GND -> GND
  - D23 -> MOSI
  - D21 -> MISO
  - D18 -> SCK
  - D5 -> CS
## neo6m -> esp32
  - here is used UART2 (not UART0)
  - VCC -> 3.3v or VCC
  - GND -> GND
  - TX -> GPIO16 (UART2 RX)
  - RX -> GPIO17 (UART2 TX) 

## esp8266 -> SD card
  - VCC -> 3.3v or VCC
  - GND -> GND
  - D7 -> MOSI
  - D6 -> MISO
  - D5 -> SCK
  - D8 -> CS
## neo6m -> esp8266
  - here is used UART0 !
  - VCC -> 3.3v or VCC
  - GND -> GND
  - TX ─> D1 (GPIO5) 
  - RX -> D2 (GPIO4)

## What I Reccommend ?
  - USE Battery powered like with 3.7V 18650 2500mah, booster to 5.1V as power supply for esp32, second "regulator" for constant 3.3V for neo6m (or maybe also for SD module).
  - if it resets by itself, it means, you have bad power supply or high resistance in Ohm probably on your cable.
  - or Try connect 100–470 µF capacitor between 3.3V and GND -> This can help to make it more stable.
 ## devices
  - esp32 or esp8266
  - neo6m
  - SD card with SD card module
