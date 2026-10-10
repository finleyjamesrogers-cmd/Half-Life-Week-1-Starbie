# Arduino IDE setup
1. In Arduino IDE, open File → Preferences and add this to Additional Boards Manager URLs:
```
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```
2. Open Tools → Board → Boards Manager, search for esp32, and install esp32 by Espressif Systems.

3. Select Tools → Board → esp32 → XIAO_ESP32C3 and select the board's port under Tools → Port.

4. Open Sketch → Include Library → Manage Libraries and install:

    - Adafruit GFX Library

    - Adafruit SSD1306

    - Adafruit MPU6050

    - DHT sensor library

If Arduino asks to install dependencies, choose Install All.

5. Open Starbie.ino, press the arrow-shaped Upload button, and wait for the upload to finish.

Seeed's XIAO ESP32-C3 setup guide also confirms selecting XIAO_ESP32C3 from the ESP32 Arduino board list. Seeed Studio guide
