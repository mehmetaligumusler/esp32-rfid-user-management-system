# ESP32 RFID and Voice Assistant System

A comprehensive IoT system built with ESP32-S3 that combines RFID access control and a voice assistant powered by LLM technology.

---

## 📖 Overview

This project implements a dual-feature system:
1. RFID-based access control with user management via a web interface
2. Voice-activated assistant that uses TensorFlow Lite, I2S microphone, and cloud APIs (Gemini, Whisper)

The system provides both physical access control and intelligent voice interaction with low latency responses.

---

## ✨ Features

### RFID User Management
- 📱 Web-based interface for managing users and viewing logs  
- 👤 Complete user management (add, delete, view)  
- 🔑 Role-based access control (admin, user)  
- 📊 Real-time access logging with timestamps  
- 🚦 Visual and audible feedback with LEDs and buzzer  

### Voice Assistant
- 🎤 Wake word detection using TensorFlow Lite ("Hey Assistant")
- 🗣️ Speech-to-text conversion with free API services
- 🤖 LLM-powered responses via Gemini Free API
- 🔊 Text-to-speech conversion and audio playback
- 💾 Efficient memory management with SD card storage
- ⏱️ Fast response times (under 10 seconds)

---

## 🔧 Hardware Requirements

### Base System (RFID)
- ESP32-S3 development board
- MFRC522 RFID reader module  
- Red and green LEDs  
- Buzzer
- Jumper wires

### Voice Assistant Extension
- INMP441 I2S Microphone module
- I2S Speaker/amplifier module (3W mini speaker)
- SD Card module
- Additional jumper wires

---

## 📌 Pin Connections

### RFID System
| Component      | ESP32-S3 Pin |
|----------------|-------------|
| MFRC522 SDA    | 10          |
| MFRC522 SCK    | 11          |
| MFRC522 MOSI   | 12          |
| MFRC522 MISO   | 13          |
| Buzzer         | 4           |
| Red LED        | 5           |
| Green LED      | 6           |
| RST            | 9           |

### Voice Assistant System
| Component            | ESP32-S3 Pin |
|----------------------|-------------|
| I2S Mic Serial Clock | 5           |
| I2S Mic L/R Clock    | 6           |
| I2S Mic Serial Data  | 7           |
| I2S Speaker BCLK     | 16          |
| I2S Speaker WCLK     | 15          |
| I2S Speaker DOUT     | 14          |
| SD Card MISO         | 37          |
| SD Card MOSI         | 35          |
| SD Card SCK          | 36          |
| SD Card CS           | 34          |
| Status LED           | 47          |

---

## 📚 Libraries Required

### RFID System
- `Arduino.h`  
- `MFRC522v2.h`  
- `AsyncTCP.h`  
- `ESPAsyncWebServer.h`  
- `LittleFS.h`  
- `SPI.h`  
- `time.h`  
- `WiFi.h`  

### Voice Assistant System
- `I2S.h`
- `SD.h`
- `HTTPClient.h`
- `ArduinoJson.h`
- `TensorFlowLite_ESP32.h`
- Custom TensorFlow Lite components

---

## 🚀 Installation & Setup

1. Install all required libraries using the Arduino Library Manager
2. Configure your WiFi credentials in the code
3. Upload the data folder contents to ESP32 flash using the LittleFS data upload tool
4. Upload the voice_assistant_esp32.ino sketch to your ESP32-S3
5. Prepare the SD card with required system files
6. Connect all hardware components according to the pin connection diagram

---

## 📱 Web Interface

The system provides a web interface with three main sections:

- **Full Log** – View all access attempts with timestamps  
- **Add User** – Register new RFID cards with user names and roles  
- **Manage Users** – View all registered users and delete them if needed  

---

## 🗣️ Voice Assistant Usage

1. Say the wake word "Hey Assistant" to activate the system
2. The status LED will light up to indicate it's listening
3. Speak your query clearly (up to 5 seconds)
4. The system will process your query through:
   - Speech-to-text conversion (Whisper API)
   - Natural language understanding (Gemini API)
   - Text-to-speech synthesis (Google TTS API)
5. The assistant will respond through the speaker
6. Each interaction is logged to the SD card

---

## 🔄 How It Works

### RFID System
1. When an RFID card is scanned, the system reads its UID
2. The system checks if the UID exists in the users database
3. If found, it grants access (green LED + single beep)
4. If not found, it denies access (red LED + multiple beeps)
5. All access attempts are logged with timestamp, UID, role, and user name

### Voice Assistant System
1. The system continuously monitors audio for the wake word
2. When detected, it records a voice query and saves it to SD card
3. The audio is sent to a speech-to-text API for transcription
4. The text query is sent to Gemini API for processing
5. The response is converted to speech and played through the speaker
6. All processing steps are logged for debugging

---

## 🔩 Project Structure

- `esp32fs.jar.ino` – Original RFID system Arduino sketch
- `voice_assistant_esp32.ino` – Main voice assistant sketch
- `audio_provider.h` – Manages audio input from microphone
- `command_responder.h` – Handles responses to wake word detection
- `feature_provider.h` – Extracts features from audio for wake word detection
- `recognize_commands.h` – Interprets TensorFlow model outputs
- `micro_features_*.h/cpp` – Support files for audio processing
- `data/` – Web interface files (HTML, CSS)
- `users.txt` – Database of registered users
- `log.txt` – Access and system event logs

---

## 🛠️ Troubleshooting

- Ensure ESP32-S3 is connected to WiFi
- Check serial monitor for IP address and system status
- Verify all hardware connections are correct
- Check SD card is properly formatted (FAT32)
- Ensure API keys are properly configured
- Monitor the log.txt file for error messages

---

## 📝 API Key Setup

The system requires API keys for the following services:
1. OpenAI Whisper API (for speech-to-text)
2. Google Gemini API (for natural language processing)
3. Google Text-to-Speech API (for speech synthesis)

Replace placeholder API keys in the code with your actual keys before uploading.

---

## 🔒 Privacy and Security

- All audio processing is done through secure HTTPS connections
- Audio files are temporarily stored on the SD card and can be automatically deleted
- User RFID data is stored locally in the ESP32 flash memory
- No data is permanently stored in cloud services
