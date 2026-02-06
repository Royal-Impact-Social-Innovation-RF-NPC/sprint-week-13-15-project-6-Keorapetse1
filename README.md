# Smart Sorter (ESP32 Smart Dustbin)

An intelligent waste-sorting system powered by **ESP32-CAM** and **ESP32-WROOM32**.  
This project detects waste using image processing, controls a servo-driven lid, and displays real-time system feedback — all powered by a compact battery setup and wireless communication.

---

## Overview

**Smart Sorter** is a smart dustbin that uses AI-powered visual recognition and servo control to automate waste sorting.  
Initially, serial (UART) communication was used between modules, but it has since been upgraded to **ESP-NOW**, a faster, wireless, and low-power protocol for seamless module-to-module data transmission.

The system identifies waste with the **ESP32-CAM**, sends the classification result to the **ESP32-WROOM32**, and automatically opens the correct compartment using **micro servos**.  
A compact **OLED display** provides live feedback and system status.

---

## Components & Functions

| Component | Description | Function in System |
|------------|--------------|--------------------|
| **ESP32-CAM** | Camera module with onboard Wi-Fi | Captures images of waste and classifies them using Edge Impulse or onboard ML. Sends classification results to ESP32-WROOM32 using **ESP-NOW**. |
| **ESP32-WROOM32** | Primary controller with OLED interface | Receives classification data from ESP32-CAM and actuates the appropriate **micro servo** to open the lid. Displays status on the OLED screen. |
| **Micro Servos (SG90 / MG90S)** | Small, lightweight servos | Control the opening and closing of the smart bin lid(s). |
| **OLED Display (SSD1306)** | 128x64 I2C display | Shows text feedback such as system status, waste type, and connection info. |
| **LED Indicators** | Colored LEDs (Red, Green, Blue, etc.) | Indicate system states — e.g., power ON, lid open, waste type detected, or error. |
| **Battery Pack (4-cell)** | Four lithium cells in series | Supplies **13.8V total** to power the system. |
| **Step-Down Converter (Buck Converter)** | Adjustable DC-DC converter | Steps down 13.8V → **5V**, supplying safe voltage for both ESP32 boards and peripherals. |

---

## Power System Architecture

The **battery pack** consists of 4 cells connected in **series**:  
`3.45V × 4 = 13.8V total output.`

This voltage is **stepped down to 5V** using a **buck converter** before powering the ESP32 boards, OLED, and servos.

## Communication Architecture

### From UART ➡️ ESP-NOW
Originally, the ESP32-CAM and ESP32-WROOM32 communicated through **UART serial**.  
This required physical wiring and caused potential signal noise.  

The communication was later **upgraded to ESP-NOW**, enabling:
- **Faster wireless data transfer**
-  **Lower power consumption**
-  **No physical connection between modules**
-  **Reliable, peer-to-peer messaging**

**Workflow:**
1. ESP32-CAM classifies waste.
2. It sends a wireless ESP-NOW message to ESP32-WROOM32.
3. ESP32-WROOM32 receives the result and controls the servos + display.

## System Workflow

1. **Waste Detection** — The ESP32-CAM captures an image.
2. **Classification** — The onboard model (Edge Impulse) identifies the waste type.
3. **Wireless Transmission** — ESP-NOW sends data to the ESP32-WROOM32.
4. **Display Feedback** — The OLED screen shows the detected category.
5. **Actuation** — The corresponding servo opens the lid.
6. **LED Indicators** — Show operation status and confirmation.

## Libraries Used

Make sure to install these Arduino libraries
1. ESP32Servo
2. Wire
3. Adafruit_GFX
4. Adafruit_SSD1306
5. esp_now
6. WiFi

## These are images of the process

![IMG_20251022_114253](https://github.com/user-attachments/assets/87b6361d-f905-47df-a649-93120fee5d65)

![IMG_20251023_102959](https://github.com/user-attachments/assets/6804c9d4-d4b2-4e89-8edd-93db40602f6c)

![IMG_20251022_115455](https://github.com/user-attachments/assets/45aaebf0-a7d9-4a31-9d80-a3ed0c86a508)

![IMG_20251022_142513](https://github.com/user-attachments/assets/61d2a999-5d06-4577-871b-cb1c3d5e3468)

![IMG_20251023_094448](https://github.com/user-attachments/assets/1f3b4366-3099-4ab8-bdb9-614e2efa3c3c)

![IMG_20251023_101248](https://github.com/user-attachments/assets/07a05283-d800-4ac6-bc70-cfb1594d2458)

![IMG_20251023_135641](https://github.com/user-attachments/assets/0a835cc2-ed4a-4385-ace4-62712f5a1133)

![IMG_20251023_142044](https://github.com/user-attachments/assets/5f612ae1-4063-4a52-93ec-db7de500c21b)

![IMG_20251023_150648](https://github.com/user-attachments/assets/9cbf4551-5e77-4f55-9924-97fbf14e09e3)

![IMG_20251024_093118](https://github.com/user-attachments/assets/9c77d306-8b33-4a77-8913-587c866bf376)

![IMG_20251024_104923](https://github.com/user-attachments/assets/183b378b-d6dc-4734-b637-20afb4faf050)

![IMG_20251024_115001](https://github.com/user-attachments/assets/6baba4a3-b362-4e4f-9f2d-6fd0024496a5)

![IMG_20251030_155914](https://github.com/user-attachments/assets/7f7b28ec-a872-429c-8e75-6ef86c91ee83)

![IMG_20251030_163024](https://github.com/user-attachments/assets/9856cdd7-5386-4538-8474-758ca8972a52)

![IMG_20251030_163031](https://github.com/user-attachments/assets/acddb3c4-abb5-41a3-bfe3-9b768894f728)

![IMG_20251030_163039](https://github.com/user-attachments/assets/9459b04e-f1ef-4aa4-b13c-ce842dd4d5fd)

![IMG_20251030_155950](https://github.com/user-attachments/assets/6ce4085d-0120-4b86-a15e-287694244571)

![IMG_20251030_160002](https://github.com/user-attachments/assets/4dd50e68-f358-410c-96f3-5b622229da3b)


