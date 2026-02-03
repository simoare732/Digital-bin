import ssl
import paho.mqtt.client as mqtt
import serial
import threading
import time
import json
import os
from pathlib import Path

#MQTT TOPICS
TOPIC_BASE = f"hivemq/ahfgnsad439/BINs/"
topic_sub_lock = TOPIC_BASE + '+/lock'
topic_sub_lcd = TOPIC_BASE + '+/lcd'

#SERIAL CONNECTION
SERIAL_PORT = '/dev/ttyACM0'
BAUD_RATE = 9600
ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)

def read_password_from_file(file_name):
    try:
        with open(file_name, 'r', encoding='utf-8') as file:
            password = file.read().strip()
            return password
            
    except FileNotFoundError:
        print(f"Error: The file '{file_name}' was not found.")
        return None
    except Exception as e:
        print(f"An error occurred while reading the file: {e}")
        return None

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected with result code", rc)
        client.subscribe(topic_sub_lock)
        client.subscribe(topic_sub_lcd)

        #Fake Data
        id_1 = 2 
        data_1 = {
            "lat": 44.628444, 
            "lon": 10.909944, 
            "overturn": False,
            "fill": 10
        }

        TOPIC_BASE = f"hivemq/ahfgnsad439/BINs/{id_1}"
        for key, value in data_1.items():
            
            full_topic = f"{TOPIC_BASE}/{key}"
            payload_str = str(value) 
            
            client.publish(full_topic, payload=payload_str, qos=1)
            print(f"Sent: '{payload_str}' to topic: '{full_topic}'")

        id_1 = 3 
        data_1 = {
            "lat": 44.628694, 
            "lon": 10.907417, 
            "overturn": False,
            "fill": 50
        }

        TOPIC_BASE = f"hivemq/ahfgnsad439/BINs/{id_1}"
        for key, value in data_1.items():
            
            full_topic = f"{TOPIC_BASE}/{key}"
            payload_str = str(value) 
            
            client.publish(full_topic, payload=payload_str, qos=1)
            print(f"Sent: '{payload_str}' to topic: '{full_topic}'")

        id_1 = 4 
        data_1 = {
            "lat": 44.627556, 
            "lon": 10.909667, 
            "overturn": False,
            "fill": 60
        }

        TOPIC_BASE = f"hivemq/ahfgnsad439/BINs/{id_1}"
        for key, value in data_1.items():
            
            full_topic = f"{TOPIC_BASE}/{key}"
            payload_str = str(value) 
            
            client.publish(full_topic, payload=payload_str, qos=1)
            print(f"Sent: '{payload_str}' to topic: '{full_topic}'")
    else:
        print("Connection failed with code", rc)

def on_message(client, userdata, msg):
    payload = msg.payload.decode()
    topic = msg.topic
    print(f"    MQTT RX | Topic: {topic} | Payload: {payload}\n")

    payload_bytes = payload.encode('utf-8')

    start_marker = b'\xFE'
    if('lock' in topic):
        topic_marker = b'\x01'
    elif ('lcd' in topic):
        topic_marker = b'\x02'
    else:
        topic_marker = b'\x00' 
    end_marker = b'\xFF'

    packet = start_marker + topic_marker + payload_bytes + end_marker

    print(f"    SENDING SERIAL COMMAND: {packet}\n")


    # Send command through bridge via serial
    try:
        ser.write(packet)
    except Exception as e:
        print(f"Errore scrittura seriale: {e}")


def serial_reader(client):

    print("Serial Thread running...\n")

    while True:
        if ser.in_waiting:
            try:
                # Read serial line
                line = ser.readline().decode().strip()
                if not line:
                    continue

                #print(f"    Serial RX: {line}\n")
                
                parts = line.split(',')
                if len(parts) < 2:
                    print(f"Log message: {line}\n")
                    continue

                msg_type = parts[0].upper() 
                bin_id = parts[1]          

                payload = None

                if msg_type == 'FILL' and len(parts) == 3:
                    payload = {"fill": parts[2]}
                
                elif msg_type == 'POSITION' and len(parts) >= 3:
                    payload = {"lat": float(parts[2]), "lon": float(parts[3])}

                elif msg_type == 'OVERTURN' and len(parts) == 3:
                    payload = {"overturn": parts[2]}
                
                else:
                    print(f"Serial Error: {line}\n")

                if payload:
                    print(f"    MQTT TX | Payload: {payload}\n")
                    if(msg_type == 'POSITION'):
                        client.publish(f"{TOPIC_BASE}{bin_id}/lat", payload=payload['lat'], qos=1)
                        client.publish(f"{TOPIC_BASE}{bin_id}/lon", payload=payload['lon'], qos=1)
                    else:
                        client.publish(f"{TOPIC_BASE}{bin_id}/{msg_type.lower()}", payload=payload[msg_type.lower()], qos=1)
                    
            except Exception as e:
                print(f"Error serial thread: {e}")


def main():

    #MQTT CONNECTION (HiveMQ)
    BROKER = '3224d9e30f954d01a9b9570ad77953f2.s1.eu.hivemq.cloud'
    PORT = 8883 
    CLIENT_ID = "bridge_publisher"
    USERNAME = "bridge1" 

    #GET HIVEMQ PASSWORD 
    script_dir = Path(__file__).parent.resolve()
    password_file = script_dir / "token.txt"
    PASSWORD = read_password_from_file(password_file)
    if not PASSWORD:
        print(f"Error password can not be null")
        return -1

    client = mqtt.Client(client_id=CLIENT_ID, protocol=mqtt.MQTTv311)
    client.on_connect = on_connect
    client.on_message = on_message
    client.username_pw_set(username=USERNAME, password=PASSWORD)
    if PORT == 8883:
        client.tls_set(tls_version=ssl.PROTOCOL_TLS)

    try:
        client.connect(BROKER, PORT, 60)
    except Exception as e:
        print(f"HiveMQ connection failed {BROKER}: {e}")
        exit(1)


    t = threading.Thread(target=serial_reader, daemon=True, args=(client,))
    t.start()

    print("MQTT Bridge running...\n")
    client.loop_forever()
    

if __name__ == "__main__":
    main()
    
