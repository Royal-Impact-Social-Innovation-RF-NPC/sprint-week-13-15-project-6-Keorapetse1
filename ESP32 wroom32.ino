#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <esp_now.h>

// ======================= OLED SETUP =======================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ======================= SERVO SETUP =======================
#define SERVO_WOOD_PIN    12
#define SERVO_PLASTIC_PIN 13
#define SERVO_PAPER_PIN   14

Servo servoWood;
Servo servoPlastic;
Servo servoPaper;

// ======================= LED SETUP =======================
#define LED_GREEN 25   // System OK
#define LED_BLUE  26   // Communication OK
#define LED_RED   27   // Error

// ======================= VARIABLES =======================
unsigned long lastCommandTime = 0;
unsigned long binOpenStartTime = 0;
bool communicationOK = false;
bool binIsOpen = false;
String currentOpenBin = "";
const unsigned long BIN_OPEN_DURATION = 5000; // 5 seconds
const unsigned long COMMS_TIMEOUT = 10000;    // 10 seconds
const unsigned long DEBOUNCE_DELAY = 1000;    // 1 second between commands

// ======================= ESP-NOW STRUCT =======================
typedef struct struct_message {
    char command[20];
    char label[20];
    float confidence;
    uint32_t timestamp;
    uint8_t object_count;
} struct_message;

struct_message incomingMsg;
float CONFIDENCE_THRESHOLD = 0.7; // Increased threshold for better accuracy

// ======================= FUNCTION DECLARATIONS =======================
void openBin(const String &type);
void closeAllBins();
void updateDisplay(const String &line1, const String &line2 = "", const String &line3 = "");
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len);
void handleSystemStatus();

// ======================= SETUP =======================
void setup() {
    Serial.begin(115200);
    Serial.println("=== Smart Dustbin System Starting ===");

    // Initialize LEDs
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
    pinMode(LED_RED, OUTPUT);

    // Initial LED state - System starting
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, LOW);
    digitalWrite(LED_RED, HIGH); // Red during startup

    // OLED setup
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("OLED allocation failed!");
        while (true); // Stop if OLED fails
    }
    updateDisplay("System Starting", "Initializing...", "Please wait");

    // Servo setup
    servoWood.setPeriodHertz(50);
    servoWood.attach(SERVO_WOOD_PIN, 500, 2400);

    servoPlastic.setPeriodHertz(50);
    servoPlastic.attach(SERVO_PLASTIC_PIN, 500, 2400);

    servoPaper.setPeriodHertz(50);
    servoPaper.attach(SERVO_PAPER_PIN, 500, 2400);

    // Close all servos initially
    closeAllBins();
    delay(1000);

    // ESP-NOW Initialization
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        updateDisplay("ESP-NOW FAILED", "Restart system", "Error: ESP-NOW");
        digitalWrite(LED_RED, HIGH);
        Serial.println("Error initializing ESP-NOW");
        while(true);
    }
    
    // Register the receive callback with proper casting
    esp_now_register_recv_cb((esp_now_recv_cb_t)OnDataRecv);

    // System ready
    updateDisplay("Smart Dustbin", "System Ready", "Waiting...");
    digitalWrite(LED_GREEN, HIGH); // System OK
    digitalWrite(LED_RED, LOW);    // No errors
    digitalWrite(LED_BLUE, LOW);   // No comms yet
    
    Serial.println("✓ System initialized successfully");
    Serial.println("✓ Waiting for ESP-NOW commands...");
    Serial.print("MAC Address: ");
    Serial.println(WiFi.macAddress());
}

// ======================= MAIN LOOP =======================
void loop() {
    handleSystemStatus();
    
    // Auto-close bin after 5 seconds
    if (binIsOpen && (millis() - binOpenStartTime >= BIN_OPEN_DURATION)) {
        Serial.println("Auto-closing bin after 5 seconds");
        closeAllBins();
        binIsOpen = false;
        currentOpenBin = "";
        updateDisplay("Bin Closed", "Ready for next", "item...");
    }

    delay(100); // Small delay to prevent overwhelming the processor
}

