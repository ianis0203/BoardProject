Markdown
# VaultGuard 🛡️: Event-Driven Asset Security System

This project is a high-security, ultra-low-power IoT physical intrusion detection system designed to monitor a safe, server rack, or valuable asset. It utilizes hardware interrupts to remain in zero-power deep sleep until a breach occurs, instantly encrypting and firing an alert to a local gateway.

## 🏗 System Architecture

1.  **Vault Node (ESP32 + Sensors)**: Mounted on the asset. Sleeps indefinitely. Wakes instantly via `ext0` hardware interrupt if the door opens or if the accelerometer detects tampering.
2.  **Gateway Node (ESP32)**: Securely receives encrypted payloads, acts as a cryptographic validator, and forwards verified events via Serial.
3.  **Command Center (Python/Flask)**: Listens to the Gateway, analyzes telemetry for Replay/Desync attacks, and hosts a real-time Socket.IO Web Dashboard.

---

## 🚀 Key Security & Power Features

### 🔒 Cryptography & Security
*   **AES-128 ECB Encryption**: The payload is completely scrambled at the application layer before wireless transmission using mbedTLS.
*   **Rolling Codes (Anti-Replay)**: Every transmission includes a sequentially incrementing code stored in RTC memory. The Gateway drops any packet with an older or duplicate code, rendering replay attacks useless.
*   **Dead-Man's Switch**: The Vault Node sends a silent heartbeat every 60 minutes. If the Gateway does not receive this heartbeat (e.g., the Vault Node was jammed, destroyed, or lost power), the dashboard automatically triggers a "Critical Missing" alert.

### 🔋 Ultra-Low Power
*   **EXT0 Hardware Wakeup**: Unlike standard IoT sensors that wake up on a timer, this ESP32 draws micro-amps of power while asleep. The physical action of opening the door closes a Reed Switch, which instantly powers up the CPU to fire the alarm.

---

## 🛠 Hardware Setup

### Vault Node (Sender)
*   **MPU6050 Accelerometer** (I2C): 
    *   SDA -> GPIO 21
    *   SCL -> GPIO 22
*   **Magnetic Reed Switch** (Door Sensor): 
    *   One leg to GND, one leg to **GPIO 33** (Must be an RTC GPIO for ext0 wakeup).

### Gateway Node (Receiver)
*   **Type**: Naked ESP32 (No sensors/actuators).
*   **Role**: Always-on cryptographic validator connected via USB to the server.

---

## 📂 Project Structure

```text
.
├── src/
│   ├── board1.cpp        # Vault Firmware (AES Encrypt, EXT0 Wakeup, MPU6050)
│   └── board2.cpp      # Gateway Firmware (AES Decrypt, Rolling Code Auth)
├── server/
│   ├── app.py            # Python Flask & Socket.IO Gateway Server
│   └── templates/
│       └── index.html    # Real-time Web Dashboard (Tailwind CSS)
└── platformio.ini        # Project Configuration & Dependencies