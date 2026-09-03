#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <mbedtls/aes.h>

const unsigned char AES_KEY[16] = "V@uLtGu4rdK3y!23";

// Recreate the struct to hold decrypted data
typedef struct __attribute__((packed)) {
    uint32_t rollingCode; 
    uint8_t eventType;    
    uint8_t padding[3];   
    float accelX;         
    float accelY;         
} PlaintextPayload;

uint32_t lastValidCode = 0;
const unsigned long DEADMAN_TIMEOUT = 65 * 60 * 1000; // 65 mins (Expected heartbeat every 60m)
unsigned long lastHeartbeat = 0;

void OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *incomingData, int len) {
    if (len != 16) return; // Must be exactly 1 AES block

    // --- DECRYPT PAYLOAD ---
    PlaintextPayload decryptedData;
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, AES_KEY, 128);
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, incomingData, (unsigned char*)&decryptedData);
    mbedtls_aes_free(&aes);

    // --- ROLLING CODE VERIFICATION (Anti-Replay) ---
    // The code must be strictly greater than the last seen code
    if (decryptedData.rollingCode <= lastValidCode && lastValidCode != 0) {
        Serial.println("{\"error\": \"SECURITY BREACH: Replay Attack Blocked\"}");
        return;
    }
    
    // Prevent massive desyncs (e.g., attacker trying to skip codes)
    if (lastValidCode != 0 && (decryptedData.rollingCode - lastValidCode) > 50) {
        Serial.println("{\"error\": \"SECURITY BREACH: Rolling Code Desync\"}");
        return;
    }

    lastValidCode = decryptedData.rollingCode;
    lastHeartbeat = millis();

    // --- OUTPUT TO PYTHON ---
    StaticJsonDocument<256> doc;
    doc["rolling_code"] = decryptedData.rollingCode;
    
    if (decryptedData.eventType == 0) doc["event"] = "Heartbeat";
    else if (decryptedData.eventType == 1) doc["event"] = "ALARM_DOOR_OPEN";
    else if (decryptedData.eventType == 2) doc["event"] = "ALARM_TAMPER_TILT";
    
    doc["tilt_x"] = decryptedData.accelX;
    doc["tilt_y"] = decryptedData.accelY;
    
    serializeJson(doc, Serial);
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    
    if (esp_now_init() != ESP_OK) return;
    esp_now_register_recv_cb(OnDataRecv);
    
    lastHeartbeat = millis();
}

void loop() {
    // Dead-Man's Switch Monitor
    if (millis() - lastHeartbeat > DEADMAN_TIMEOUT) {
        Serial.println("{\"error\": \"CRITICAL: Vault Node Heartbeat Lost! (Destroyed or Jammed)\"}");
        lastHeartbeat = millis(); // Reset to prevent spamming, wait for next cycle
    }
    delay(1000);
}