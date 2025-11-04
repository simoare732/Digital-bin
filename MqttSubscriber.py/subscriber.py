import paho.mqtt.client as mqtt
import ssl
import os
BROKER_HOST = "3224d9e30f954d01a9b9570ad77953f2.s1.eu.hivemq.cloud" 
PORT = 8883 
CLIENT_ID = "subscriber_1"
USERNAME = "bridge" 

TOPIC_TO_SUBSCRIBE= "hivemq/ahfgnsad439/BINs/#"

SUBSCRIBED_DATA = {} 

def read_password_from_file(file_name="/home/francesco/Unimore/Digital-bin/MqttSubscriber.py/token.txt"):
    try:
        with open(file_name, 'r', encoding='utf-8') as file:
            password = file.read().strip()
            return password
            
    except FileNotFoundError:
        print(f"❌ Error: The file '{file_name}' was not found.")
        return None
    except Exception as e:
        print(f"⚠️ An error occurred while reading the file: {e}")
        return None

PASSWORD = read_password_from_file()


# --- FUNZIONI DI CALLBACK ---
def on_connect(client, userdata, flags, rc):
    """callback connection"""
    if rc == 0:
        print("Connected to broker")
        client.subscribe(TOPIC_TO_SUBSCRIBE, qos=1)
        print(f"subscribed to topic'{TOPIC_TO_SUBSCRIBE}'")
    else:
        print(f"connection failed error code: {rc}")

def on_message(client, userdata, msg):
    """Callback new message"""
    bin_id = msg.topic.split("/")[-2]
    data_key = msg.topic.split("/")[-1]
    payload_value = msg.payload.decode()

    if bin_id not in SUBSCRIBED_DATA:
        SUBSCRIBED_DATA[bin_id] = {}
    SUBSCRIBED_DATA[bin_id][data_key] = payload_value
    print(SUBSCRIBED_DATA)


def main():
    client = mqtt.Client(client_id=CLIENT_ID, protocol=mqtt.MQTTv311)

    client.on_connect = on_connect
    client.on_message = on_message

    if USERNAME and PASSWORD:
        client.username_pw_set(username=USERNAME, password=PASSWORD)
        
        if PORT == 8883:
            client.tls_set(tls_version=ssl.PROTOCOL_TLS)

    try:
        print(f"Try to connect {BROKER_HOST}:{PORT}...")
        client.connect(BROKER_HOST, PORT, 60)
        client.loop_forever()

    except KeyboardInterrupt:
        print("\nInterrotto dall'utente (Ctrl+C).")
    except Exception as e:
        print(f"Errore: {e}")
    finally:
        client.disconnect()
        print("Disconnesso.")

if __name__ == "__main__":
    main()