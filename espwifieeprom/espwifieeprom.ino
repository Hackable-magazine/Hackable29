#include <ESP8266WiFi.h>
#include <EEPROM.h>

// structure pour stocker les infos
struct EEconf {
  char ssid[32];
  char password[64];
  char myhostname[32];
};

void setup() {
  // déclaration et initialisation
  EEconf myconf = {
    "APmonSSID",
    "phrase2passewifi",
    "nomhote"
  };

  // seconde variable pour la (re)lecture
  EEconf readconf;

  Serial.begin(115200);

  // initialisation EEPROM
  EEPROM.begin(sizeof(myconf));
  // enregistrement
  EEPROM.put(0, myconf);
  EEPROM.commit();

  // relecture et affichage
  EEPROM.get(0, readconf);
  Serial.println("\n\n\n");
  Serial.println(readconf.ssid);
  Serial.println(readconf.password);
  Serial.println(readconf.myhostname);
}

void loop() {
}

