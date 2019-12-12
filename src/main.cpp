#include <Arduino.h>

#include "WiFi.h"
#include <HTTPClient.h>

#include "html.h"
#include "creds.h"

WiFiServer server(80);

String header;

// Assign output variables to GPIO pins
const int output26 = 26;
const int output27 = 27;

void setup() {
    Serial.begin(115200);
    WiFi.begin(SSID, PSK);


    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    server.begin();
}
void loop() {
    WiFiClient client = server.available();

    if (client) {
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        header += c;
        if (c == '\n') {
          if (currentLine.length() == 0) {
            client.println(HTTP_HEADER);
            if (header.indexOf("GET /req") >= 0) {
                HTTPClient http;
                http.begin(IFTTT);
                http.addHeader("Content-Type", "application/json");
                int httpResponseCode = http.POST("{\"value1\":\"10\"}");

                if (httpResponseCode>0) {
                    String response = http.getString();
                    Serial.println(httpResponseCode);
                    Serial.println(response);
                } else {
                    Serial.print("Error on sending POST: ");
                    Serial.println(httpResponseCode);
                }

                http.end();
            }

            client.println(HTML_HEADER);
            client.println(CSS);
            client.println(BODY);
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    header = "";
    client.stop();
  }
}
