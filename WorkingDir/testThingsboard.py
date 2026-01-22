import paho.mqtt.client as mqtt
import json
import time
import random

# --- CONFIGURAZIONE ---
THINGSBOARD_HOST = "mqtt.faffofvtt.work"  # O l'IP del tuo server
GATEWAY_ACCESS_TOKEN = "" # Inserisci il token del dispositivo Gateway
MQTT_TOPIC = "v1/gateway/telemetry"

# --- FUNZIONI DI CONNESSIONE ---
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connesso correttamente al Gateway ThingsBoard!")
    else:
        print(f"Errore di connessione. Codice: {rc}")

client = mqtt.Client()
client.username_pw_set(GATEWAY_ACCESS_TOKEN)
client.on_connect = on_connect

client.connect(THINGSBOARD_HOST, 1883, 60)
client.loop_start()

try:
    while True:
        # Struttura del messaggio per il Gateway
        # Ogni chiave nel dizionario principale rappresenta il NOME di un dispositivo figlio
        payload = {
            "Sensore_Alpha": [
                {
                    "ts": int(round(time.time() * 1000)), # Timestamp opzionale (ms)
                    "values": {
                        "temperatura": round(random.uniform(20.0, 25.0), 2),
                        "umidita": random.randint(40, 50)
                    }
                }
            ],
            "Sensore_Beta": [
                {
                    "values": {
                        "voltaggio": round(random.uniform(3.0, 4.2), 2),
                        "stato": "attivo"
                    }
                }
            ]
        }

        # Pubblicazione sul topic specifico del gateway
        client.publish(MQTT_TOPIC, json.dumps(payload), qos=1)
        
        print(f"Dati inviati: {payload}")
        time.sleep(10) # Invia dati ogni 10 secondi

except KeyboardInterrupt:
    print("Chiusura in corso...")
    client.loop_stop()
    client.disconnect()