// ======================= SYSTEM STATUS HANDLER =======================
void handleSystemStatus() {
    // Check communication timeout
    if (millis() - lastCommandTime > COMMS_TIMEOUT) {
        communicationOK = false;
        digitalWrite(LED_BLUE, LOW);
    }
    
    // System status LED (Green always on when system is operational)
    digitalWrite(LED_GREEN, HIGH);
}

// ======================= ESP-NOW CALLBACK =======================
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len != sizeof(struct_message)) {
        Serial.println("Invalid message length received");
        return;
    }

    memcpy(&incomingMsg, incomingData, sizeof(struct_message));
    
    // Debug print received data
    Serial.printf("Received - Command: %s, Label: %s, Confidence: %.2f\n", 
                  incomingMsg.command, incomingMsg.label, incomingMsg.confidence);

    // Check if this is an object detection command
    if (strcmp(incomingMsg.command, "OBJECT_DETECTED") != 0) {
        Serial.println("Ignoring non-detection command");
        return;
    }

    // Check confidence threshold
    if (incomingMsg.confidence < CONFIDENCE_THRESHOLD) {
        Serial.printf("Low confidence: %.2f < %.2f, ignoring\n", 
                      incomingMsg.confidence, CONFIDENCE_THRESHOLD);
        return;
    }

    // Prevent command flooding - debounce
    if (millis() - lastCommandTime < DEBOUNCE_DELAY) {
        Serial.println("Command too soon, ignoring (debounce)");
        return;
    }

    // Check if a bin is already open
    if (binIsOpen) {
        Serial.printf("Bin busy (%s open), ignoring new: %s\n", 
                      currentOpenBin.c_str(), incomingMsg.label);
        updateDisplay("System Busy", currentOpenBin + " bin open", "Wait...");
        return;
    }

    communicationOK = true;
    lastCommandTime = millis();
    digitalWrite(LED_BLUE, HIGH); // Communication active
    digitalWrite(LED_RED, LOW);   // No errors

    String label = String(incomingMsg.label);
    label.trim();
    label.toUpperCase();

    Serial.printf("Processing detection: %s (%.1f%%)\n", label.c_str(), incomingMsg.confidence * 100);

    // Handle different material types
    if (label.indexOf("WOOD") >= 0 || label == "WOOD") {
        openBin("WOOD");
    } else if (label.indexOf("PLASTIC") >= 0 || label == "PLASTIC") {
        openBin("PLASTIC");
    } else if (label.indexOf("PAPER") >= 0 || label == "PAPER") {
        openBin("PAPER");
    } else {
        Serial.printf("Unknown label: %s\n", label.c_str());
        updateDisplay("Unknown Material", label, "Try again");
        delay(2000);
        updateDisplay("Ready", "Waiting for", "materials...");
    }
}

// ======================= BIN CONTROL FUNCTIONS =======================
void openBin(const String &type) {
    Serial.printf("Opening %s bin\n", type.c_str());
    
    binIsOpen = true;
    currentOpenBin = type;
    binOpenStartTime = millis();

    updateDisplay("Opening:", type + " Bin", "Please wait...");

    // Close all bins first (safety measure)
    closeAllBins();
    delay(500);

    // Open the specific bin
    if (type == "WOOD") {
        servoWood.write(90);
        Serial.println("✓ Wood bin opened");
    } else if (type == "PLASTIC") {
        servoPlastic.write(90);
        Serial.println("✓ Plastic bin opened");
    } else if (type == "PAPER") {
        servoPaper.write(90);
        Serial.println("✓ Paper bin opened");
    }

    updateDisplay(type + " Bin", "OPEN", "5 seconds...");
    
    Serial.printf("%s bin will auto-close in 5 seconds\n", type.c_str());
}

void closeAllBins() {
    servoWood.write(0);
    servoPlastic.write(0);
    servoPaper.write(0);
    delay(100); // Small delay for servos to move
    
    Serial.println("All bins closed");
}

// ======================= DISPLAY FUNCTION =======================
void updateDisplay(const String &line1, const String &line2, const String &line3) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    display.setCursor(0, 0);
    display.println(line1);
    
    display.setCursor(0, 20);
    display.println(line2);
    
    display.setCursor(0, 40);
    display.println(line3);
    
    display.display();
}
