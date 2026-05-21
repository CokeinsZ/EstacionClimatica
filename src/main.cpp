#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#include <DatosClima.h>

WiFiClient espClient;
PubSubClient mqttClient(espClient);

const char* ssid = "CokeinsZ";
const char* password = "9=EJA2&Di3_5";

const char* mqtt_server = "mqtt.cokeinsz.com";
const char* MQTT_USER = "iot_device";
const char* MQTT_PASSWORD = "SecurePass123!";

#define I2C_SDA 17
#define I2C_SCL 18

const int pinMQ135 = 10;
const int pinMQ7 = 11;
const int pinMQ5 = 12;
const int lcdCols = 16;
const int lcdRows = 2;
const unsigned long tiempoTituloMs = 2000;
const unsigned long scrollPasoMs = 450;
const unsigned long pausaDatoMs = 1000;

Adafruit_BME280 bme;
LiquidCrystal_I2C lcd(0x27, 16, 2);

const String urlMeteo = "https://api.open-meteo.com/v1/forecast?latitude=5.066&longitude=-75.499&current=temperature_2m,wind_speed_10m&daily=precipitation_probability_max&timezone=America/Bogota&forecast_days=1";

DatosLocales datosLocales;
Pronosticos pronosticos;

SemaphoreHandle_t mutexDatos;

void conectarWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado");
  Serial.print("IP local: ");
  Serial.println(WiFi.localIP());
}

void incializarLcd() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Iniciando...");
}

void obtenerPronosticos() {
  HTTPClient http;
  http.begin(urlMeteo);
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    
    xSemaphoreTake(mutexDatos, portMAX_DELAY);
    pronosticos.vientoPronostico = doc["current"]["wind_speed_10m"];
    pronosticos.lluviaPronostico = doc["daily"]["precipitation_probability_max"][0];
    pronosticos.tempPronostico = doc["current"]["temperature_2m"];
    xSemaphoreGive(mutexDatos);
  }

  http.end();
}

void leerSensores() {
  float t = bme.readTemperature();
  float h = bme.readHumidity();
  float a = bme.readAltitude(1013.25);
  float p = bme.readPressure() / 100.0F;
  int mq135 = analogRead(pinMQ135) ;
  int mq7 = analogRead(pinMQ7);
  int mq5 = analogRead(pinMQ5);

  xSemaphoreTake(mutexDatos, portMAX_DELAY);

  datosLocales.tempLocal = t;
  datosLocales.humLocal = h;
  datosLocales.altLocal = a;
  datosLocales.presLocal = p;
  datosLocales.calidadAire.co2 = mq135;
  datosLocales.calidadAire.co = mq7;
  datosLocales.calidadAire.inflamables = mq5;
  
  xSemaphoreGive(mutexDatos);
}

String rellenarDerecha(const String& texto, int longitud) {
  String resultado = texto;
  while (resultado.length() < longitud) {
    resultado += " ";
  }
  return resultado;
}

void mostrarTituloSeccion(const String& titulo) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(rellenarDerecha(titulo.substring(0, lcdCols), lcdCols));
  lcd.setCursor(0, 1);
  lcd.print("Cargando datos  ");
  delay(tiempoTituloMs);
}

void mostrarCinta(const String& encabezado, const String& dato) {
  String encabezadoAjustado = rellenarDerecha(encabezado.substring(0, lcdCols), lcdCols);
  String mensaje = dato + "    ";
  String cinta = String("                ") + mensaje;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(encabezadoAjustado);

  int pasos = cinta.length() - lcdCols;
  if (pasos < 0) pasos = 0;

  for (int i = 0; i <= pasos; i++) {
    lcd.setCursor(0, 1);
    lcd.print(cinta.substring(i, i + lcdCols));
    delay(scrollPasoMs);
  }

  delay(pausaDatoMs);
}

void mostrarDatosEnLCD(Pronosticos pronos, DatosLocales datos) {
  mostrarTituloSeccion("Datos locales");
  mostrarCinta("Datos locales", "Temp: " + String(datos.tempLocal, 1) + " C");
  mostrarCinta("Datos locales", "Presion: " + String(datos.presLocal, 1) + " hPa");
  mostrarCinta("Datos locales", "Altura: " + String(datos.altLocal, 1) + " msnv");
  mostrarCinta("Datos aire", "CO2:" + String(datos.calidadAire.co2) + " CO:" + String(datos.calidadAire.co));
  mostrarCinta("Datos aire", "Inflamables:" + String(datos.calidadAire.inflamables));

  mostrarTituloSeccion("Pronostico");
  mostrarCinta("Pronostico", "Temp: " + String(pronos.tempPronostico, 1) + " C");
  mostrarCinta("Pronostico", "Viento: " + String(pronos.vientoPronostico, 1) + " km/h");
  mostrarCinta("Pronostico", "Lluvia: " + String(pronos.lluviaPronostico, 1) + " %");
}

