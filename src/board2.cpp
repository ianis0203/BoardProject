#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <mbedtls/aes.h>

// ===== CONFIGURATION =====
uint8_t gatewayMac[] = {0xC8, 0xF0, 0x9E, 0xAC, 0x8C, 0xCC}; 

// AES-128 Encryption Key (Must be exactly 16 bytes)
const unsigned char AES_KEY[16] = "V@uLtGu4rdK3y!23";

// Pins
#define REED_SWITCH_PIN GPIO_NUM_33 // Must be an RTC GPIO for ext0 wakeup

Adafruit_MPU6050 mpu;

// --- 16-Byte Payload Structure (Perfect for 1 AES Block) ---
typedef struct __attribute__((packed)) {
    uint32_t rollingCode; // 4 bytes
    uint8_t eventType;    // 1 byte (0=Heartbeat, 1=Door Open, 2=Tamper)
    uint8_t padding[3];   // 3 bytes (alignment)
    float accelX;         // 4 bytes
    float accelY;         // 4 bytes
} PlaintextPayload;       // Total: 16 bytes

RTC_DATA_ATTR uint32_t rtcRollingCode = 0; // Survives deep sleep

// Encrypted buffer to send
uint8_t encryptedData[16];
volatile bool ackReceived = false;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    ackReceived = (status == ESP_NOW_SEND_SUCCESS);
}

void setup() {
    Serial.begin(115200);
    pinMode(REED_SWITCH_PIN, INPUT_PULLUP);
    
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) return;
    esp_now_register_send_cb(OnDataSent);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, gatewayMac, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false; // We are doing App-Layer AES encryption instead!
    esp_now_add_peer(&peerInfo);

    // Initialize Payload
    PlaintextPayload myData;
    myData.rollingCode = ++rtcRollingCode;
    myData.accelX = 0; myData.accelY = 0;

    // --- DETERMINE WAKEUP REASON ---
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println("🚨 WAKEUP: DOOR OPENED!");
        myData.eventType = 1; 
    } else {
        Serial.println("WAKEUP: HEARTBEAT / BOOT");
        myData.eventType = 0;
        
        // Only power up I2C and MPU on Heartbeat to check position
        if (mpu.begin()) {
            sensors_event_t a, g, temp;
            mpu.getEvent(&a, &g, &temp);
            myData.accelX = a.acceleration.x;
            myData.accelY = a.acceleration.y;
            // If tilted too much, change event to TAMPER
            if (abs(a.acceleration.x) > 5.0 || abs(a.acceleration.y) > 5.0) {
                myData.eventType = 2; 
            }
        }
    }

    // --- ENCRYPT PAYLOAD (AES-128 ECB) ---
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, AES_KEY, 128);
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, (const unsigned char*)&myData, encryptedData);
    mbedtls_aes_free(&aes);

    // --- FIRE AND FORGET ---
    esp_now_send(gatewayMac, encryptedData, sizeof(encryptedData));
    
    // Wait max 100ms for ACK to save power
    unsigned long start = millis();
    while (!ackReceived && millis() - start < 100) { delay(1); }

    // --- CONFIGURE DEEP SLEEP ---
    // Wake up if the door opens (Pin goes HIGH)
    esp_sleep_enable_ext0_wakeup(REED_SWITCH_PIN, 1); 
    
    // OR Wake up every 60 minutes to send a Heartbeat
    esp_sleep_enable_timer_wakeup(60ULL * 60 * 1000000); 

    Serial.println("Going to sleep...");
    esp_deep_sleep_start();
}

void loop() {}