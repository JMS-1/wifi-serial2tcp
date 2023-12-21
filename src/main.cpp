#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <regex.h>

// LED signaling menu mode (or missing configuration) GREEN
#define LED_MENU D1

// LED signaling missing WLAN connection BLUE
#define LED_WLAN D7

// LED signaling active client TCP connection RED
#define LED_ACTIVE D2

// Button to switch through menu.
#define SWITCH_MENU D6

// Button to confirm menu entry.
#define SWITCH_ENTER D5

enum mode
{
  // Regular running mode.
  running,
  // Show WPS menu.
  wps,
  // Show configuration menu.
  config
};

// Always start in regular mode.
mode currentMode = running;

// TCP server provided - will be opened as soon as a WLAN connection is detected.
WiFiServer server(29111);
bool serverStarted = false;

// The one and only TCP client.
WiFiClient client;

// Current configuration.
char configuration[100] = "";
int configurationIndex = -1;

// Pending password from TCP client.
char password[100] = "";
int passwordIndex = -1;

// Configuration data.
String authorization;
int baudRate = -1;
int dataBits = -1;
int stopBits = -1;
int parity = -1;

// Regular expression detecting valid configurations.
regex_t reg = {0};
int regErr = regcomp(&reg, "^([^;]+);([1-9][0-9]{0,7});(8);([12]);(0)$", REG_EXTENDED);

// Try to connect to WLAN through WPS.
bool configureWiFi()
{
  // Run the algorithm.
  if (!WiFi.beginWPSConfig())
    return false;

  // Validate the SSID to check for a valid connection.
  return WiFi.SSID().length() > 0;
}

// Load the configuration from the EEPROM and try to parse it.
void readConfiguration()
{
  // Load the full string.
  for (auto addr = sizeof(configuration); addr-- > 0;)
    configuration[addr] = EEPROM.read(addr);

  // Check for empty EEPROM - values will all be 0xff not 0x00.
  if (configuration[0] == 255)
    configuration[0] = 0;

  // See if configuration can be parsed - really should be never occur.
  if (regErr)
    return;

  // Parse the configuration: <Password>;<Baud rate>;<Number of data bits>;<Number of stop bits>;<Parity mode>
  String config(configuration);

  regmatch_t regmatch[6] = {0};

  if (regexec(&reg, config.c_str(), sizeof(regmatch) / sizeof(regmatch[0]), regmatch, 0))
    return;

  // Extract the result.
  authorization = config.substring(regmatch[1].rm_so, regmatch[1].rm_eo);

  baudRate = String(config.substring(regmatch[2].rm_so, regmatch[2].rm_eo)).toInt();
  dataBits = String(config.substring(regmatch[3].rm_so, regmatch[3].rm_eo)).toInt();
  stopBits = String(config.substring(regmatch[4].rm_so, regmatch[4].rm_eo)).toInt();
  parity = String(config.substring(regmatch[5].rm_so, regmatch[5].rm_eo)).toInt();

  // Must collect the password.
  passwordIndex = 0;
}

// Update the configuration in the EEPROM - use as seldom as possible.
void writeConfiguration(const String &config)
{
  // Never overwrite the buffer allocated.
  auto chars = min((uint)(sizeof(configuration) - 1), config.length());

  for (uint i = 0; i < chars; i++)
    EEPROM.write(i, config[i]);

  // Always add a terminating 0.
  EEPROM.write(chars, 0);

  // Update EEPROM.
  EEPROM.commit();
}

// Run once when the chip is started.
void setup()
{
  // Reserve our EEPROM area, load and parse the configuration from there.
  EEPROM.begin(sizeof(configuration));

  readConfiguration();

  // Configure output LEDs.
  pinMode(LED_MENU, OUTPUT);
  pinMode(LED_WLAN, OUTPUT);
  pinMode(LED_ACTIVE, OUTPUT);

  // Configure input buttons.
  pinMode(SWITCH_MENU, INPUT_PULLUP);
  pinMode(SWITCH_ENTER, INPUT_PULLUP);

  // Configure the serial connection according to the configuration - currently only the number of stop bits is selectable.
  if (authorization.length() > 0 && dataBits == 8 && parity == 0)
  {
    Serial.begin(baudRate, stopBits == 1 ? SerialConfig::SERIAL_8N1 : SerialConfig::SERIAL_8N2);
  }

  // Switch all LED off.
  digitalWrite(LED_MENU, LOW);
  digitalWrite(LED_WLAN, LOW);
  digitalWrite(LED_ACTIVE, LOW);

  // Try to connect to the last WLAN discovered during WPS.
  auto ssid = WiFi.SSID();
  auto pass = WiFi.psk();

  if (ssid.length() > 0 && pass.length() > 0)
    WiFi.begin(ssid, pass);
}

