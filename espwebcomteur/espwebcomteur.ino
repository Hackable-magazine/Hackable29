#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>
#include <DSP0401B.h>

#define SDI_PIN D8   // data out
#define CLK_PIN D7   // clock
#define LE_PIN  D6   // latch
#define OE_PIN  D5   // output enable

#define MSECOND  1000
#define MMINUTE  60*MSECOND
#define MHOUR    60*MMINUTE
#define MDAY     24*MHOUR

DSP0401B mydisp;

// configuration en EEPROM
struct EEconf {
  char ssid[32];
  char password[64];
  char myhostname[32];
};
EEconf readconf;

unsigned long previousMillis = 0;

// mot de passe pour l'OTA
const char* otapass = "123456";
// gestion du temps pour calcul de la durée de la MaJ
unsigned long otamillis;

// serveur Web
const char* host = "boutique.com";
const int httpsPort = 443;

// Empreinte SHA1 du certificat
const char* fingerprint = "AA BB CC DD EE FF 00 11 22 33 44 55 66 77 88 99 AA BB CC DD";

void confOTA() {
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(readconf.myhostname);
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

void webcollect() {
  // connexion au serveur web
  WiFiClientSecure client;
  Serial.println(F("Connexion au serveur Web"));
  if (!client.connect(host, httpsPort)) {
    Serial.println(F("Erreur connexion serveur web!"));
    return;
  }

  // vérification certificat SSL/TLS
  if (client.verify(fingerprint, host)) {
    Serial.println(F("Certificat ok"));
  } else {
    Serial.println(F("Mauvais certificat !"));
    client.stop();
    return;
  }

  // Authentification
  Serial.println(F("Envoi authentification (POST): "));
  client.print(F("POST /admin/index.php?controller=AdminLogin HTTP/1.1\r\n"
                 "Host: boutique.com\r\n"
                 "User-Agent: curl/7.55.0\r\n"
                 "Authorization: Basic zzzzzzzzzzzzzzzzzSECRETzzzzzzzzzzzzzzzzzzzzzzzz\r\n"
                 "Content-Type: application/x-www-form-urlencoded\r\n"
                 "Content-Length: 55\r\n"
                 "\r\n"
                 "email=adresse%40domaine.fr&passwd=mot2passe&submitLogin="));

  // ligne de réception
  String line;
  // URL de redirection
  String newurl;
  // Token PrestaShop
  String token;
  // positions pour extraction token
  int debtoken;
  int fintoken;
  // Cookie
  String prestacookie;

  // Quantité
  int nbr_papier = 0;
  int nbr_pdf = 0;

  Serial.println(F("request sent"));
  while(client.connected()) {
    line = client.readStringUntil('\n');
    // test code réponse
    if(line.startsWith(F("HTTP/1.1"))) {
      if(!line.startsWith(F("HTTP/1.1 302"))) {
        Serial.println(F("Erreur authentification !"));
        Serial.println(F("Fermeture connexion"));
        client.stop();
        mydisp.sendtext("ERR ERR ");
        return;
      }
    }

    // récupération URL de redirection
    if(line.startsWith(F("Location: "))) {
      newurl = line.substring(10);
      newurl.trim();

      //extraction token
      if((debtoken = newurl.indexOf("token=")) == -1) {
        Serial.println("Pas de token dans l'URL");
        Serial.println(F("Fermeture connexion"));
        client.stop();
        mydisp.sendtext("ERR ERR ");
        return;
      }
      if((fintoken = newurl.indexOf("&", debtoken)) == -1)
        token = newurl.substring(debtoken);
      else
        token = newurl.substring(debtoken, fintoken);
    }

    // récupération cookie
    if(line.startsWith(F("Set-Cookie: PrestaShop-"))) {
      int pos = line.indexOf(';',12);
      if(pos>0) {
        prestacookie = line.substring(12,pos);
      }
    }
    // fin de l'entête HTTP
    if (line == "\r") {
      Serial.println(F("entête ok"));
      break;
    }
  }

  // Authentifié. Collecte de données
  Serial.println(F("Collecte des infos"));

  // Nouvelle requête (partie non présente dans la redirection)
  client.print(String("GET /admin/") + newurl + "&view_sub_deliveries_sched" + " HTTP/1.1\r\n"
    + "Host: boutique.com\r\n"
    + "User-Agent: curl/7.55.0\r\n"
    + "Authorization: Basic zzzzzzzzzzzzzzzzzSECRETzzzzzzzzzzzzzzzzzzzzzzzz\r\n"
    + "Cookie: " + prestacookie + ";\r\n"
    + "\r\n"
    );

  // Récupération entête
  while(client.connected()) {
    line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println(F("entête ok"));
      break;
    }
  }

  // Analyse de la page HTML
  while(client.connected()) {
    line = client.readStringUntil('\n');
    if(line.indexOf("Famille <b>Hackable Magazine") != -1) {
      line = client.readStringUntil('\n');
      line.trim();
      if(!nbr_papier) {
        nbr_papier = line.substring(line.indexOf("<b>")+3).toInt();
      } else {
        nbr_pdf = line.substring(line.indexOf("<b>")+3).toInt();
      }
    }
    if(nbr_papier && nbr_pdf) {
      Serial.println(F("Information obtenues"));
      break;
    }
  }

  Serial.println(F("Fermeture connexion"));
  client.stop();

  Serial.println(F("-----------------------"));
  Serial.print(F("Papier = "));
  Serial.println(nbr_papier);
  Serial.print(F("PDF = "));
  Serial.println(nbr_pdf);
  Serial.println(F("-----------------------"));

  char strbuffer[9];

  snprintf(strbuffer,sizeof(strbuffer),"%04d%04d",nbr_papier, nbr_pdf);
  mydisp.sendtext(strbuffer);
}

void setup() {
  // Moniteur série
  Serial.begin(115200);
  Serial.println();

  // afficheur
  mydisp.begin(2, SDI_PIN, CLK_PIN, LE_PIN, OE_PIN);

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

  // configuration OTA
  confOTA();

  webcollect();
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= (MMINUTE*30)) {
    previousMillis = currentMillis;
      webcollect();
  }
  // gestion OTA
  ArduinoOTA.handle();
}
