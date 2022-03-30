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
#include <WiFiClient.h>

#if defined(ARDUINO_ARCH_ESP8266)
ESP8266WebServer server;
#elif defined(ARDUINO_ARCH_ESP32)
WebServer server;
#endif

#ifndef RELAY
#define RELAY 0 // backward compatibility
#endif

// Your Domain name with URL path or IP address with path
String ipUrl = "http://192.168.1.42:5000/ip";
// String ipUrl = "http://10.42.0.1:5000/ip";

AutoConnect portal(server);

void handleRoot()
{
  if (server.arg("v") == "low")
    digitalWrite(RELAY, LOW);
  else if (server.arg("v") == "high")
    digitalWrite(RELAY, HIGH);
  server.send(200, "text/plain"
                   "SUCCESS");
}

void sendRedirect(String uri)
{
  server.sendHeader("Location", uri, true);
  server.send(302, "text/plain", "");
  server.client().stop();
}

void handleGPIO()
{
  if (server.arg("v") == "low")
    digitalWrite(RELAY, LOW);
  else if (server.arg("v") == "high")
    digitalWrite(RELAY, HIGH);
  sendRedirect("/");
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
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, HIGH);

  // Put the home location of the web site.
  // But in usually, setting the home uri is not needed cause default location is "/".
  // portal.home("/");

  server.on("/", handleRoot);
  server.on("/io", handleGPIO);

  // Starts user web site included the AutoConnect portal.
  portal.onDetect(atDetect);
  if (portal.begin())
  {
    Serial.println("Started, IP:" + WiFi.localIP().toString());
    WiFiClient client;
    HTTPClient http;
    String serverPath = ipUrl + "?name=Humidity&IP=" + WiFi.localIP().toString();

    // Your Domain name with URL path or IP address with path
    http.begin(client, serverPath.c_str());

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
}