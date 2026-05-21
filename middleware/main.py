import os
import json
import time
import threading
from datetime import datetime, date, timedelta
from fastapi import FastAPI, Query, HTTPException
import httpx
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

if URL and KEY:
    supabase = create_client(URL, KEY)
else:
    print("WARNING: Supabase credentials not found. DB features won't work.")
    supabase = None

app = FastAPI()

def on_message(client, userdata, msg):
    payload = msg.payload.decode(errors="replace")
    try:
        evento = json.loads(payload)
        data = {
            "templocal": float(evento.get("tempLocal", 0)),
            "humlocal": float(evento.get("humLocal", 0)),
            "altlocal": float(evento.get('altLocal') or 0.0),
            "preslocal": float(evento.get("presLocal", 0)),
            "calidadaire": int(evento.get("calidadAire", 0)),
            "vientopronostico": float(evento.get("vientoPronostico", 0)),
            "lluviapronostico": float(evento.get("lluviaPronostico", 0)),
            "temppronostico": float(evento.get("tempPronostico", 0))
        }
        if supabase:
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
if MQTT_USER and MQTT_PASSWORD:
    client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
client.on_connect = on_connect
client.on_message = on_message

def start_mqtt():
    while True:
        try:
            print(f"Intentando conectar al broker en '{BROKER}'...")
            client.connect(BROKER, 1883)
            break
        except Exception as e:
            print(f"Error. Reintentando en 1 segundos...")
            time.sleep(1)
    client.loop_forever()

@app.on_event("startup")
def startup_event():
    mqtt_thread = threading.Thread(target=start_mqtt, daemon=True)
    mqtt_thread.start()

@app.get("/clima")
async def get_clima(fecha: date = Query(default_factory=date.today)):
    try:
        # 1. Obtener lecturas del día
        lecturas = []
        if supabase:
            start_date = f"{fecha}T00:00:00"
            end_date = f"{fecha}T23:59:59.999"
            res = supabase.table("clima").select("*").gte("time", start_date).lte("time", end_date).execute()
            lecturas = res.data

        # 2. Obtener predicciones de OpenMeteo
        url_openmeteo = (
            "https://api.open-meteo.com/v1/forecast"
            "?latitude=5.066&longitude=-75.499"
            "&daily=temperature_2m_max,temperature_2m_min,relative_humidity_2m_mean,"
            "precipitation_probability_max,uv_index_max,precipitation_sum,"
            "surface_pressure_mean,wind_speed_10m_max,wind_direction_10m_dominant"
            "&timezone=auto"
        )
        
        async with httpx.AsyncClient() as http_client:
            response = await http_client.get(url_openmeteo)
            response.raise_for_status()
            meteo_data = response.json()

        # Agrupar predicciones
        predicciones = []
        daily = meteo_data.get("daily", {})
        times = daily.get("time", [])
        
        for i, t in enumerate(times):
            pred = {
                "fecha": t,
                "temperatura_max": daily.get("temperature_2m_max", [])[i] if i < len(daily.get("temperature_2m_max", [])) else None,
                "temperatura_min": daily.get("temperature_2m_min", [])[i] if i < len(daily.get("temperature_2m_min", [])) else None,
                "humedad": daily.get("relative_humidity_2m_mean", [])[i] if i < len(daily.get("relative_humidity_2m_mean", [])) else None,
                "probabilidad_precipitaciones": daily.get("precipitation_probability_max", [])[i] if i < len(daily.get("precipitation_probability_max", [])) else None,
                "uv": daily.get("uv_index_max", [])[i] if i < len(daily.get("uv_index_max", [])) else None,
                "precipitacion_suma": daily.get("precipitation_sum", [])[i] if i < len(daily.get("precipitation_sum", [])) else None,
                "presion": daily.get("surface_pressure_mean", [])[i] if i < len(daily.get("surface_pressure_mean", [])) else None,
                "velocidad_viento": daily.get("wind_speed_10m_max", [])[i] if i < len(daily.get("wind_speed_10m_max", [])) else None,
                "direccion_viento": daily.get("wind_direction_10m_dominant", [])[i] if i < len(daily.get("wind_direction_10m_dominant", [])) else None
            }
            predicciones.append(pred)

        return {
            "fecha_solicitada": str(fecha),
            "lecturas": lecturas,
            "predicciones": predicciones
        }
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)

