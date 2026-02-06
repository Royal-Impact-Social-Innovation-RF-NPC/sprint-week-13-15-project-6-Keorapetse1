#include <Theo1-project-1_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include "esp_camera.h"
#include <esp_now.h>
#include <WiFi.h>

// Select camera model
#define CAMERA_MODEL_AI_THINKER // Has PSRAM

#if defined(CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#endif

/* Constant defines -------------------------------------------------------- */
#define EI_CAMERA_RAW_FRAME_BUFFER_COLS           320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS           240
#define EI_CAMERA_FRAME_BYTE_SIZE                 3

/* ESP-NOW Configuration */
uint8_t slaveMac[] = {0x4c, 0x11, 0xae, 0x7d, 0xaf, 0xf4}; // REPLACE WITH YOUR WROOM32 MAC

// Message structure for ESP-NOW
typedef struct struct_message {
  char command[20];
  char label[20];
  float confidence;
  uint32_t timestamp;
  uint8_t object_count;
} struct_message;

struct_message detectionData;

// ESP-NOW callbacks
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

/* Private variables ------------------------------------------------------- */
static bool debug_nn = false;
static bool is_initialised = false;
uint8_t *snapshot_buf;
unsigned long lastDetectionTime = 0;
const unsigned long DETECTION_COOLDOWN = 5000;
const unsigned long LOOP_DELAY = 3000;

static camera_config_t camera_config = {
    .pin_pwdn = PWDN_GPIO_NUM,
    .pin_reset = RESET_GPIO_NUM,
    .pin_xclk = XCLK_GPIO_NUM,
    .pin_sscb_sda = SIOD_GPIO_NUM,
    .pin_sscb_scl = SIOC_GPIO_NUM,

    .pin_d7 = Y9_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d0 = Y2_GPIO_NUM,
    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href = HREF_GPIO_NUM,
    .pin_pclk = PCLK_GPIO_NUM,

    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_QVGA,
    .jpeg_quality = 12,
    .fb_count = 1,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

// Forward declarations
bool ei_camera_init(void);
void ei_camera_deinit(void);
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf);
static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr);
void setupESP_NOW();
void sendDetection(const char* label, float confidence, uint8_t count);
bool initializeCameraWithRetry();

/**
* Camera initialization function
*/
bool ei_camera_init(void) {
    if (is_initialised) return true;

    // Initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        return false;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s->id.PID == OV3660_PID) {
        s->set_vflip(s, 1);
        s->set_brightness(s, 1);
        s->set_saturation(s, 0);
    }

    is_initialised = true;
    return true;
}

/**
* Camera deinitialization
*/
void ei_camera_deinit(void) {
    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK) {
        Serial.println("Camera deinit failed");
        return;
    }
    is_initialised = false;
}

/**
* Camera capture function
*/
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) {
    if (!is_initialised) {
        Serial.println("ERR: Camera is not initialized");
        return false;
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        return false;
    }

    bool converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, snapshot_buf);
    esp_camera_fb_return(fb);

    if (!converted) {
        Serial.println("Conversion failed");
        return false;
    }

    if ((img_width != EI_CAMERA_RAW_FRAME_BUFFER_COLS) || (img_height != EI_CAMERA_RAW_FRAME_BUFFER_ROWS)) {
        ei::image::processing::crop_and_interpolate_rgb888(
            out_buf,
            EI_CAMERA_RAW_FRAME_BUFFER_COLS,
            EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
            out_buf,
            img_width,
            img_height);
    }

    return true;
}

/**
* Get camera data for Edge Impulse
*/
static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr) {
    size_t pixel_ix = offset * 3;
    size_t pixels_left = length;
    size_t out_ptr_ix = 0;

    while (pixels_left != 0) {
        out_ptr[out_ptr_ix] = (snapshot_buf[pixel_ix + 2] << 16) + (snapshot_buf[pixel_ix + 1] << 8) + snapshot_buf[pixel_ix];
        out_ptr_ix++;
        pixel_ix += 3;
        pixels_left--;
    }
    return 0;
}

/**
* Setup function
*/
void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("=== Smart Bin ESP32-CAM Starting ===");
    
    delay(2000);
    
    setupESP_NOW();

    if (!initializeCameraWithRetry()) {
        Serial.println("CRITICAL: Camera initialization failed!");
        while(1) {
            delay(1000);
            Serial.println("System halted - Check camera connection");
        }
    }

    Serial.println("✓ All systems ready");
    Serial.println("=====================================");
    delay(2000);
}

