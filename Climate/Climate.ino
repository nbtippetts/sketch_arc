#include <Arduino.h>
#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#endif
#include <AutoConnect.h>

#if defined(ARDUINO_ARCH_ESP8266)
ESP8266WebServer server;
#elif defined(ARDUINO_ARCH_ESP32)
WebServer server;
#endif

#include <SensirionI2CScd4x.h>
#include <Wire.h>

// Your Domain name with URL path or IP address with path
String ipUrl = "http://192.168.1.42:5000/ip";
String climateUrl = "http://192.168.1.42:5000/climate";
String climateLogUrl = "http://192.168.1.42:5000/climate/log";
// String ipUrl = "http://10.42.0.1:5000/ip";
// String climateUrl = "http://10.42.0.1:5000/climate";
// String climateLogUrl = "http://10.42.0.1:5000/climate/log";

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
// unsigned long timerDelay = 600000;
// Set timer to 5 seconds (5000)
unsigned long timerDelay = 5000;
int logCount = 0;

SensirionI2CScd4x scd4x;
HTTPClient http;
WiFiClient client;

void printUint16Hex(uint16_t value)
{
  Serial.print(value < 4096 ? "0" : "");
  Serial.print(value < 256 ? "0" : "");
  Serial.print(value < 16 ? "0" : "");
  Serial.print(value, HEX);
}

void printSerialNumber(uint16_t serial0, uint16_t serial1, uint16_t serial2)
{
  Serial.print("Serial: 0x");
  printUint16Hex(serial0);
  printUint16Hex(serial1);
  printUint16Hex(serial2);
  Serial.println();
}
AutoConnect portal(server);

void handleRoot()
{
  String page = PSTR(
      "<html>"
      "</head>"
      "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
      "<style type=\"text/css\">"
      "body {"
      "-webkit-appearance:none;"
      "-moz-appearance:none;"
      "font-family:'Arial',sans-serif;"
      "text-align:center;"
      "}"
      ".menu > a:link {"
      "position: absolute;"
      "display: inline-block;"
      "right: 12px;"
      "padding: 0 6px;"
      "text-decoration: none;"
      "}"
      ".button {"
      "display:inline-block;"
      "border-radius:7px;"
      "background:#73ad21;"
      "margin:0 10px 0 10px;"
      "padding:10px 20px 10px 20px;"
      "text-decoration:none;"
      "color:#000000;"
      "}"
      "</style>"
      "</head>"
      "<body>"
      "<div class=\"menu\">" AUTOCONNECT_LINK(BAR_24) "</div>"
                                                      "BUILT-IN LED<br>"
                                                      "GPIO(");
  server.send(200, "text/html", page);
}

void sendRedirect(String uri)
{
  server.sendHeader("Location", uri, true);
  server.send(302, "text/plain", "");
  server.client().stop();
}

bool atDetect(IPAddress &softapIP)
{
  Serial.println("Captive portal started, SoftAP IP:" + softapIP.toString());
  return true;
}

void setup()
{
  delay(1000);
  Serial.begin(115200);
  Serial.println();
  WiFi.disconnect();
  // Put the home location of the web site.
  // But in usually, setting the home uri is not needed cause default location is "/".
  // portal.home("/");

  server.on("/", handleRoot);

  // Starts user web site included the AutoConnect portal.
  portal.onDetect(atDetect);
  if (portal.begin())
  {
    Serial.println("Started, IP:" + WiFi.localIP().toString());
    String ipEndPoint = ipUrl + "?name=Climate&IP=" + WiFi.localIP().toString();
    http.begin(client, ipEndPoint.c_str());
    // Send HTTP GET request
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0)
    {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      Serial.println(payload);
    }
    else
    {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    // Free resources
    http.end();
    Serial.println("Timer set to 5 seconds (timerDelay variable), it will take 5 seconds before publishing the first reading.");
    Serial.begin(115200);
    while (!Serial)
    {
      delay(100);
    }

    Wire.begin();

    uint16_t error;
    char errorMessage[256];

    scd4x.begin(Wire);

    // stop potentially previously started measurement
    error = scd4x.stopPeriodicMeasurement();
    if (error)
    {
      Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
    }

    uint16_t serial0;
    uint16_t serial1;
    uint16_t serial2;
    error = scd4x.getSerialNumber(serial0, serial1, serial2);
    if (error)
    {
      Serial.print("Error trying to execute getSerialNumber(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
    }
    else
    {
      printSerialNumber(serial0, serial1, serial2);
    }

    // Start Measurement
    error = scd4x.startPeriodicMeasurement();
    if (error)
    {
      Serial.print("Error trying to execute startPeriodicMeasurement(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
    }

    Serial.println("Waiting for first measurement... (5 sec)");
  }
  else
  {
    Serial.println("Connection failed.");
    while (true)
    {
      yield();
    }
  }
}

void loop()
{
  server.handleClient();
  portal.handleRequest(); // Need to handle AutoConnect menu.
  if (WiFi.status() == WL_IDLE_STATUS)
  {
#if defined(ARDUINO_ARCH_ESP8266)
    ESP.reset();
#elif defined(ARDUINO_ARCH_ESP32)
    ESP.restart();
#endif
    delay(1000);
  }
  uint16_t error;
  char errorMessage[256];

  delay(5000);

  // Read Measurement
  uint16_t co2;
  float temperature;
  float fahrenheit;
  float humidity;
  error = scd4x.readMeasurement(co2, temperature, humidity);
  if (error)
  {
    Serial.print("Error trying to execute readMeasurement(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }
  else if (co2 == 0)
  {
    Serial.println("Invalid sample detected, skipping.");
  }
  else
  {
    fahrenheit = (temperature * 9.0) / 5.0 + 32;
    String climateEndPoint = climateUrl + "?co2=" + co2 + "&humidity=" + humidity + "&temperature=" + fahrenheit;

    // Your Domain name with URL path or IP address with path
    http.begin(client, climateEndPoint.c_str());

    // Send HTTP GET request
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0)
    {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      Serial.println(payload);
    }
    else
    {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    // Free resources
    http.end();
    if (logCount >= 60)
    {
      String climateLogEndPoint = climateLogUrl + "?co2=" + co2 + "&humidity=" + humidity + "&temperature=" + fahrenheit;

      // Your Domain name with URL path or IP address with path
      http.begin(client, climateLogEndPoint.c_str());

      // Send HTTP GET request
      int httpResponseCode = http.GET();

      if (httpResponseCode > 0)
      {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
      }
      else
      {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      // Free resources
      http.end();
      logCount = 0;
    }
    else
    {
      logCount++;
    }
    Serial.print("Co2:");
    Serial.print(co2);
    Serial.print("\t");
    Serial.print("Temperature:");
    Serial.print(temperature);
    Serial.print("\t");
    Serial.print("Humidity:");
    Serial.println(humidity);
  }
}