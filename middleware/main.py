import os
import json
import time
import paho.mqtt.client as mqtt
from supabase import create_client
from dotenv import load_dotenv

print("Iniciando middleware...")
load_dotenv()

URL = os.getenv("URL")
KEY = os.getenv("KEY")
BROKER = "mqtt.cokeinsz.com"
MQTT_USER = os.getenv("MQTT_USER")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD")

supabase = create_client(URL, KEY)

def on_message(client, userdata, msg):
    payload = msg.payload.decode(errors="replace")

    try:
        evento = json.loads(payload)

        data = {
            "templocal": float(evento.get("tempLocal", 0)),
            "altlocal": float(evento.get('altLocal') or 0.0),
            "preslocal": float(evento.get("presLocal", 0)),
            "calidadaire": int(evento.get("calidadAire", 0)),
            "luz": int(evento.get("luz", 0)),
            "vientopronostico": float(evento.get("vientoPronostico", 0)),
            "lluviapronostico": float(evento.get("lluviaPronostico", 0)),
            "temppronostico": float(evento.get("tempPronostico", 0))
        }

        supabase.table("clima").insert(data).execute()
        print("Datos guardados en Supabase\n")
        print(data)

    except Exception as e:
        print(f"Error procesando mensaje: {e}")

def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        client.subscribe("estacion/clima")
        print("Suscrito a 'estacion/clima'\n")
    else:
        print(f"Error en conexión MQTT: {rc}")

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.username_pw_set(MQTT_USER, MQTT_PASSWORD)

client.on_connect = on_connect
client.on_message = on_message

while True:
    try:
        print(f"Intentando conectar al broker en '{BROKER}'...")
        client.connect(BROKER, 1883)
        break
    except Exception as e:
        print(f"Error al conectar: {e}. Reintentando en 1 segundos...")
        time.sleep(1)

client.loop_forever()