FROM python:3.11-slim

WORKDIR /app

RUN pip install paho-mqtt supabase python-dotenv

# Copiar el script
COPY middleware/main.py .

# Ejecutar el script sin buffer de consola para ver los logs en tiempo real
CMD ["python", "main.py"]