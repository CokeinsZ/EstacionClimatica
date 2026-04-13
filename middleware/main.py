import os
import json
import paho.mqtt.client as mqtt
from supabase import create_client
from dotenv import load_dotenv

load_dotenv()

URL = os.getenv("SUPABASE_URL")
KEY = os.getenv("SUPABASE_KEY")
BROKER = "rabbitmq"
MQTT_USER = os.getenv("MQTT_USER")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD")

supabase = create_client(URL, KEY)

def on_message(client, userdata, msg):
    payload = msg.payload.decode(errors="replace")
    try:
        evento = json.loads(payload)
        data = {
            "tempLocal": float(evento.get("tempLocal", 0)),
            "altLocal": float(evento.get("altLocal", 0)),
            "presLocal": float(evento.get("presLocal", 0)),
            "calidadAire": int(evento.get("calidadAire", 0)),
            "luz": int(evento.get("luz", 0)),
            "vientoPronostico": float(evento.get("vientoPronostico", 0)),
            "lluviaPronostico": float(evento.get("lluviaPronostico", 0)),
            "tempPronostico": float(evento.get("tempPronostico", 0))
        }
        supabase.table("clima").insert(data).execute()
        print("Datos guardados")
    except Exception as e:
        print(f"Error: {e}")

client = mqtt.Client()
client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
client.connect(BROKER, 1883)
client.subscribe("estacion/clima")
client.on_message = on_message

print(f"Escuchando en 'estacion/clima' \n")
client.loop_forever()