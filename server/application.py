import json
import serial
import threading
import time
from flask import Flask, render_template
from flask_socketio import SocketIO

# ===== CONFIGURATION =====
# Change this to match your Receiver ESP32's COM port (e.g., 'COM12' for Windows, '/dev/ttyUSB0' for Linux)
SERIAL_PORT = 'COM12' 
BAUD_RATE = 115200

app = Flask(__name__)
app.config['SECRET_KEY'] = 'vaultguard_super_secret'
socketio = SocketIO(app, cors_allowed_origins="*")

# Global state to keep the latest event
latest_status = {
    "status": "WAITING_FOR_DATA",
    "event": "None",
    "rolling_code": 0,
    "last_update": "Never"
}

def read_serial_data():
    """Background thread to continuously read data from the ESP32 Gateway"""
    global latest_status
    
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"✅ Successfully connected to Gateway on {SERIAL_PORT}")
    except Exception as e:
        print(f"❌ Error connecting to Serial Port: {e}")
        return

    while True:
        try:
            line = ser.readline().decode('utf-8').strip()
            if not line:
                continue
                
            data = json.loads(line)
            current_time = time.strftime("%H:%M:%S")
            
            # 1. Handle Critical Security Errors (Dead-Man Switch / Replay Attacks)
            if "error" in data:
                print(f"[{current_time}] 🚨 CRITICAL ERROR: {data['error']}")
                alert_payload = {
                    "type": "error",
                    "message": data["error"],
                    "time": current_time
                }
                socketio.emit('security_alert', alert_payload)
                
            # 2. Handle Normal Payload Events
            elif "event" in data:
                event_type = data['event']
                print(f"[{current_time}] Event: {event_type} | Code: {data['rolling_code']}")
                
                # Update global state
                latest_status = {
                    "status": "BREACHED" if "ALARM" in event_type else "SECURE",
                    "event": event_type,
                    "rolling_code": data['rolling_code'],
                    "tilt_x": data.get('tilt_x', 0),
                    "tilt_y": data.get('tilt_y', 0),
                    "time": current_time
                }
                
                # Push data to the frontend immediately
                socketio.emit('vault_update', latest_status)
                
        except json.JSONDecodeError:
            # Ignore garbled serial data during ESP32 boot
            pass
        except Exception as e:
            print(f"Serial read error: {e}")
            time.sleep(2) # Wait before retrying if serial crashes

@app.route('/')
def index():
    # Serve the dashboard page
    return render_template('index.html')

if __name__ == '__main__':
    # Start the background serial reader thread
    thread = threading.Thread(target=read_serial_data, daemon=True)
    thread.start()
    
    # Start the Flask web server
    print("🚀 Starting VaultGuard Server on http://127.0.0.1:5000")
    socketio.run(app, host='0.0.0.0', port=5000, debug=False)