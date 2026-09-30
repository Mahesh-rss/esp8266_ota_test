#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Updater.h>

ESP8266WebServer server(80);

String ssid;
String password;

const char* version_url = "https://raw.githubusercontent.com/Mahesh-rss/esp8266_ota_test/master/version.txt";
const char* firmware_url = "https://raw.githubusercontent.com/Mahesh-rss/esp8266_ota_test/master/build/esp32.esp32.esp32/esp8266_ota_test.ino.bin";

const int CURRENT_VERSION = 1;

#define EEPROM_SIZE 128
#define SSID_ADDR 0
#define PASS_ADDR 64


void saveCredentials(String newSSID, String newPassword) {

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
}


void loadCredentials() {

  char ssidBuffer[64];
  char passwordBuffer[64];

  for (int i = 0; i < 64; i++) {
    ssidBuffer[i] = EEPROM.read(SSID_ADDR + i);
    passwordBuffer[i] = EEPROM.read(PASS_ADDR + i);
  }

  ssidBuffer[63] = '\0';
  passwordBuffer[63] = '\0';

  ssid = String(ssidBuffer);
  password = String(passwordBuffer);

  ssid.trim();
  password.trim();
}


bool connectToWiFi() {

  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    return true;

  } else {

    Serial.println("WiFi connection failed");

    return false;
  }
}


void startWiFiConfig() {

  WiFi.mode(WIFI_AP);

  WiFi.softAP("ESP8266-Setup", "12345678");

  Serial.println();
  Serial.println("WiFi configuration mode");
  Serial.println("Connect mobile to:");
  Serial.println("ESP8266-Setup");
  Serial.println("Password: 12345678");
  Serial.println("Open: 192.168.4.1");


  server.on("/", []() {
    String page = "<!DOCTYPE html>";
    page += "<html>";
    page += "<head>";
    page += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    page += "<style>";

    page += "body{";
    page += "font-family:Arial;";
    page += "background:#f4f4f9;";
    page += "text-align:center;";
    page += "padding-top:50px;";
    page += "}";

    page += ".box{";
    page += "background:white;";
    page += "padding:30px;";
    page += "margin:auto;";
    page += "max-width:400px;";
    page += "border-radius:10px;";
    page += "box-shadow:0 4px 8px rgba(0,0,0,0.15);";
    page += "}";

    page += "input{";
    page += "width:90%;";
    page += "padding:12px;";
    page += "margin:8px;";
    page += "border:1px solid #ccc;";
    page += "border-radius:5px;";
    page += "}";

    page += "input[type=submit]{";
    page += "background:#4CAF50;";
    page += "color:white;";
    page += "border:none;";
    page += "cursor:pointer;";
    page += "}";

    page += "</style>";
    page += "</head>";

    page += "<body>";

    page += "<div class='box'>";

    page += "<h2>WiFi Configuration</h2>";

    page += "<form method='POST' action='/save'>";

    page += "<input type='text' name='ssid' placeholder='WiFi SSID' required>";

    page += "<input type='password' name='password' placeholder='WiFi Password' required>";

    page += "<input type='submit' value='Save Settings'>";

    page += "</form>";

    page += "</div>";

    page += "</body>";
    page += "</html>";

    server.send(200, "text/html", page);
  });


  server.on("/save", HTTP_POST, []() {
    String newSSID = server.arg("ssid");
    String newPassword = server.arg("password");

    newSSID.trim();
    newPassword.trim();


    if (newSSID.length() == 0 || newPassword.length() == 0) {

      server.send(
        400,
        "text/html",
        "<html><body style='text-align:center;font-family:Arial;padding-top:60px;'>"
        "<h2 style='color:red;'>Error!</h2>"
        "<p>Please fill all fields.</p>"
        "</body></html>");

      return;
    }


    Serial.println();
    Serial.println("Testing WiFi credentials...");

    WiFi.mode(WIFI_STA);

    WiFi.begin(newSSID.c_str(), newPassword.c_str());

    int attempts = 0;

    while (WiFi.status() != WL_CONNECTED && attempts < 20) {

      delay(500);

      Serial.print(".");

      attempts++;
    }

    Serial.println();


    if (WiFi.status() != WL_CONNECTED) {

      Serial.println("WiFi connection failed");

      server.send(
        400,
        "text/html",
        "<html><body style='text-align:center;font-family:Arial;padding-top:60px;'>"
        "<h2 style='color:#e53935;'>WiFi Connection Failed!</h2>"
        "<p>Please check your SSID and password.</p>"
        "<br><a href='/'>Try Again</a>"
        "</body></html>");

      WiFi.mode(WIFI_AP);
      WiFi.softAP("ESP8266-Setup", "12345678");

      return;
    }


    Serial.println("WiFi connected");

    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());


    ssid = newSSID;
    password = newPassword;

    saveCredentials(ssid, password);


    server.send(
      200,
      "text/html",
      "<html><body style='text-align:center;font-family:Arial;padding-top:60px;'>"
      "<h2 style='color:#4CAF50;'>Success!</h2>"
      "<p>WiFi connected successfully.</p>"
      "<p>Settings saved.</p>"
      "<p>Device restarting...</p>"
      "</body></html>");


    delay(3000);

    ESP.restart();
  });


  server.begin();

  Serial.println("Configuration server started");
}


