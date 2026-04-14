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

Adafruit_BME280 bme;
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define I2C_SDA 17
#define I2C_SCL 18

const int pinMQ135 = 2;
const int pinLDR = 1;
const int lcdCols = 16;
const int lcdRows = 2;
const unsigned long tiempoTituloMs = 2000;
const unsigned long scrollPasoMs = 450;
const unsigned long pausaDatoMs = 1000;

// ---OpenMeteo---
// Trae la velocidad del viento actual y la probabilidad máxima de lluvia de hoy
const String urlMeteo = "https://api.open-meteo.com/v1/forecast?latitude=5.066&longitude=-75.499&current=temperature_2m,wind_speed_10m&daily=precipitation_probability_max&timezone=America/Bogota&forecast_days=1";

DatosLocales datosLocales;
Pronosticos pronosticos;

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
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(urlMeteo);
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String payload = http.getString();
      
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      
      pronosticos.vientoPronostico = doc["current"]["wind_speed_10m"];
      pronosticos.lluviaPronostico = doc["daily"]["precipitation_probability_max"][0];
      pronosticos.tempPronostico = doc["current"]["temperature_2m"];
    } else {
      Serial.print("Error en peticion HTTP: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
}

void leerSensores() {
  datosLocales.tempLocal = bme.readTemperature();
  datosLocales.altLocal = bme.readAltitude(0);
  datosLocales.presLocal = bme.readPressure() / 100.0F;
  datosLocales.calidadAire = analogRead(pinMQ135);
  datosLocales.luz = analogRead(pinLDR);
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

  // Espacios iniciales para que el texto entre de derecha a izquierda.
  String cinta = String("                ") + mensaje;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(encabezadoAjustado);

  int pasos = cinta.length() - lcdCols;
  if (pasos < 0) {
    pasos = 0;
  }

  for (int i = 0; i <= pasos; i++) {
    lcd.setCursor(0, 1);
    lcd.print(cinta.substring(i, i + lcdCols));
    delay(scrollPasoMs);
  }

  delay(pausaDatoMs);
}

void mostrarPronosticos() {
  leerSensores();

  obtenerPronosticos();

  Serial.println("--- Datos de la Estacion ---");
  Serial.printf("Temp Local: %.2f *C\n", datosLocales.tempLocal);
  Serial.printf("Altura Local: %.2f m\n", datosLocales.altLocal);
  Serial.printf("Calidad Aire (Raw): %d\n", datosLocales.calidadAire);
  Serial.printf("Luminosidad (Raw): %d\n", datosLocales.luz);
  Serial.printf("Viento Pronosticado: %.2f km/h\n", pronosticos.vientoPronostico);
  Serial.printf("Probabilidad Lluvia: %.2f %%\n\n", pronosticos.lluviaPronostico);

  mostrarTituloSeccion("Datos locales");
  mostrarCinta("Datos locales", "Temp: " + String(datosLocales.tempLocal, 1) + " C");
  mostrarCinta("Datos locales", "Presion: " + String(datosLocales.presLocal, 1) + " hPa");
  mostrarCinta("Datos locales", "Altura: " + String(datosLocales.altLocal, 1) + " m");
  mostrarCinta("Datos locales", "Aire RAW: " + String(datosLocales.calidadAire));
  mostrarCinta("Datos locales", "Luz RAW: " + String(datosLocales.luz));

  mostrarTituloSeccion("Pronostico");
  mostrarCinta("Pronostico", "Temp: " + String(pronosticos.tempPronostico, 1) + " C");
  mostrarCinta("Pronostico", "Viento: " + String(pronosticos.vientoPronostico, 1) + " km/h");
  mostrarCinta("Pronostico", "Lluvia: " + String(pronosticos.lluviaPronostico, 1) + " %");
}

void reconectarMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Intentando conexión MQTT...");
    // Crear un ID de cliente aleatorio para evitar colisiones
    String clientId = "ESP32S3-Weather-";
    clientId += String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("Conectado al broker MQTT");
    } else {
      Serial.print("Falló, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Intentando de nuevo en 5 segundos...");
      delay(5000);
    }
  }
}

void enviarDatosMQTT() {
  if (!mqttClient.connected()) {
    reconectarMQTT();
  }
  mqttClient.loop();

  // Creamos el documento JSON con capacidad suficiente
  StaticJsonDocument<256> doc;

  // Asignamos las lecturas de los sensores locales
  doc["tempLocal"] = datosLocales.tempLocal;
  doc["altLocal"] = datosLocales.altLocal;
  doc["presLocal"] = datosLocales.presLocal;
  doc["calidadAire"] = datosLocales.calidadAire;
  doc["luz"] = datosLocales.luz;

  // Asignamos los datos obtenidos de OpenMeteo
  doc["vientoPronostico"] = pronosticos.vientoPronostico;
  doc["lluviaPronostico"] = pronosticos.lluviaPronostico;
  doc["tempPronostico"] = pronosticos.tempPronostico;

  char buffer[256];
  size_t n = serializeJson(doc, buffer);

  if (mqttClient.publish("estacion/clima", buffer, n)) {
    Serial.println("Datos de clima publicados correctamente:");
    Serial.println(buffer);
  } else {
    Serial.println("Error al publicar en MQTT.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Iniciando estación meteorológica...");
  
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.printf("I2C iniciado en SDA=%d, SCL=%d\n", I2C_SDA, I2C_SCL);

  incializarLcd();

  Serial.println(bme.sensorID());
  if (!bme.begin(0x76)) { 
    Serial.println("No se encontro un BME280 valido, revisa las conexiones!");
  }

  conectarWiFi();
  lcd.clear();

  mqttClient.setServer(mqtt_server, 1883);
}

void loop() {
  mostrarPronosticos();
  reconectarMQTT();
  enviarDatosMQTT();

  delay(10000); 
}