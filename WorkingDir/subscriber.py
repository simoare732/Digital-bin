import paho.mqtt.client as mqtt
import ssl
import os
from pathlib import Path
import math
import requests
import json

BROKER_HOST = "3224d9e30f954d01a9b9570ad77953f2.s1.eu.hivemq.cloud" 
PORT = 8883 
CLIENT_ID = "subscriber_1"
USERNAME = "bridge" 
TOPIC_TO_SUBSCRIBE= "hivemq/ahfgnsad439/BINs/#"
SUBSCRIBED_DATA = {} 
GATEWAY_TOKEN = ""

MAX_FILL = 50

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
    """callback connection"""
    if rc == 0:
        print("Connected to broker")
        client.subscribe(TOPIC_TO_SUBSCRIBE, qos=1)
        print(f"subscribed to topic'{TOPIC_TO_SUBSCRIBE}'")
    else:
        print(f"connection failed error code: {rc}")

def next_bin_direction(lat1, lon1, lat2, lon2):

    directions = ["U", "R", "D", "L"]

    lat1_rad = math.radians(lat1)
    lon1_rad = math.radians(lon1)
    lat2_rad = math.radians(lat2)
    lon2_rad = math.radians(lon2)

    delta_lon_rad = lon2_rad - lon1_rad

    y = math.sin(delta_lon_rad) * math.cos(lat2_rad)
    
    x = (math.cos(lat1_rad) * math.sin(lat2_rad)) - \
        (math.sin(lat1_rad) * math.cos(lat2_rad) * math.cos(delta_lon_rad))
    
    bearing_rad = math.atan2(y, x)

    bearing_deg = math.degrees(bearing_rad)

    index = round(((bearing_deg + 45) % 360) / 90) % 4
    
    return directions[int(index)]

def bin_distance(lat1, lon1, lat2, lon2):
    EARTH = 6371000

    lat1_rad = math.radians(lat1)
    lon1_rad = math.radians(lon1)
    lat2_rad = math.radians(lat2)
    lon2_rad = math.radians(lon2)

    delta_lat = lat2_rad - lat1_rad
    delta_lon = lon2_rad - lon1_rad
    
    a = (math.sin(delta_lat / 2) ** 2) + \
        (math.cos(lat1_rad) * math.cos(lat2_rad) * (math.sin(delta_lon / 2) ** 2))

    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))

    distance = EARTH * c
    
    return distance

def publish_gateway_data(gateway_token, bin_id, telemetries):
    """
    Invia dati a ThingsBoard tramite un unico Gateway.
    Format: {"NomeCestino": [{"key": "value"}, ...]}
    """
    script_dir = Path(__file__).parent.resolve()
    password_file = script_dir / "token_tib"
    GATEWAY_TOKEN = read_password_from_file(password_file)
    print(GATEWAY_TOKEN)

    url = f"https://thingboard.faffofvtt.work:443/api/v1/{GATEWAY_TOKEN}/telemetry"
    headers = {'Content-Type': 'application/json'}
    
    # Formato richiesto dal Gateway di ThingsBoard
    payload = {
        bin_id: [
            {
                "ts":None,
                "values":  telemetries
            }
        ]
    }
    
    try:
        response = requests.post(url, data=json.dumps(payload), headers=headers, timeout=5)
        if response.status_code == 200:
            print(f"Dati Gateway inviati: {bin_id} -> ThingsBoard")
        else:
            print(f"Errore Gateway: {response.status_code} - {response.text}")
    except Exception as e:
        print(f"Errore connessione Gateway: {e}")

