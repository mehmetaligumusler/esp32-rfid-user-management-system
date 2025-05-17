# ESP32-S3 RFID & Voice Assistant - Wiring Guide

This guide provides detailed instructions for wiring all the components of the ESP32-S3 RFID and Voice Assistant project.

## Hardware Components

### Required Components
1. ESP32-S3 Dev Kit (main controller)
2. MFRC522 RFID Reader Module
3. INMP441 I2S MEMS Microphone
4. 3W Speaker with I2S Amplifier (MAX98357A or similar)
5. SD Card Module
6. Red and Green LEDs
7. Buzzer
8. Resistors: 2x 220Ω (for LEDs)
9. Breadboard and jumper wires
10. Micro SD Card (min 8GB, Class 10)
11. Power supply (5V)

## Wiring Diagrams

### 1. ESP32-S3 to MFRC522 RFID Reader

```
ESP32-S3    | MFRC522
------------|----------
GPIO 10     | SDA (SS)
GPIO 11     | SCK
GPIO 12     | MOSI
GPIO 13     | MISO
GPIO 9      | RST
3.3V        | VCC
GND         | GND
```

### 2. ESP32-S3 to INMP441 I2S Microphone

```
ESP32-S3    | INMP441
------------|----------
GPIO 5      | SCK (BCLK)
GPIO 6      | WS (L/R CLK)
GPIO 7      | SD (DATA)
3.3V        | VDD
GND         | GND
GND         | L/R SEL (connect to GND for left channel)
```

### 3. ESP32-S3 to I2S Amplifier & Speaker

```
ESP32-S3    | MAX98357A
------------|----------
GPIO 16     | BCLK
GPIO 15     | LRC (WS)
GPIO 14     | DIN
3.3V        | VIN
GND         | GND
```

Connect the speaker to the output terminals of the MAX98357A amplifier.

### 4. ESP32-S3 to SD Card Module

```
ESP32-S3    | SD Card Module
------------|----------
GPIO 37     | MISO
GPIO 35     | MOSI
GPIO 36     | SCK
GPIO 34     | CS
3.3V        | VCC
GND         | GND
```

### 5. ESP32-S3 to LEDs and Buzzer

```
ESP32-S3    | Component
------------|----------
GPIO 4      | Buzzer (+) (GND to -)
GPIO 5      | Red LED (+) via 220Ω resistor (GND to -)
GPIO 6      | Green LED (+) via 220Ω resistor (GND to -)
GPIO 47     | Status LED (+) via 220Ω resistor (GND to -)
```

## Complete Wiring Diagram

```
                                 +----------------+
                                 |                |
                                 |   ESP32-S3     |
                                 |                |
                                 +----------------+
                                    |  |  |  |  |
    +----------+                 +--+--+--+--+--+--+                  +---------+
    |          | <-- SDA/10  ----+     |     |     +--- 5/SCK ------> |         |
    |          | <-- SCK/11  ----+     |     |     +--- 6/WS -------> |         |
    | MFRC522  | <-- MOSI/12 ----+     |     |     +--- 7/SD ------> |  INMP441 |
    |          | <-- MISO/13 ----+     |     |     |                  |         |
    |          | <-- RST/9   ----+     |     |     |                  +---------+
    +----------+                       |     |     |
                                       |     |     |                  +---------+
                                       |     |     +--- 16/BCLK ----> |         |
    +----------+                       |     +------- 15/LRC ------> | MAX98357A|
    |          | <-- MISO/37 ----------+             14/DIN -------> |         |
    |  SD Card | <-- MOSI/35 -----------------------> |                  +---------+
    |  Module  | <-- SCK/36  -----------------------> |                      |
    |          | <-- CS/34   -----------------------> |                  [SPEAKER]
    +----------+                                      |
                                                      |
    +----------+                                      |
    | Red LED  | <-- 5 via 220Ω --------------------+
    +----------+                                    |
                                                    |
    +----------+                                    |
    | Green LED| <-- 6 via 220Ω --------------------+
    +----------+                                    |
                                                    |
    +----------+                                    |
    | Status LED| <-- 47 via 220Ω -------------------+
    +----------+                                    |
                                                    |
    +----------+                                    |
    | Buzzer   | <-- 4 ----------------------------+
    +----------+
```

## Important Notes

1. **Power Supply**: Ensure the ESP32-S3 is powered properly. When using multiple peripherals, a dedicated 5V power supply with sufficient current capacity (at least 1A) is recommended.

2. **Ground Connections**: All ground connections should be common. Make sure all component GND pins are connected to the ESP32-S3 GND.

3. **SD Card Formatting**: Format the SD card as FAT32 before use.

4. **I2S Configuration**: The INMP441 microphone and MAX98357A amplifier share the I2S bus but use different ESP pins to avoid conflicts.

5. **Voltage Levels**: All components in this project operate at 3.3V logic level, which is compatible with the ESP32-S3. Do not use 5V components without level shifters.

## Testing the Connections

After completing the wiring, you can run the following basic tests:

1. **RFID Reader Test**: Use the MFRC522 basic example to verify the RFID reader is working.
2. **Microphone Test**: Run a simple I2S microphone sample code to test audio input.
3. **Speaker Test**: Play a test tone through the I2S amplifier and speaker.
4. **SD Card Test**: Perform basic SD card read/write operations.
5. **LED and Buzzer Test**: Run a simple sketch to toggle the LEDs and buzzer.

Only proceed to the full firmware installation after each component has been individually tested and verified to be working. 