# ESP32 RFID User Management System

A comprehensive RFID-based access control and user management system built with ESP32 and a web interface.

---

## 📖 Overview

This project implements a full-featured RFID user management system using ESP32. It allows you to register RFID cards/tags, assign roles to users, and log access attempts. The system includes a responsive web interface for managing users and viewing access logs in real-time.

---

## ✨ Features

- 📱 Web-based interface accessible from any device on the network  
- 👤 Complete user management (add, delete, view)  
- 🔑 Role-based access control (admin, user)  
- 📊 Real-time access logging with timestamps  
- 🚦 Visual feedback using LED indicators (green for access granted, red for denied)  
- 🔔 Audible notifications using a buzzer  
- 💾 Persistent storage using LittleFS file system  

---

## 🔧 Hardware Requirements

- ESP32 development board  
- MFRC522 RFID reader module  
- Red and green LEDs  
- Buzzer  
- Jumper wires  
- Power supply  
- RFID cards/tags (13.56MHz)  

---

## 📌 Pin Connections

| Component      | ESP32 Pin |
|----------------|-----------|
| MFRC522 SDA    | 10        |
| MFRC522 SCK    | 11        |
| MFRC522 MOSI   | 12        |
| MFRC522 MISO   | 13        |
| Buzzer         | 4         |
| Red LED        | 5         |
| Green LED      | 6         |
| RST            | 9         |

---

## 📚 Libraries Required

- `Arduino.h`  
- `MFRC522v2.h`  
- `AsyncTCP.h`  
- `ESPAsyncWebServer.h`  
- `LittleFS.h`  
- `SPI.h`  
- `time.h`  
- `WiFi.h`  

---

## 🚀 Installation & Setup

1. Install the required libraries using the Arduino Library Manager  
2. Configure your WiFi credentials in the code  
3. Upload the data folder contents to ESP32 flash using the LittleFS data upload tool  
4. Upload the sketch to your ESP32  

---

## 📱 Web Interface

The system provides a web interface with three main sections:

- **Full Log** – View all access attempts with timestamps  
- **Add User** – Register new RFID cards with user names and roles  
- **Manage Users** – View all registered users and delete them if needed  

---

## 🔄 How It Works

1. When an RFID card is scanned, the system reads its UID  
2. The system checks if the UID exists in the users database  
3. If found, it grants access (green LED + single beep)  
4. If not found, it denies access (red LED + multiple beeps)  
5. All access attempts are logged with timestamp, UID, role, and user name  
6. The web interface updates in real-time to show the latest logs  

---

## 🔩 Project Structure

- `data` – Web interface files (HTML, CSS)  
- `esp32fs.jar.ino` – Main Arduino sketch  
- `users.txt` – Database of registered users  
- `log.txt` – Access attempt logs  

---

## 🛠️ Troubleshooting

- Make sure ESP32 is connected to WiFi  
- Check serial monitor for IP address and connection status  
- Ensure RFID reader is properly connected  
- Verify the LittleFS is properly mounted  

---

## 📝 License

This project is open source and available for personal and educational use.

---

## 🔗 Credits

Created by [Your Name] for IoT Midterm Homework.