// Check for operating the menu button.
void selectMenu()
{
  // Trigger on full DOWN/UP sequence.
  if (digitalRead(SWITCH_MENU))
  {
    while (digitalRead(SWITCH_MENU))
      ;

    // Avoid duplicated pulse detection.
    delay(500);

    switch (currentMode)
    {
    case running:
      // Visualize WPS menu mode.
      currentMode = wps;

      digitalWrite(LED_ACTIVE, LOW);
      digitalWrite(LED_MENU, HIGH);
      digitalWrite(LED_WLAN, HIGH);

      break;
    case wps:
      // Visualize configuration menu mode.
      currentMode = config;

      digitalWrite(LED_WLAN, LOW);
      digitalWrite(LED_ACTIVE, HIGH);
      break;
    case config:
      // Go back to regular operation mode.
      currentMode = running;

      digitalWrite(LED_ACTIVE, LOW);
      digitalWrite(LED_MENU, LOW);
      break;
    }
  }
}

// See if a menu has been selected.
void checkEnter()
{
  // Not in menu mode.
  if (currentMode == running)
    return;

  // Check for a full DOWN/UP of the confirmation button.
  if (digitalRead(SWITCH_ENTER))
  {
    while (digitalRead(SWITCH_ENTER))
      ;

    // Be aware of duplicate triggers.
    delay(500);

    switch (currentMode)
    {
    case running:
      break;
    case wps:
      // Run the WPS algorithm.
      if (configureWiFi())
      {
        // Did work just leave menu mode - WLAN connection may take some time afterwards.
        currentMode = running;

        digitalWrite(LED_WLAN, LOW);
        digitalWrite(LED_MENU, LOW);
      }
      else
      {
        // Visualize failing WPS and stay in WPS menu mode.
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
      // Simple wipe out the configuration to allow for following update through the TCP connection.
      writeConfiguration("");

      // Restart the chip.
      ESP.restart();

      break;
    }
  }
}

// See if WLAN is up.
void checkWiFi()
{
  // We are in menu mode.
  if (currentMode != running)
    return;

  // Visualize current situation - WLAN connections isself and usable configuration.
  auto connected = WiFi.status() == WL_CONNECTED;

  digitalWrite(LED_WLAN, connected ? LOW : HIGH);
  digitalWrite(LED_MENU, connected && authorization.length() < 1 ? HIGH : LOW);

  // On first connection just start the TCP server.
  if (!connected || serverStarted)
    return;

  serverStarted = true;

  server.begin();
  server.setNoDelay(true);

  // If there is no valid configuration so far the first TCP client may set one.
  if (authorization.length() > 0)
    configurationIndex = -1;
  else
    configurationIndex = 0;
}

// See if a new TCP client wants to connect to our server.
void testPendingClient()
{
  // Server could not yet be started due to a missing WLAN connection.
  if (!serverStarted)
    return;

  // See if there is a client waiting.
  if (!server.hasClient())
    return;

  // Only allow a single active client at each time.
  if (client)
    server.accept().abort();
  else
  {
    client = server.accept();

    // Must authorize.
    passwordIndex = 0;
  }
}

// See if there is data coming from the client.
void checkClient()
{
  // We are in menu mode.
  if (currentMode != running)
    return;

  // Visualize an active client.
  digitalWrite(LED_ACTIVE, client ? HIGH : LOW);

  while (client.available())
  {
    // Retrieve the next byte.
    auto data = client.read();

    if (configurationIndex >= 0)
    {
      // Collect the configuration from the very first client connection.
      configuration[configurationIndex] = data == 13 ? 0 : data;

      // Make sure out buffer is never overrun.
      if ((uint)configurationIndex < sizeof(configuration) - 1)
        configurationIndex++;

      // On end-of-line (indicated by a carriage return) write the configuration to the EEPROM and restart the chip.
      if (data == 13)
      {
        writeConfiguration(configuration);

        ESP.restart();
      }
    }
    else if (passwordIndex >= 0 && authorization.length() > 0)
    {
      // Collect the first input lineand expect it to be the password.
      password[passwordIndex] = data == 13 ? 0 : data;

      // Brute force protection against buffer overflow attacks,
      if ((uint)passwordIndex < sizeof(password) - 1)
        passwordIndex++;

      if (data == 13)
      {
        // Check the password.
        if (authorization == password)
          passwordIndex = -1;
        else
        {
          // On failure reset the detection BUT ...
          passwordIndex = 0;

          // ... abort this connection since the client does not now the password.
          client.abort();
        }
      }
    }
    else
      // Forward to serial line as is byte by byte.
      Serial.write(data);
  }
}

// See if there is any data from the serial line.
void checkSerial()
{
  // Get the number of pending characters.
  auto len = Serial.available();

  if (len < 1)
    return;

  // Discatd data - even if we can not process it.
  uint8_t data[len];

  len = Serial.readBytes(data, len);

  if (len < 1)
    return;

  // Not in active mode.
  if (currentMode != running)
    return;

  // No client or not yet authorized.
  if (!client || configurationIndex >= 0 || passwordIndex >= 0 || authorization.length() < 1)
    return;

  // Blind send to client - hopefully fast enough, we do no buffering.
  client.write(data, len);
}

// Regular operation loop.
void loop()
{
  // Menu selection.
  selectMenu();

  // Menu execution.
  checkEnter();

  // WLAN connection and TCP server startup.
  checkWiFi();

  // TCP client connection.
  testPendingClient();

  // TCP client data.
  checkClient();

  // Serial data.
  checkSerial();
}