void checkForUpdate() {

  WiFiClient client;
  HTTPClient http;

  Serial.println();
  Serial.println("Checking OTA...");


  if (!http.begin(client, version_url)) {

    Serial.println("Version connection failed");

    return;
  }


  int httpCode = http.GET();

  Serial.print("Version HTTP code: ");
  Serial.println(httpCode);


  if (httpCode == HTTP_CODE_OK) {

    String serverVersion = http.getString();

    serverVersion.trim();

    int newVersion = serverVersion.toInt();


    Serial.print("Current version: ");
    Serial.println(CURRENT_VERSION);

    Serial.print("Server version: ");
    Serial.println(newVersion);


    http.end();


    if (newVersion > CURRENT_VERSION) {

      Serial.println("New firmware available");

      performOTA();

    } else {

      Serial.println("Already using the latest version");
    }

  } else {

    Serial.println("Failed to check version");

    http.end();
  }
}


void performOTA() {

  WiFiClient client;
  HTTPClient http;

  Serial.println();
  Serial.println("Starting OTA...");


  if (!http.begin(client, firmware_url)) {

    Serial.println("Firmware connection failed");

    return;
  }


  int httpCode = http.GET();


  if (httpCode == HTTP_CODE_OK) {

    int contentLength = http.getSize();

    Serial.print("Firmware size: ");
    Serial.println(contentLength);


    if (contentLength > 0) {

      if (Update.begin(contentLength)) {

        size_t written = Update.writeStream(http.getStream());

        Serial.print("Written: ");
        Serial.println(written);


        if (written == contentLength) {

          if (Update.end()) {

            if (Update.isFinished()) {

              Serial.println("OTA SUCCESS");

              Serial.println("Restarting...");

              delay(2000);

              ESP.restart();
            }

          } else {

            Serial.println("OTA update failed");
          }

        } else {

          Serial.println("Firmware write incomplete");
        }

      } else {

        Serial.println("Not enough space for OTA");
      }

    } else {

      Serial.println("Invalid firmware size");
    }

  } else {

    Serial.print("Firmware HTTP error: ");
    Serial.println(httpCode);
  }


  http.end();
}


void setup() {

  Serial.begin(115200);

  Serial.println();
  Serial.println("Starting...");


  EEPROM.begin(EEPROM_SIZE);

  loadCredentials();


  if (ssid.length() == 0 || password.length() == 0) {

    Serial.println("No WiFi credentials found");

    startWiFiConfig();

    return;
  }


  Serial.println("Saved WiFi credentials found");


  if (connectToWiFi()) {

    checkForUpdate();

  } else {

    Serial.println("Starting WiFi configuration...");

    startWiFiConfig();
  }
}


void loop() {

  server.handleClient();
}