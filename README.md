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

 ## devices
  - esp32 or esp8266
  - neo6m
  - SD card with SD card module
