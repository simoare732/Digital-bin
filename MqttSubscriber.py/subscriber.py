import paho.mqtt.client as mqtt
import ssl
import os
from pathlib import Path
import math

BROKER_HOST = "3224d9e30f954d01a9b9570ad77953f2.s1.eu.hivemq.cloud" 
PORT = 8883 
CLIENT_ID = "subscriber_1"
USERNAME = "bridge" 
TOPIC_TO_SUBSCRIBE= "hivemq/ahfgnsad439/BINs/#"
SUBSCRIBED_DATA = {} 

MAX_FILL = 90

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

    SUBSCRIBED_DATA[bin_id][data_key] = payload_value

    if data_key == "fill":
        if int(payload_value) >= MAX_FILL:
            client.publish(f"hivemq/ahfgnsad439/BINs/{bin_id}/lock", payload=1, qos=1)
            SUBSCRIBED_DATA[bin_id]["lock"] = 1
            nextBinDistace=1000
            direction="none"
            for key in SUBSCRIBED_DATA:
                if key != bin_id:
                    if int(SUBSCRIBED_DATA[key]["lock"]) == 0:
                        tmpDistance = bin_distance(float(SUBSCRIBED_DATA[bin_id]["lat"]), float(SUBSCRIBED_DATA[bin_id]["long"]), float(SUBSCRIBED_DATA[key]["lat"]), float(SUBSCRIBED_DATA[key]["long"]))
                        if tmpDistance < nextBinDistace:
                            nextBinDistace = tmpDistance
                            direction = next_bin_direction(float(SUBSCRIBED_DATA[bin_id]["lat"]), float(SUBSCRIBED_DATA[bin_id]["long"]), float(SUBSCRIBED_DATA[key]["lat"]), float(SUBSCRIBED_DATA[key]["long"]))
                        
            if nextBinDistace < 1000:
                client.publish(f"hivemq/ahfgnsad439/BINs/{bin_id}/lcd", payload=f"{nextBinDistace},{direction}", qos=1)
                SUBSCRIBED_DATA[bin_id]["lcd"] = f"{nextBinDistace},{direction}"
            else:
                client.publish(f"hivemq/ahfgnsad439/BINs/{bin_id}/lcd", payload="0,ok", qos=1)
                SUBSCRIBED_DATA[bin_id]["lcd"] = "0,ok"
        elif(int(SUBSCRIBED_DATA[bin_id]["lock"])==1):
            client.publish(f"hivemq/ahfgnsad439/BINs/{bin_id}/lock", payload=0, qos=1)
            client.publish(f"hivemq/ahfgnsad439/BINs/{bin_id}/lcd", payload="0,ok", qos=1)
            SUBSCRIBED_DATA[bin_id]["lock"] = 0
            SUBSCRIBED_DATA[bin_id]["lcd"] = "0,ok"

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