void imprimirDatosSerial(Pronosticos pronos, DatosLocales datos) {
  Serial.println("=== Datos Locales ===");
  Serial.printf("Temperatura: %.1f C\n", datos.tempLocal);
  Serial.printf("Humedad: %.1f %%\n", datos.humLocal);
  Serial.printf("Altitud: %.1f msnv\n", datos.altLocal);
  Serial.printf("Presión: %.1f hPa\n", datos.presLocal);
  Serial.printf("MQ135: %d\n", datos.calidadAire.co2);
  Serial.printf("MQ7: %d\n", datos.calidadAire.co);
  Serial.printf("MQ5: %d\n", datos.calidadAire.inflamables);

  Serial.println("\n=== Pronóstico ===");
  Serial.printf("Temperatura Pronosticada: %.1f C\n", pronos.tempPronostico);
  Serial.printf("Velocidad del Viento Pronosticada: %.1f km/h\n", pronos.vientoPronostico);
  Serial.printf("Probabilidad de Lluvia Pronosticada: %.1f %%\n", pronos.lluviaPronostico);
}

void reconectarMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Intentando conexión MQTT...");
    String clientId = "ESP32S3-Weather-";
    clientId += String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("Conectado al broker MQTT");
    } else {
      Serial.print("Falló MQTT. Reintento próximo ciclo.");
    }
  }
}

void enviarDatosMQTT() {
  mqttClient.loop();
  StaticJsonDocument<400> mqttDoc;

  xSemaphoreTake(mutexDatos, portMAX_DELAY);
  mqttDoc["tempLocal"] = datosLocales.tempLocal;
  mqttDoc["humLocal"] = datosLocales.humLocal;
  mqttDoc["altLocal"] = datosLocales.altLocal;
  mqttDoc["presLocal"] = datosLocales.presLocal;
  mqttDoc["calidadAire"]["co2"] = datosLocales.calidadAire.co2;
  mqttDoc["calidadAire"]["co"] = datosLocales.calidadAire.co;
  mqttDoc["calidadAire"]["inflamables"] = datosLocales.calidadAire.inflamables;
  mqttDoc["vientoPronostico"] = pronosticos.vientoPronostico;
  mqttDoc["lluviaPronostico"] = pronosticos.lluviaPronostico;
  mqttDoc["tempPronostico"] = pronosticos.tempPronostico;
  xSemaphoreGive(mutexDatos);

  char buffer[400];
  size_t n = serializeJson(mqttDoc, buffer);

  if (mqttClient.publish("estacion/clima", buffer, n)) {
    Serial.println("[Núcleo 1] Datos publicados en MQTT correctamente.");
  } else {
    Serial.println("[Núcleo 1] Error al publicar en MQTT.");
  }
}

void TareaSensoresLCD(void *pvParameters) {
  for (;;) {
    leerSensores();

    xSemaphoreTake(mutexDatos, portMAX_DELAY);
    Pronosticos pronosCopia = pronosticos;
    DatosLocales datosCopia = datosLocales;
    xSemaphoreGive(mutexDatos);

    imprimirDatosSerial(pronosCopia, datosCopia);
    mostrarDatosEnLCD(pronosCopia, datosCopia);

  }
}

void TareaRedes(void *pvParameters) {
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      obtenerPronosticos();

      reconectarMQTT();
      enviarDatosMQTT();

    } else {
      WiFi.reconnect();
    }
    
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Iniciando estación meteorológica...");

  //analogReadResolution(8);

  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.printf("I2C iniciado en SDA=%d, SCL=%d\n", I2C_SDA, I2C_SCL);

  incializarLcd();
  lcd.clear();

  if (!bme.begin(0x76)) { 
    Serial.println("¡No se encontro un BME280 valido!");
  }

  pinMode(pinMQ135, INPUT);
  pinMode(pinMQ7, INPUT);
  pinMode(pinMQ5, INPUT);

  conectarWiFi();
  mqttClient.setServer(mqtt_server, 1883);

  mutexDatos = xSemaphoreCreateMutex();
  obtenerPronosticos();
  xTaskCreatePinnedToCore(
    TareaSensoresLCD,
    "Tarea_Sensores",
    10000,
    NULL,
    1,
    NULL,
    0
  );

  xTaskCreatePinnedToCore(
    TareaRedes,         
    "Tarea_Redes",      
    10000,              
    NULL,               
    1,                  
    NULL,               
    1
  );

}

void loop() {}