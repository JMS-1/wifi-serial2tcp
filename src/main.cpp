#include <ESP8266WiFi.h>

#define LED_POWER D1
#define LED_WLAN D7
#define LED_ACTIVE D2

#define SWITCH_MENU D6
#define SWITCH_ENTER D5

WiFiServer server(29111);
WiFiClient client;

enum mode
{
  running,
  wps,
  config
};

mode currentMode = running;
bool serverStarted = false;

// Startet die WPS Konfiguration
bool startWPS()
{
  if (!WiFi.beginWPSConfig())
    return false;

  auto newSSID = WiFi.SSID();

  if (newSSID.length() < 1)
    return false;

  return true;
}

// Setup Funktion
void setup()
{
  pinMode(LED_POWER, OUTPUT);
  pinMode(LED_WLAN, OUTPUT);
  pinMode(LED_ACTIVE, OUTPUT);

  pinMode(SWITCH_MENU, INPUT_PULLUP);
  pinMode(SWITCH_ENTER, INPUT_PULLUP);

  Serial.begin(115200);

  digitalWrite(LED_POWER, HIGH);
  digitalWrite(LED_WLAN, LOW);
  digitalWrite(LED_ACTIVE, LOW);

  auto ssid = WiFi.SSID();
  auto pass = WiFi.psk();

  if (ssid.length() > 0 && pass.length() > 0)
    WiFi.begin(ssid, pass);
}

void selectMenu()
{
  if (digitalRead(SWITCH_MENU))
  {
    while (digitalRead(SWITCH_MENU))
      ;

    delay(500);

    switch (currentMode)
    {
    case running:
      currentMode = wps;

      digitalWrite(LED_POWER, LOW);
      digitalWrite(LED_WLAN, HIGH);

      break;
    case wps:
      currentMode = config;

      digitalWrite(LED_WLAN, LOW);
      digitalWrite(LED_ACTIVE, HIGH);
      break;
    case config:
      currentMode = running;

      digitalWrite(LED_ACTIVE, LOW);
      digitalWrite(LED_POWER, HIGH);
      break;
    }
  }
}

void checkEnter()
{
  if (currentMode == running)
    return;

  if (digitalRead(SWITCH_ENTER))
  {
    while (digitalRead(SWITCH_ENTER))
      ;

    delay(500);

    switch (currentMode)
    {
    case wps:
      if (startWPS())
      {
        currentMode = running;

        digitalWrite(LED_WLAN, LOW);
        digitalWrite(LED_POWER, HIGH);

        Serial.println("Verbunden via WPS");
      }
      else
      {
        for (auto i = 10; i-- > 0;)
        {
          digitalWrite(LED_WLAN, LOW);
          delay(100);
          digitalWrite(LED_WLAN, HIGH);
          delay(100);
        }

        Serial.println("Keine Verbindung über WPS herstellbar");
      }

      break;
    }
  }
}

void checkWiFi()
{
  if (currentMode != running)
    return;

  auto connected = WiFi.status() == WL_CONNECTED;

  digitalWrite(LED_WLAN, connected ? LOW : HIGH);

  if (!connected || serverStarted)
    return;

  serverStarted = true;

  server.begin();
  server.setNoDelay(true);
}

void checkServer()
{
  if (!serverStarted)
    return;

  if (!server.hasClient())
    return;

  if (client)
    server.accept().abort();
  else
    client = server.accept();
}

void checkClient()
{
  digitalWrite(LED_ACTIVE, client ? HIGH : LOW);

  while (client.available() && Serial.availableForWrite() > 0)
  {
    auto data = client.read();

    Serial.write(data);
  }
}

void loop()
{
  selectMenu();
  checkEnter();
  checkWiFi();
  checkServer();
  checkClient();
}