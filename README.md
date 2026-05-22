Thermostat_esp32s3

🇫🇷 Thermostat connecté basé sur ESP32-S3 avec interface web embarquée et intégration domotique.
🇬🇧 Smart ESP32-S3 thermostat with embedded web interface and home automation integration.

---

🇫🇷 Français

✨ Fonctionnalités

- 🌡️ Mesure température / humidité
- 📶 Connexion Wi-Fi
- 🌐 Interface Web responsive
- 🔥 Contrôle chauffage / ventilation
- 📡 MQTT compatible Home Assistant
- ⚡ OTA Update
- 🧠 Régulation intelligente (hystérésis)
- 📺 Support écran OLED/TFT
- 🔒 Fonctionnement local sans cloud

---

🛠️ Matériel

Carte principale

- ESP32-S3

Capteurs compatibles

- DS18B20
- SHT31
- BME280
- DHT22

Sorties

- Relais chauffage
- SSR
- Ventilation

---

📂 Structure du projet

Thermostat_esp32s3/
├── src/               # Code principal
├── include/           # Headers
├── lib/               # Librairies
├── data/              # Interface Web / SPIFFS
├── docs/              # Documentation
├── test/              # Tests
├── platformio.ini
└── README.md

---

⚙️ Environnement

Projet développé avec :

- PlatformIO
- Arduino Framework
- FreeRTOS
- AsyncWebServer
- MQTT

---

🚀 Installation

Cloner le dépôt

git clone https://github.com/tlmtcg/Thermostat_esp32s3.git

Compiler

pio run

Flasher

pio run --target upload

Monitor série

pio device monitor

---

🔧 Configuration Wi-Fi

const char* ssid = "Votre_SSID";
const char* password = "Votre_MotDePasse";

---

🏠 Home Assistant

Exemple MQTT :

climate:
  - platform: mqtt
    name: "ESP32 Thermostat"

---

🌐 Interface Web

Fonctionnalités :

- température temps réel
- réglage consigne
- état relais
- OTA firmware
- configuration réseau

---

🧪 Roadmap

- [ ] Scheduler hebdomadaire
- [ ] Historique températures
- [ ] Support Matter
- [ ] API REST
- [ ] Dashboard avancé
- [ ] Gestion multi-zones

---

🤝 Contribution

Les contributions sont les bienvenues.

1. Fork
2. Nouvelle branche
3. Commit
4. Pull Request

---

🇬🇧 English

✨ Features

- 🌡️ Temperature / humidity monitoring
- 📶 Wi-Fi connectivity
- 🌐 Responsive web interface
- 🔥 Heating / ventilation control
- 📡 MQTT Home Assistant integration
- ⚡ OTA firmware updates
- 🧠 Smart regulation (hysteresis)
- 📺 OLED/TFT display support
- 🔒 Local-first operation (no cloud required)

---

🛠️ Hardware

Main board

- ESP32-S3

Supported sensors

- DS18B20
- SHT31
- BME280
- DHT22

Outputs

- Heating relay
- SSR
- Ventilation

---

📂 Project structure

Thermostat_esp32s3/
├── src/
├── include/
├── lib/
├── data/
├── docs/
├── test/
├── platformio.ini
└── README.md

---

⚙️ Environment

Built with :

- PlatformIO
- Arduino Framework
- FreeRTOS
- AsyncWebServer
- MQTT

---

🚀 Installation

Clone repository

git clone https://github.com/tlmtcg/Thermostat_esp32s3.git

Build firmware

pio run

Upload firmware

pio run --target upload

Serial monitor

pio device monitor

---

🔧 Wi-Fi configuration

const char* ssid = "Your_SSID";
const char* password = "Your_Password";

---

🏠 Home Assistant

MQTT example :

climate:
  - platform: mqtt
    name: "ESP32 Thermostat"

---

🌐 Web Interface

Available features :

- real-time temperature
- setpoint control
- relay status
- OTA firmware update
- network configuration

---

🧪 Roadmap

- [ ] Weekly scheduler
- [ ] Temperature history
- [ ] Matter support
- [ ] REST API
- [ ] Advanced dashboard
- [ ] Multi-zone support

---

🤝 Contributing

Contributions are welcome.

1. Fork the project
2. Create a branch
3. Commit changes
4. Open a Pull Request

---

📜 License

MIT License

---

👨‍💻 Author

Developed by tlmtcg.