/**
* Initialize camera with retry mechanism
*/
bool initializeCameraWithRetry() {
    int retryCount = 0;
    const int maxRetries = 5;
    
    while (retryCount < maxRetries) {
        Serial.print("Camera initialization attempt ");
        Serial.print(retryCount + 1);
        Serial.print("/");
        Serial.println(maxRetries);
        
        if (ei_camera_init()) {
            return true;
        }
        
        retryCount++;
        delay(2000);
    }
    return false;
}

/**
* Initialize ESP-NOW
*/
void setupESP_NOW() {
    Serial.println("Initializing ESP-NOW...");
    
    WiFi.mode(WIFI_STA);
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    
    esp_now_register_send_cb((esp_now_send_cb_t)OnDataSent);
    
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, slaveMac, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }
    
    Serial.println("✓ ESP-NOW Initialized");
}

/**
* Send detection via ESP-NOW
*/
void sendDetection(const char* label, float confidence, uint8_t count) {
    strncpy(detectionData.command, "OBJECT_DETECTED", sizeof(detectionData.command));
    strncpy(detectionData.label, label, sizeof(detectionData.label));
    detectionData.confidence = confidence;
    detectionData.timestamp = millis();
    detectionData.object_count = count;
    
    Serial.print("Sending detection: ");
    Serial.print(label);
    Serial.print(" (");
    Serial.print(confidence * 100);
    Serial.println("%)");
    
    esp_err_t result = esp_now_send(slaveMac, (uint8_t *) &detectionData, sizeof(detectionData));
    
    if (result == ESP_OK) {
        Serial.println("✓ Detection sent successfully");
    } else {
        Serial.println("✗ Error sending detection");
    }
}

/**
* Main loop
*/
void loop() {
    Serial.println();
    Serial.println("=== Starting new detection cycle ===");
    delay(LOOP_DELAY);

    snapshot_buf = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);

    if(snapshot_buf == nullptr) {
        Serial.println("✗ ERR: Failed to allocate snapshot buffer!");
        return;
    }

    ei::signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = &ei_camera_get_data;

    if (ei_camera_capture((size_t)EI_CLASSIFIER_INPUT_WIDTH, (size_t)EI_CLASSIFIER_INPUT_HEIGHT, snapshot_buf) == false) {
        Serial.println("✗ Failed to capture image");
        free(snapshot_buf);
        return;
    }

    Serial.println("✓ Image captured successfully");

    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, debug_nn);
    
    if (err != EI_IMPULSE_OK) {
        Serial.print("✗ ERR: Failed to run classifier (");
        Serial.print(err);
        Serial.println(")");
        free(snapshot_buf);
        return;
    }

    Serial.println("✓ Classification completed");

    bool objectDetected = false;
    uint8_t detectedCount = 0;
    char detectedLabel[20] = "";
    float maxConfidence = 0.0;

#if EI_CLASSIFIER_OBJECT_DETECTION == 1
    Serial.println("Object detection results:");
    for (uint32_t i = 0; i < result.bounding_boxes_count; i++) {
        ei_impulse_result_bounding_box_t bb = result.bounding_boxes[i];
        if (bb.value == 0) {
            continue;
        }
        Serial.print("  ");
        Serial.print(bb.label);
        Serial.print(" (");
        Serial.print(bb.value * 100, 1);
        Serial.println("%)");
        
        if (bb.value > maxConfidence && bb.value > 0.7) {
            objectDetected = true;
            detectedCount++;
            strncpy(detectedLabel, bb.label, sizeof(detectedLabel));
            maxConfidence = bb.value;
        }
    }
#else
    Serial.println("Classification results:");
    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        Serial.print("  ");
        Serial.print(ei_classifier_inferencing_categories[i]);
        Serial.print(": ");
        Serial.println(result.classification[i].value, 5);
        
        if (result.classification[i].value > 0.7 && result.classification[i].value > maxConfidence) {
            objectDetected = true;
            strncpy(detectedLabel, ei_classifier_inferencing_categories[i], sizeof(detectedLabel));
            maxConfidence = result.classification[i].value;
        }
    }
#endif

    if (objectDetected) {
        if (millis() - lastDetectionTime > DETECTION_COOLDOWN) {
            Serial.println("✓ Sending detection via ESP-NOW");
            sendDetection(detectedLabel, maxConfidence, detectedCount);
            lastDetectionTime = millis();
        } else {
            Serial.println("⏳ Detection cooldown active - skipping send");
        }
    } else {
        Serial.println("ℹ️ No confident objects detected");
    }

    free(snapshot_buf);
    Serial.println("=== Detection cycle completed ===");
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_CAMERA
#error "Invalid model for current sensor"
#endif
