# visualize.py
import re
import folium

LOG_FILE = "wifi_log.txt"
OUTPUT_HTML = "wifi_map.html"

points = []

with open(LOG_FILE, "r", encoding="utf-8") as f:
    content = f.read()

blocks = content.strip().split("--------")
for block in blocks:
    if not block.strip():
        continue

    # Extrahuj typ
    typ_match = re.search(r"TYPE:\s*(\w+)", block)
    if not typ_match:
        continue
    typ = typ_match.group(1)

    # Extrahuj SSID
    ssid_match = re.search(r'SSID:\s*"([^"]*)"', block)
    ssid = ssid_match.group(1) if ssid_match else "Unknown"

    # Extrahuj MAC
    client_mac = re.search(r"CLIENT_MAC:\s*([0-9A-F:]+)", block)
    ap_mac = re.search(r"AP_MAC:\s*([0-9A-F:]+)", block)

    # Extrahuj GPS
    gps_match = re.search(r"GPS:\s*([\-0-9.,]+)", block)
    if not gps_match or gps_match.group(1) == "NoGPS":
        continue

    coords = gps_match.group(1).split(",")
    if len(coords) != 2:
        continue

    try:
        lat, lon = float(coords[0]), float(coords[1])
        if -90 <= lat <= 90 and -180 <= lon <= 180:
            points.append({
                "lat": lat,
                "lon": lon,
                "ssid": ssid,
                "type": typ,
                "client": client_mac.group(1) if client_mac else "",
                "ap": ap_mac.group(1) if ap_mac else ""
            })
    except:
        continue

# Vytvor mapu
if not points:
    print("Žiadne platné GPS body!")
else:
    m = folium.Map(location=[points[0]["lat"], points[0]["lon"]], zoom_start=15)
    for p in points:
        color = "blue" if p["type"] == "Beacon" else "red"
        popup = f"""
        <b>Typ:</b> {p['type']}<br>
        <b>SSID:</b> {p['ssid']}<br>
        <b>Klient:</b> {p['client']}<br>
        <b>AP:</b> {p['ap']}
        """
        folium.CircleMarker(
            location=[p["lat"], p["lon"]],
            radius=6,
            color=color,
            fill=True,
            fill_color=color,
            popup=popup
        ).add_to(m)

    m.save(OUTPUT_HTML)
    print(f"Mapa uložená ako {OUTPUT_HTML}")
