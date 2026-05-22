Thermostat_esp32s3

🇫🇷 Thermostat intelligent basé sur ESP32-S3 utilisant ESP-IDF avec architecture modulaire et intégration domotique.
🇬🇧 Smart ESP32-S3 thermostat built with ESP-IDF featuring a modular architecture and home automation integration.

---

🇫🇷 Français

✨ Fonctionnalités

- 🌡️ Gestion température / humidité
- 📶 Wi-Fi embarqué
- 🌐 Interface Web locale
- 🔥 Contrôle chauffage / ventilation
- 📡 MQTT / Home Assistant
- ⚡ OTA Update
- 🧠 Régulation intelligente
- 🧩 Architecture modulaire ESP-IDF
- 🔒 Fonctionnement local sans cloud

---

🏗️ Architecture du projet

Thermostat_esp32s3/
├── components/        # Modules ESP-IDF
├── main/              # Point d’entrée application
├── unity-app/         # Tests unitaires
├── weather_test/      # Tests météo / expérimentation
├── .vscode/
├── CMakeLists.txt
├── partitions.csv
├── dependencies.lock
└── README.md

---

🧩 Organisation des composants

Le projet utilise l’architecture modulaire ESP-IDF :

- chaque fonctionnalité est isolée dans "components/"
- séparation claire des responsabilités
- meilleure maintenabilité
- simplifie les tests et évolutions

Exemples possibles :

- Wi-Fi manager
- MQTT client
- thermostat controller
- sensors
- web server
- OTA manager

---

⚙️ Environnement de développement

Framework

- ESP-IDF

Cible

- ESP32-S3

Outils

- ESP-IDF Toolchain
- CMake
- Ninja
- VSCode

---

🚀 Installation

1. Cloner le dépôt

git clone https://github.com/tlmtcg/Thermostat_esp32s3.git

---

2. Configurer ESP-IDF

idf.py set-target esp32s3

---

3. Compiler

idf.py build

---

4. Flasher le firmware

idf.py flash

---

5. Monitor série

idf.py monitor

---

📡 Fonctionnalités réseau

- Wi-Fi STA/AP
- MQTT
- OTA firmware update
- Interface Web locale

---

🧪 Tests

Le projet inclut :

- "unity-app/"
  
  - tests unitaires
  - validation composants

- "weather_test/"
  
  - expérimentation API météo
  - intégration données externes

---

🗂️ Partitionnement mémoire

Le fichier "partitions.csv" définit :

- firmware principal
- OTA slots
- NVS
- SPIFFS/LittleFS

---

🧠 Objectifs du projet

- thermostat autonome
- architecture robuste
- faible latence
- modularité
- extensibilité
- intégration domotique moderne

---

🧪 Roadmap

- [ ] Dashboard Web avancé
- [ ] Historique température
- [ ] Support Matter
- [ ] Multi-zones
- [ ] Scheduler hebdomadaire
- [ ] API REST
- [ ] Statistiques énergétiques

---

🤝 Contribution

Les contributions sont les bienvenues.

1. Fork du projet
2. Création d’une branche
3. Commit des modifications
4. Pull Request

---

🇬🇧 English

✨ Features

- 🌡️ Temperature / humidity management
- 📶 Embedded Wi-Fi
- 🌐 Local web interface
- 🔥 Heating / ventilation control
- 📡 MQTT / Home Assistant integration
- ⚡ OTA updates
- 🧠 Smart regulation
- 🧩 Modular ESP-IDF architecture
- 🔒 Local-first operation

---

🏗️ Project architecture

Thermostat_esp32s3/
├── components/        # ESP-IDF modules
├── main/              # Main application entry point
├── unity-app/         # Unit tests
├── weather_test/      # Weather experiments
├── .vscode/
├── CMakeLists.txt
├── partitions.csv
├── dependencies.lock
└── README.md

---

🧩 Component organization

The project follows a modular ESP-IDF architecture :

- isolated feature modules
- clean responsibility separation
- easier maintenance
- scalable firmware structure

Possible modules :

- Wi-Fi manager
- MQTT client
- thermostat controller
- sensors
- web server
- OTA manager

---

⚙️ Development environment

Framework

- ESP-IDF

Target

- ESP32-S3

Tools

- ESP-IDF Toolchain
- CMake
- Ninja
- VSCode

---

🚀 Installation

1. Clone repository

git clone https://github.com/tlmtcg/Thermostat_esp32s3.git

---

2. Configure ESP-IDF

idf.py set-target esp32s3

---

3. Build firmware

idf.py build

---

4. Flash firmware

idf.py flash

---

5. Serial monitor

idf.py monitor

---

📡 Network features

- Wi-Fi STA/AP
- MQTT
- OTA updates
- Local web interface

---

🧪 Testing

Included :

- unit testing
- component validation
- weather experimentation

---

🗂️ Memory partitioning

"partitions.csv" defines :

- main firmware
- OTA slots
- NVS
- SPIFFS/LittleFS

---

🧠 Project goals

- autonomous thermostat
- robust architecture
- low latency
- modular firmware
- extensibility
- modern home automation integration

---

📜 License

MIT License

---

👨‍💻 Author

Developed by tlmtcg.