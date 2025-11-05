import paho.mqtt.client as mqtt
import time
import ssl
import json 

# --- CONFIGURATION ---
BROKER_HOST = "3224d9e30f954d01a9b9570ad77953f2.s1.eu.hivemq.cloud" 
PORT = 8883 
CLIENT_ID = "bridge_publisher"
USERNAME = "bridge" 


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

# --- CALLBACK FUNCTIONS ---

def on_connect(client, userdata, flags, rc):
    """Callback triggered upon connection to the broker."""
    if rc == 0:
        print("✅ Broker Connected successfully!")
    else:
        print(f"❌ Error while connecting with broker. Return code: {rc}")

def on_publish(client, userdata, mid):
    """Callback triggered after the message is published."""
    pass 

def main():

    client = mqtt.Client(client_id=CLIENT_ID, protocol=mqtt.MQTTv311)
    client.on_connect = on_connect
    client.on_publish = on_publish

    if USERNAME and PASSWORD:
        client.username_pw_set(username=USERNAME, password=PASSWORD)
        if PORT == 8883:
            client.tls_set(tls_version=ssl.PROTOCOL_TLS)

    try:
        print(f"Attempting to connect to broker {BROKER_HOST}:{PORT}...")
        client.connect(BROKER_HOST, PORT, 60)
        
        client.loop_start() 

        while True:
            # --- Dummy Data Generation (Replace with actual data fetching) ---
            id_ = 1 
            data = {
                
                "position_lat": 41.9028, 
                "position_long": 12.4964, 
                "overturn": False,
                "fill": 40
            }

            id_1 = 2 
            data_1 = {
                "position_lat": 41.9078, 
                "position_long": 12.4964, 
                "overturn": False,
                "fill": 10
            }

            TOPIC_BASE = f"hivemq/ahfgnsad439/BINs/{id_}"
            for key, value in data.items():
                
                full_topic = f"{TOPIC_BASE}/{key}"
                payload_str = str(value) 
                
                client.publish(full_topic, payload=payload_str, qos=1)
                
                print(f"Sent: '{payload_str}' to topic: '{full_topic}'")

            TOPIC_BASE = f"hivemq/ahfgnsad439/BINs/{id_1}"
            for key, value in data_1.items():
                
                full_topic = f"{TOPIC_BASE}/{key}"
                payload_str = str(value) 
                
                client.publish(full_topic, payload=payload_str, qos=1)
                
                print(f"Sent: '{payload_str}' to topic: '{full_topic}'")


            time.sleep(2) 

    except KeyboardInterrupt:
        print("\nPublisher stopped by user (Ctrl+C).")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")

    finally:
        # 5. DISCONNECTION & CLEANUP
        client.loop_stop()
        client.disconnect()
        print("Disconnected from broker.")


if __name__ == "__main__":
    main()

