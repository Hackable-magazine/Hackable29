#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <NtpClientLib.h>

#define MSECOND  1000
#define MMINUTE  60*MSECOND
#define MHOUR    60*MMINUTE
#define MDAY     24*MHOUR

// configuration en EEPROM
struct EEconf {
  char ssid[32];
  char password[64];
  char myhostname[32];
};
EEconf readconf;

// mot de passe pour l'OTA
const char* otapass = "123456";
// gestion du temps pour calcul de la durée de la MaJ
unsigned long otamillis;

unsigned long previousMillis = 0;

ESP8266WebServer server(80);
Adafruit_BME280 bme;

void confOTA() {
  // Port 8266 (défaut)
  ArduinoOTA.setPort(8266);

  // Hostname défaut : esp8266-[ChipID]
  ArduinoOTA.setHostname(readconf.myhostname);

  // mot de passe pour OTA
  ArduinoOTA.setPassword(otapass);

  ArduinoOTA.onStart([]() {
    Serial.println("/!\\ Maj OTA");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\n/!\\ MaJ terminee");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progression: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

void adddata() {
  char strbuffer[64];
  snprintf(strbuffer, 64, "\"%s\";%.2f;%.2f;%.2f\n",
    NTP.getTimeDateString().c_str(),
    bme.readTemperature(),
    bme.readHumidity(),
    bme.readPressure()/100.0);
  Serial.println(strbuffer);
  File f = SPIFFS.open("/data.csv", "a+");
  if (!f) {
    Serial.println("erreur ouverture fichier!");
  } else {
      f.print(strbuffer);
      f.close();
  }
}

void setup() {
  // Moniteur série
  Serial.begin(115200);
  Serial.println();

  if(!SPIFFS.begin()) {
    Serial.println("Erreur initialisation SPIFFS");
  }

  // Lecture configuration Wifi
  EEPROM.begin(sizeof(readconf));
  EEPROM.get(0, readconf);

  // Connexion au Wifi
  Serial.print(F("Connexion Wifi AP"));
  WiFi.mode(WIFI_STA);
  WiFi.begin(readconf.ssid, readconf.password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(F("\r\nWiFi connecté"));
  Serial.print(F("Mon adresse IP: "));
  Serial.println(WiFi.localIP());

  server.serveStatic("/", SPIFFS, "/index.html");
  server.serveStatic("/index.html", SPIFFS, "/index.html");
  server.serveStatic("/Chart.bundle.min.js", SPIFFS, "/Chart.bundle.min.js");
  server.serveStatic("/data.csv", SPIFFS, "/data.csv");
  server.serveStatic("/mytooltips.js", SPIFFS, "/mytooltips.js");
  server.serveStatic("/papaparse.min.js", SPIFFS, "/papaparse.min.js");
  server.serveStatic("/utils.js", SPIFFS, "/utils.js");
  server.begin();

  Wire.begin(D2, D1);
  if (!bme.begin(0x76)){
    Serial.println(F("Erreur BME280"));
  }

  Serial.print(F("Température: "));
  Serial.println(bme.readTemperature());
  Serial.print(F("Pression: "));
  Serial.println(bme.readPressure()/100.0);
  Serial.print(F("Humidité: "));
  Serial.println(bme.readHumidity());

  // configuration NTP
  NTP.begin("europe.pool.ntp.org", 1, true, 0);

  // configuration OTA
  confOTA();
}

void loop() {
  char strbuffer[64];
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= MHOUR) {
    previousMillis = currentMillis;
    adddata();
  } 
  // gestion OTA
  ArduinoOTA.handle();
  // gestion serveur HTTP
  server.handleClient();
}
