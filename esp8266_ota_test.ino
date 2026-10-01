#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Updater.h>

ESP8266WebServer server(80);
#define LED_PIN 4

String ssid;
String password;

const char* version_url =
  "https://raw.githubusercontent.com/Mahesh-rss/esp8266_ota_test/main/version.txt";

const char* firmware_url =
  "https://raw.githubusercontent.com/Mahesh-rss/esp8266_ota_test/main/build/esp8266.esp8266.generic/esp8266_ota_test.ino.bin";

const int CURRENT_VERSION = 5;


#define EEPROM_SIZE 128
#define SSID_ADDR 0
#define PASS_ADDR 64


void loadWiFiCredentials() {

  char storedSSID[64];
  char storedPASS[64];

  EEPROM.begin(EEPROM_SIZE);

  EEPROM.get(SSID_ADDR, storedSSID);
  EEPROM.get(PASS_ADDR, storedPASS);

  storedSSID[63] = '\0';
  storedPASS[63] = '\0';

  ssid = String(storedSSID);
  password = String(storedPASS);

  EEPROM.end();
}


void saveWiFiCredentials(String newSSID, String newPassword) {

  EEPROM.begin(EEPROM_SIZE);

  for (int i = 0; i < 64; i++) {
    EEPROM.write(SSID_ADDR + i, 0);
    EEPROM.write(PASS_ADDR + i, 0);
  }

  for (int i = 0; i < newSSID.length() && i < 63; i++) {
    EEPROM.write(SSID_ADDR + i, newSSID[i]);
  }

  for (int i = 0; i < newPassword.length() && i < 63; i++) {
    EEPROM.write(PASS_ADDR + i, newPassword[i]);
  }

  EEPROM.commit();
  EEPROM.end();
}


bool connectToWiFi() {

  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid.c_str(), password.c_str());

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 20) {

    Serial.print(".");
    delay(500);

    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("WiFi connected");

    Serial.print("WiFi IP: ");
    Serial.println(WiFi.localIP());

    Serial.print("Setup AP IP: ");
    Serial.println(WiFi.softAPIP());

    return true;
  }

  Serial.println("WiFi connection failed");

  return false;
}


void startWiFiConfig() {



  server.on("/", HTTP_GET, []() {
    String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP8266 WiFi Setup</title>
</head>

<body>

<h2>ESP8266 WiFi Setup</h2>

<form action="/save" method="POST">

<label>WiFi SSID</label><br>
<input type="text" name="ssid" required><br><br>

<label>WiFi Password</label><br>
<input type="password" name="password"><br><br>

<input type="submit" value="Save">

</form>

</body>
</html>
)rawliteral";

    server.send(200, "text/html", page);
  });


  server.on("/save", HTTP_POST, []() {
    String newSSID = server.arg("ssid");
    String newPassword = server.arg("password");

    Serial.println();
    Serial.println("Testing new WiFi credentials...");
    Serial.print("SSID: ");
    Serial.println(newSSID);

    WiFi.disconnect();
    delay(500);

    WiFi.begin(newSSID.c_str(), newPassword.c_str());

    int attempts = 0;

    while (WiFi.status() != WL_CONNECTED && attempts < 20) {

      delay(500);
      Serial.print(".");

      attempts++;
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {

      Serial.println("New WiFi credentials are correct");

      saveWiFiCredentials(newSSID, newPassword);

      ssid = newSSID;
      password = newPassword;

      String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
</head>

<body>

<h2>WiFi Saved Successfully</h2>

<p>ESP8266 will restart and connect to the new WiFi.</p>

</body>
</html>
)rawliteral";

      server.send(200, "text/html", page);

      delay(2000);

      ESP.restart();

    } else {

      Serial.println("New WiFi credentials are incorrect");

      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP("ESP8266-Setup");

      String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
</head>

<body>

<h2>WiFi Connection Failed</h2>

<p>Wrong SSID or password.</p>

<p>Credentials were NOT saved.</p>

<a href="/">Try Again</a>

</body>
</html>
)rawliteral";

      server.send(200, "text/html", page);
    }
  });

  server.begin();

  Serial.println("Configuration server started");
}


void checkOTA() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  Serial.println();
  Serial.println("Checking OTA...");

  http.begin(client, version_url);

  int httpCode = http.GET();

  Serial.print("Version HTTP code: ");
  Serial.println(httpCode);

  if (httpCode != HTTP_CODE_OK) {
    Serial.println("Failed to check version");
    http.end();
    return;
  }

  String serverVersion = http.getString();
  serverVersion.trim();

  int serverVersionNumber = serverVersion.toInt();

  Serial.print("Current version: ");
  Serial.println(CURRENT_VERSION);

  Serial.print("Server version: ");
  Serial.println(serverVersionNumber);

  http.end();

  if (serverVersionNumber <= CURRENT_VERSION) {
    Serial.println("No update required");
    return;
  }

  Serial.println("New firmware available");

  http.begin(client, firmware_url);

  httpCode = http.GET();

  Serial.print("Firmware HTTP code: ");
  Serial.println(httpCode);

  if (httpCode != HTTP_CODE_OK) {
    Serial.println("Firmware download failed");
    http.end();
    return;
  }

  int contentLength = http.getSize();

  Serial.print("Firmware size: ");
  Serial.println(contentLength);

  if (contentLength <= 0) {
    Serial.println("Invalid firmware size");
    http.end();
    return;
  }

  if (!Update.begin(contentLength)) {
    Serial.print("Update.begin failed: ");
    Serial.println(Update.getError());
    http.end();
    return;
  }

  Serial.println("OTA memory ready");

  WiFiClient* stream = http.getStreamPtr();

  size_t written = Update.writeStream(*stream);

  Serial.print("Written: ");
  Serial.println(written);

  if (written == contentLength) {

    Serial.println("Firmware written successfully");

    if (Update.end() && Update.isFinished()) {
      Serial.println("OTA successful");
      Serial.println("Restarting...");

      http.end();

      delay(1000);
      ESP.restart();
    } else {
      Serial.print("Update.end failed: ");
      Serial.println(Update.getError());
    }

  } else {
    Serial.println("Firmware write incomplete");
  }

  http.end();
}


void setup() {

  pinMode(LED_PIN, OUTPUT);

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("Starting...");

  loadWiFiCredentials();

  WiFi.mode(WIFI_AP_STA);

  if (WiFi.softAP("ESP8266-Setup")) {
    Serial.println("Setup AP started");
    Serial.print("Setup AP IP: ");
    Serial.println(WiFi.softAPIP());
  }

  if (ssid.length() > 0) {
    connectToWiFi();
  } else {
    Serial.println("Setup AP failed");
  }

  startWiFiConfig();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    checkOTA();
  }
  Serial.println(ESP.getFreeSketchSpace());
  Serial.println(ESP.getFlashChipRealSize());
  Serial.println(ESP.getFlashChipSize());
}


void loop() {

  server.handleClient();
  digitalWrite(LED_PIN, LOW);  // LED ON
  delay(5000);

  digitalWrite(LED_PIN, HIGH);  // LED OFF
  delay(1000);
}
