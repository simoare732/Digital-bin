import paho.mqtt.client as mqtt
import json
import time
import os

TOKEN_FILE_NAME = "/home/francesco/Unimore/Digital-bin/MqttSubscriber.py/token.txt" 
MQTT_BROKER_HOST = "mqtt.faffofvtt.work"
MQTT_BROKER_PORT = 1883
REQUEST_ID = 1

def get_gateway_token():
    """Read AccessToken."""
    try:
        with open(TOKEN_FILE_NAME, 'r') as f:
            token = f.read().strip()
            if not token:
                raise ValueError("Token not found")
            return token
    except FileNotFoundError:
        print(f"Error: File '{TOKEN_FILE_NAME}' not found.")
        exit(1)
    except ValueError as e:
        print(f"Error: {e}")
        exit(1)

GATEWAY_ACCESS_TOKEN = get_gateway_token()

# --- Topic e Payload ---

TOPIC_SUBSCRIBE_TELEMETRY_RESPONSE = f"v1/devices/me/telemetry/response/{REQUEST_ID}"
TOPIC_PUBLISH_TELEMETRY_REQUEST = f"v1/devices/me/telemetry/request/{REQUEST_ID}"

TELEMETRY_REQUEST_PAYLOAD = json.dumps({
    "telemetryKeys": ["temperatura", "umidita"], 
    "cmdId": REQUEST_ID,
    "method": "getTelemetry",
    "scope": "LATEST_TELEMETRY",
    "query": {
        "keys": ["temperatura", "umidita"],
        "startTs": 0,
        "endTs": 9223372036854775807,
        "interval": 0,
        "agg": "NONE",
        "limit": 1
    }
})


# --- Funzioni Callback di MQTT ---

def on_connect(client, userdata, flags, rc):
    """Callback chiamata alla connessione al broker."""
    if rc == 0:
        print(f"✅ Connessione stabilita con successo al broker {MQTT_BROKER_HOST}")
        
        # 1. Sottoscrizione al topic di risposta
        client.subscribe(TOPIC_SUBSCRIBE_TELEMETRY_RESPONSE)
        print(f"🔔 Sottoscritto al topic di risposta: {TOPIC_SUBSCRIBE_TELEMETRY_RESPONSE}")
        
        # 2. Pubblicazione della richiesta di telemetria
        client.publish(
            TOPIC_PUBLISH_TELEMETRY_REQUEST, 
            TELEMETRY_REQUEST_PAYLOAD, 
            qos=1
        )
        print(f"📤 Pubblicata richiesta di telemetria su: {TOPIC_PUBLISH_TELEMETRY_REQUEST}")
        
    else:
        print(f"❌ Connessione fallita. Codice di ritorno: {rc}")

def on_message(client, userdata, msg):
    """Callback chiamata alla ricezione di un messaggio."""
    print("-" * 40)
    print(f"➡️ Risposta di Telemetria ricevuta su topic: {msg.topic}")
    try:
        data = json.loads(msg.payload.decode("utf-8"))
        print(f"Dati Telemetria ricevuti: {json.dumps(data, indent=2)}")
        
    except json.JSONDecodeError:
        print(f"Payload non JSON: {msg.payload.decode('utf-8')}")
    
    print("-" * 40)

# --- Configurazione del Client MQTT ---

client = mqtt.Client(
    client_id="", 
    protocol=mqtt.MQTTv311
)

# Non stampiamo il token per mantenere la sicurezza
client.username_pw_set(GATEWAY_ACCESS_TOKEN) 

# Impostazione delle callback
client.on_connect = on_connect
client.on_message = on_message

try:
    print(f"Connecting to mqtt://{MQTT_BROKER_HOST}:{MQTT_BROKER_PORT}...")
    client.connect(MQTT_BROKER_HOST, MQTT_BROKER_PORT, 60)
    
    client.loop_forever()

except Exception as e:
    print(f"ERRORE DI CONNESSIONE: {e}")
finally:
    client.disconnect()