def on_message(client, userdata, msg):
    """Callback new message"""
    bin_id = msg.topic.split("/")[-2]
    data_key = msg.topic.split("/")[-1]
    payload_value = msg.payload.decode()

    if bin_id not in SUBSCRIBED_DATA:
        SUBSCRIBED_DATA[bin_id] = {}
        client.publish(f"hivemq/ahfgnsad439/BINs/{bin_id}/lock", payload=0, qos=1)
        client.publish(f"hivemq/ahfgnsad439/BINs/{bin_id}/lcd", payload="0,ok", qos=1)
        SUBSCRIBED_DATA[bin_id]["lock"] = 0
        SUBSCRIBED_DATA[bin_id]["lcd"] = "0,ok"
        telemetries={"lock":0}
        publish_gateway_data(GATEWAY_TOKEN, bin_id, telemetries)    
        telemetries={"lcd":"0,ok"}
        publish_gateway_data(GATEWAY_TOKEN, bin_id, telemetries)    

    SUBSCRIBED_DATA[bin_id][data_key] = payload_value
    telemetries={data_key:payload_value}
    publish_gateway_data(GATEWAY_TOKEN, bin_id, telemetries)

    if data_key == "fill":
        if int(payload_value) >= MAX_FILL and int(SUBSCRIBED_DATA[bin_id]["lock"])==0:
            client.publish(f"hivemq/ahfgnsad439/BINs/{bin_id}/lock", payload=1, qos=1)
            SUBSCRIBED_DATA[bin_id]["lock"] = 1
            telemetries={"lock":1}
            publish_gateway_data(GATEWAY_TOKEN, bin_id, telemetries)  
            nextBinDistace=1000
            direction="none"
            for key in SUBSCRIBED_DATA:
                if key != bin_id:
                    if int(SUBSCRIBED_DATA[key]["lock"]) == 0:
                        tmpDistance = bin_distance(float(SUBSCRIBED_DATA[bin_id]["lat"]), float(SUBSCRIBED_DATA[bin_id]["lon"]), float(SUBSCRIBED_DATA[key]["lat"]), float(SUBSCRIBED_DATA[key]["lon"]))
                        if tmpDistance < nextBinDistace:
                            nextBinDistace = tmpDistance
                            direction = next_bin_direction(float(SUBSCRIBED_DATA[bin_id]["lat"]), float(SUBSCRIBED_DATA[bin_id]["lon"]), float(SUBSCRIBED_DATA[key]["lat"]), float(SUBSCRIBED_DATA[key]["lon"]))
                        
            if nextBinDistace < 1000:
                nextBinDistace=round(nextBinDistace, 0)
                client.publish(f"hivemq/ahfgnsad439/BINs/{bin_id}/lcd", payload=f"{nextBinDistace, 0},{direction}", qos=1)
                SUBSCRIBED_DATA[bin_id]["lcd"] = f"{nextBinDistace},{direction}"
                telemetries={"lcd":f"{nextBinDistace},{direction}"}
                publish_gateway_data(GATEWAY_TOKEN, bin_id, telemetries)  
            else:
                client.publish(f"hivemq/ahfgnsad439/BINs/{bin_id}/lcd", payload="0,ok", qos=1)
                SUBSCRIBED_DATA[bin_id]["lcd"] = "0,ok"
                telemetries={"lcd":"0,ok"}
                publish_gateway_data(GATEWAY_TOKEN, bin_id, telemetries)   

        elif(int(SUBSCRIBED_DATA[bin_id]["lock"])==1 and int(SUBSCRIBED_DATA[bin_id]["fill"])<MAX_FILL):
            client.publish(f"hivemq/ahfgnsad439/BINs/{bin_id}/lock", payload=0, qos=1)
            client.publish(f"hivemq/ahfgnsad439/BINs/{bin_id}/lcd", payload="0,ok", qos=1)
            SUBSCRIBED_DATA[bin_id]["lock"] = 0
            SUBSCRIBED_DATA[bin_id]["lcd"] = "0,ok"
            telemetries={"lock":0}
            publish_gateway_data(GATEWAY_TOKEN, bin_id, telemetries)    
            telemetries={"lcd":"0,ok"}
            publish_gateway_data(GATEWAY_TOKEN, bin_id, telemetries)    

    print(SUBSCRIBED_DATA)
    print("\n")

def on_publish(client, userdata, mid):
    pass 

def main():

    script_dir = Path(__file__).parent.resolve()
    password_file = script_dir / "token.txt"
    PASSWORD = read_password_from_file(password_file)

    if not PASSWORD:
        print(f"Error password can not be null")
        return -1


    client = mqtt.Client(client_id=CLIENT_ID, protocol=mqtt.MQTTv311)
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_publish = on_publish

    if USERNAME and PASSWORD:
        client.username_pw_set(username=USERNAME, password=PASSWORD)
        
        if PORT == 8883:
            client.tls_set(tls_version=ssl.PROTOCOL_TLS)

    try:
        print(f"Try to connect {BROKER_HOST}:{PORT}...")
        client.connect(BROKER_HOST, PORT, 60)
        client.loop_forever()

    except KeyboardInterrupt:
        print("\nProcess stopped by user(Ctrl+C).")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        client.disconnect()
        print("Disconnected.")

if __name__ == "__main__":
    main()