#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <regex.h>

#define LED_POWER D1
#define LED_WLAN D7
#define LED_ACTIVE D2

#define SWITCH_MENU D6
#define SWITCH_ENTER D5

enum mode
{
  running,
  wps,
  config
};

mode currentMode = running;

WiFiServer server(29111);
bool serverStarted = false;

WiFiClient client;

char configuration[100] = "";
int configurationIndex = -1;

char password[100] = "";
int passwordIndex = -1;

String authorization;
int baudRate = -1;
int dataBits = -1;
int stopBits = -1;
int parity = -1;

regex_t reg = {0};
int regErr = regcomp(&reg, "^([^;]+);([1-9][0-9]{0,7});(8);([12]);([01])$", REG_EXTENDED);

bool configureWiFi()
{
  if (!WiFi.beginWPSConfig())
    return false;

  auto newSSID = WiFi.SSID();

  if (newSSID.length() < 1)
    return false;

  return true;
}

void readConfiguration()
{
  for (auto addr = sizeof(configuration); addr-- > 0;)
    configuration[addr] = EEPROM.read(addr);

  if (configuration[0] == 255)
    configuration[0] = 0;

  if (regErr)
    return;

  String config(configuration);

  regmatch_t regmatch[6] = {0};

  if (regexec(&reg, config.c_str(), sizeof(regmatch) / sizeof(regmatch[0]), regmatch, 0))
    return;

  authorization = config.substring(regmatch[1].rm_so, regmatch[1].rm_eo);

  baudRate = String(config.substring(regmatch[2].rm_so, regmatch[2].rm_eo)).toInt();
  dataBits = String(config.substring(regmatch[3].rm_so, regmatch[3].rm_eo)).toInt();
  stopBits = String(config.substring(regmatch[4].rm_so, regmatch[4].rm_eo)).toInt();
  parity = String(config.substring(regmatch[5].rm_so, regmatch[5].rm_eo)).toInt();

  passwordIndex = 0;
}

void writeConfiguration(const String &config)
{
  auto chars = min((uint)(sizeof(configuration) - 1), config.length());

  for (uint i = 0; i < chars; i++)
    EEPROM.write(i, config[i]);

  EEPROM.write(chars, 0);
  EEPROM.commit();
}

void setup()
{
  EEPROM.begin(sizeof(configuration));

  readConfiguration();

  pinMode(LED_POWER, OUTPUT);
  pinMode(LED_WLAN, OUTPUT);
  pinMode(LED_ACTIVE, OUTPUT);

  pinMode(SWITCH_MENU, INPUT_PULLUP);
  pinMode(SWITCH_ENTER, INPUT_PULLUP);

  Serial.begin(115200);

  digitalWrite(LED_POWER, LOW);
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

      digitalWrite(LED_POWER, HIGH);
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
      digitalWrite(LED_POWER, LOW);
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
    case running:
      break;
    case wps:
      if (configureWiFi())
      {
        currentMode = running;

        digitalWrite(LED_WLAN, LOW);
        digitalWrite(LED_POWER, LOW);
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
      }

      break;
    case config:
      writeConfiguration("");

      ESP.restart();

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
  digitalWrite(LED_POWER, connected && !configuration[0] ? HIGH : LOW);

  if (!connected || serverStarted)
    return;

  serverStarted = true;

  server.begin();
  server.setNoDelay(true);

  if (configuration[0])
    configurationIndex = -1;
  else
    configurationIndex = 0;
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
  if (currentMode != running)
    return;

  digitalWrite(LED_ACTIVE, client ? HIGH : LOW);

  while (client.available() && Serial.availableForWrite() > 0)
  {
    auto data = client.read();

    if (configurationIndex >= 0)
    {
      configuration[configurationIndex] = data == 13 ? 0 : data;

      if ((uint)configurationIndex < sizeof(configuration) - 1)
        configurationIndex++;

      if (data == 13)
      {
        writeConfiguration(configuration);

        ESP.restart();
      }
    }
    else if (passwordIndex >= 0)
    {
      password[passwordIndex] = data == 13 ? 0 : data;

      if ((uint)passwordIndex < sizeof(password) - 1)
        passwordIndex++;

      if (data == 13)
      {
        if (authorization == password)
          passwordIndex = -1;
        else
        {
          passwordIndex = 0;

          client.abort();
        }
      }
    }
    else
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