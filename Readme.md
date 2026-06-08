# ESP32 RFID Read and Write Captive Portal

A standalone, web-based utility for interacting with 13.56 MHz RFID tags using an ESP32 and an MFRC522 reader. The system launches a dedicated Wi-Fi access point and hosts an asynchronous captive portal, enabling users to read, update, and write data blocks directly to MIFARE Classic cards from any smartphone or web browser without requiring an internet connection.

---

## Project Description

This repository provides a complete firmware solution that turns an ESP32 and an MFRC522 RFID module into an offline web-hosted chip programmer. When a device connects to the ESP32's local Wi-Fi network, a Captive Portal automatically intercepts the connection and serves a reactive web dashboard. 

The application utilizes background AJAX polling to display tag data instantly as it is swiped, and features an internal memory-staging mechanism allowing secure raw block-level writes back to the card. It is ideal for prototyping access control systems, credential managers, or localized data logging tools.

---
<img width="720" height="1608" alt="WhatsApp Image 2026-06-08 at 11 57 36 PM" src="https://github.com/user-attachments/assets/15ca777a-3d36-4510-b843-029c21ca9038" />
<img width="899" height="1599" alt="WhatsApp Image 2026-06-08 at 11 57 36 PM (1)" src="https://github.com/user-attachments/assets/f56201ab-ca91-4f02-94b7-26b73f009b94" />

## Key Features

* **Zero Configuration:** Functions as an isolated, standalone Captive Portal requiring no external internet connection, local routers, or mobile apps.
* **Asynchronous Web Engine:** Employs ESPAsyncWebServer for fast HTTP transactions that do not interfere with the underlying hardware SPI polling loops.
* **Live UI Refreshing:** Uses background JavaScript fetch calls to stream sensor updates to your browser without rendering full webpage reloads.
* **Authenticated Data Modification:** Implements default transport key verification (0xFFFFFFFFFFFF) to safely rewrite Sector 1, Block 4 on standard MIFARE Classic tags.

---

## Hardware Requirements

* **Microcontroller:** Any standard ESP32 DevKit / ESP32-WROOM-32 board.
* **RFID Module:** MFRC522 RC522 (13.56 MHz SPI Module).
* **Supported Tags:** MIFARE Classic 1K, MIFARE Classic 4K, and 13.56 MHz Keyfobs.

### Pin Configurations

The MFRC522 module is mapped to the standard high-speed VSPI hardware pins on the ESP32:

| RC522 Pin | ESP32 Pin | Function |
| :--- | :--- | :--- |
| **SDA (SS)** | GPIO 5 | SPI Slave Select |
| **SCK** | GPIO 18 | SPI Clock |
| **MOSI** | GPIO 23 | SPI Master Out Slave In |
| **MISO** | GPIO 19 | SPI Master In Slave Out |
| **RST** | GPIO 21 | Module Reset |
| **GND** | GND | Common Ground |
| **3.3V** | 3.3V | 3.3V Power Supply |

Warning: Do not connect the RC522 module to the 5V or VIN pin of the ESP32. The MFRC522 IC operates strictly at 3.3V. Exposure to 5V will permanently damage the radio transmitter circuit.

---

## Required Software Libraries

Ensure the following libraries are installed in your Arduino IDE or PlatformIO project manager:

1. **MFRC522** (By Miguel Balboa / GithubCommunity)
2. **ESPAsyncWebServer** (By me-no-dev)
3. **AsyncTCP** (By me-no-dev)
4. **DNSServer** (Built-in ESP32 Core Library)

---

## Technical Workflow and Architecture

### Web API Endpoints

The internal asynchronous server routes traffic across three critical internal locations:

* `GET /` — Serves the core HTML/CSS/JS user interface embedded directly inside the ESP32 flash memory (PROGMEM).
* `GET /status` — Returns a dynamic JSON payload containing the current parsed tag text, active write flag status, and error states.
* `POST /write` — Accepts incoming data payloads submitted by the user interface form to transition the hardware loop into an Armed Write state.

### Memory Allocation Schema
To avoid corrupting the manufacturer block (Sector 0) or standard security key trailer blocks, this implementation targets Sector 1, Block 4. It filters incoming text, truncates arrays to a maximum length of 16 characters, and fills trailing unused bits with text space layout padding.

---

## Quick Start Guide

1. Wire the components according to the pin configuration matrix above.
2. Clone this repository and open the codebase inside your preferred compiler environment.
3. Compile and flash the firmware to your ESP32 board.
4. On your smartphone or laptop, join the open Wi-Fi network named `RFID-Writer-Portal`.
5. The captive portal prompt should launch automatically. Alternatively, manually access the system by opening any web browser and navigating to `http://192.168.4.1`.
