# 🌱 Smart Agriculture Rover

An IoT-based Smart Agriculture Rover designed to monitor environmental and soil conditions using an ESP8266 NodeMCU and multiple sensors.

## 🚀 Project Overview

The Smart Agriculture Rover collects real-time agricultural data and displays it through a web-based dashboard. It uses multiple sensors to monitor soil moisture, temperature, humidity, light intensity, and rainfall.

## 🛠️ Hardware Components

- ESP8266 NodeMCU V3
- ADS1115 16-bit ADC
- DHT22 Temperature and Humidity Sensor
- BH1750 Light Intensity Sensor
- Soil Moisture Sensors (2)
- Rain Sensor
- 16×2 I2C LCD Display

## 💻 Software & Technologies

- Arduino IDE
- Embedded C/C++
- ESP8266 Wi-Fi
- HTML, CSS and JavaScript (web dashboard)

## 📊 Features

- Real-time environmental monitoring
- Dual soil moisture monitoring
- Rainfall detection
- Light intensity measurement
- Web-based monitoring dashboard
- JSON data endpoint at `/data`
- LCD-based sensor readings

## 📁 Project Structure

```text
smart-agriculture-rover/
├── README.md
├── .gitignore
├── docs/
│   ├── circuit_diagram.png
│   └── dashboard_preview.jpg
└── firmware/
    └── smart_agri_rover/
        └── smart_agri_rover.ino
```

## ⚙️ Setup

1. Install the Arduino IDE.
2. Install the ESP8266 board package.
3. Install the required sensor libraries.
4. Open `firmware/smart_agri_rover/smart_agri_rover.ino`.
5. Configure your Wi-Fi credentials in the firmware.
6. Select the correct ESP8266 board and COM port.
7. Upload the firmware to the NodeMCU.

**Note:** Do not upload your actual Wi-Fi password or private credentials to GitHub.

## 🌐 Web Interface

Connect to the same network as the NodeMCU and open its IP address in a browser to access the dashboard.

The `/data` endpoint provides sensor readings in JSON format.

## 👨‍💻 Developer

**Manikantha Gowda**

GitHub: https://github.com/Mgfx05

---

*Smart Agriculture Rover – IoT-based Agricultural Monitoring System.*