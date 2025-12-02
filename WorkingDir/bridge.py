import ssl
import paho.mqtt.client as mqtt
import serial
import threading
import time
import json

SERIAL_PORT = '/dev/ttyACM0'
BAUD_RATE = 9600
ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
#BROKER = 'localhost'
BROKER = '3224d9e30f954d01a9b9570ad77953f2.s1.eu.hivemq.cloud'
PORT = 8883 
CLIENT_ID = "bridge_publisher"
USERNAME = "bridge" 

#topic_pub_fill = '/BINs/+/fill'
#topic_pub_position = '/BINs/+/position'
#topic_pub_overturn = '/BINs/+/overturn'

TOPIC_BASE = f"hivemq/ahfgnsad439/BINs/"
topic_sub_lock = TOPIC_BASE + '+/lock'
topic_sub_lcd = TOPIC_BASE + '+/lcd'

def read_password_from_file(file_name="./MqttSubscriber.py/token.txt"):
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

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected with result code", rc)
        client.subscribe(topic_sub_lock)
        client.subscribe(topic_sub_lcd)
    else:
        print("Connection failed with code", rc)

def on_message(client, userdata, msg):
    """Callback per la ricezione di messaggi MQTT."""
    # Inoltra il messaggio ricevuto (es. 'lock' o dati per 'lcd')
    # direttamente al microcontrollore tramite seriale.
    payload = msg.payload.decode()
    topic = msg.topic
    print(f"MQTT RX | Topic: {topic} | Payload: {payload}")

    payload_bytes = payload.encode('utf-8')

    start_marker = b'\xFE'
    if('lock' in topic):
        topic_marker = b'\x01'
    elif ('lcd' in topic):
        topic_marker = b'\x02'
    else:
        topic_marker = b'\x00'  # Marker di default se non riconosciuto
    end_marker = b'\xFF'

    packet = start_marker + topic_marker + payload_bytes + end_marker

    print(packet)
    print( "\n")

    # Invia il comando al dispositivo seriale
    try:
        ser.write(packet)
    except Exception as e:
        print(f"Errore scrittura seriale: {e}")


def serial_reader():
    """Thread per leggere dati dalla seriale e pubblicarli su MQTT."""
    print("Thread lettore seriale avviato...")
    while True:
        if ser.in_waiting:
            try:
                # Leggi una riga dalla seriale (es. "FILL,1,85")
                line = ser.readline().decode().strip()
                if not line:
                    continue

                print(f"Seriale RX: {line}")
                
                # --- PARSING DEL MESSAGGIO ---
                parts = line.split(',')
                if len(parts) < 2:
                    print(f"Dato seriale non valido (troppo corto): {line}")
                    continue

                msg_type = parts[0].upper() # Tipo (FILL, POSITION, OVERTURN)
                bin_id = parts[1]           # ID del bidone (es. '1', '2')

                payload = None

                # Costruisci topic e payload in base al tipo di messaggio
                if msg_type == 'FILL' and len(parts) == 3:
                    # Formato: FILL,id,valore
                    payload = {"fill": parts[2]}
                
                elif msg_type == 'POSITION' and len(parts) >= 3:
                    # Formato: POSITION,id,lat,lon (o un altro formato)
                    # Qui uniamo lat e lon se sono separati
                    payload = {"lat": float(parts[2]), "lon": float(parts[3])}

                elif msg_type == 'OVERTURN' and len(parts) == 3:
                    # Formato: OVERTURN,id,stato
                    payload = {"overturn": parts[2]}
                
                else:
                    print(f"Dato seriale non riconosciuto: {line}")

                # Se abbiamo un topic e un payload validi, pubblichiamo
                if payload:
                    print(f"MQTT TX | Payload: {payload}")
                    if(msg_type == 'POSITION'):
                        client.publish(f"{TOPIC_BASE}{bin_id}/lat", payload=payload['lat'], qos=1)
                        client.publish(f"{TOPIC_BASE}{bin_id}/lon", payload=payload['lon'], qos=1)
                    else:
                        client.publish(f"{TOPIC_BASE}{bin_id}/{msg_type.lower()}", payload=payload[msg_type.lower()], qos=1)
                    
            except Exception as e:
                print(f"Errore nel thread seriale: {e}")
        time.sleep(0.1) # Piccolo delay per non sovraccaricare la CPU

client = mqtt.Client(client_id=CLIENT_ID, protocol=mqtt.MQTTv311)
client.on_connect = on_connect
client.on_message = on_message
client.username_pw_set(username=USERNAME, password=PASSWORD)
if PORT == 8883:
    client.tls_set(tls_version=ssl.PROTOCOL_TLS)

try:
    client.connect(BROKER, PORT, 60)
except Exception as e:
    print(f"Impossibile connettersi al broker {BROKER}: {e}")
    exit(1)



t = threading.Thread(target=serial_reader, daemon=True)
t.start()
print("MQTT Bridge running...")
client.loop_forever()