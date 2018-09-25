

/*
   Copyright (c) 2015, Majenko Technologies
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

 * * Redistributions of source code must retain the above copyright notice, this
     list of conditions and the following disclaimer.

 * * Redistributions in binary form must reproduce the above copyright notice, this
     list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.

 * * Neither the name of Majenko Technologies nor the names of its
     contributors may be used to endorse or promote products derived from
     this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
   ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>



#include <EasyNTPClient.h>
#include <WiFiUdp.h>
WiFiUDP udp;

EasyNTPClient ntpClient(udp, "pool.ntp.org", ((1 * 60 * 60))); //  GMT + 1


#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

const char *ssid = "MOVISTAR_E696";
const char *password = "68465AADE044B1F7937A";

ESP8266WebServer server(80);


#include "DHTesp.h"

#ifdef ESP32
#pragma message(THIS EXAMPLE IS FOR ESP8266 ONLY!)
#error Select ESP8266 board.
#endif

DHTesp dht;

float humidity;
float temperature;


const int led = 13;

int currentSlot = 0;
#define NBPOINTS 288 //one day every 5 minutes
#define STOREDELAY 10
int temps[NBPOINTS];
byte humidities[NBPOINTS];
unsigned long epoch;


#include <Ticker.h>
Ticker storer;
unsigned long firstRead = ntpClient.getUnixTime();
void storeData()  {

  Serial.printf("Storing at position %d/%d -> t=%f h=%f\n" , currentSlot % NBPOINTS, NBPOINTS, temperature, humidity );
  temps[currentSlot % NBPOINTS] = temperature;
  humidities[currentSlot % NBPOINTS] = humidity;
  currentSlot++;
}


char temp[7000];
void handleRoot() {
  digitalWrite(led, 1);

  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  Serial.println("Start handleRoot");
  Serial.println(millis());


  snprintf(temp, 7000,

           "<html>\
    <head>\
    <meta http-equiv='refresh' content='180'/>\
    <title>ESP8266 Demo</title>\n\
    <script type=\"text/javascript\" src=\"https://canvasjs.com/assets/script/canvasjs.min.js\"></script>\n\
    <script type=\"text/javascript\">\n\
    window.onload = function () {\n\
    var chart = new CanvasJS.Chart(\"chartContainer\",  {\n\
      zoomEnabled: true,\n\
       title:{\n\
       text: \"Temp and Humidity\"\n\
     },\n\
     data: [\n\
     {\n\
      type: \"area\",\n\
      xValueType: \"dateTime\",\n\
      dataPoints: [\n    %s    ]\n\
      }\n ]\n  }\n);\
    chart.render();\n\
  }\n\
  </script>\n\n\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\n\
  <body>\n\
    <h1>Hello from ESP8266!</h1>\n\
    <p>Uptime: %02d:%02d:%02d  - Time: %s</p>\n\
    <p>Temp: %f</p>\n\
    <p>Humidity: %f</p>\n\
    <div id=\"chartContainer\" style=\"height: 300px; width: 100%%;\"></div>\n\
  </body>\n\
</html>",
           buildData(),
           hr, min % 60, sec % 60, getDateString(),
           temperature, humidity
          );
  server.send(200, "text/html", temp);
  digitalWrite(led, 0);

  Serial.println("End handleRoot");
  Serial.println(millis());
}

void handleNotFound() {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

void setup(void) {
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");


  Serial.println();
  Serial.println("Status\tHumidity (%)\tTemperature (C)\t(F)\tHeatIndex (C)\t(F)");
  String thisBoard = ARDUINO_BOARD;
  Serial.println(thisBoard);

  // Autodetect is not working reliable, don't use the following line
  // dht.setup(17);
  // use this instead:
  dht.setup(14, DHTesp::DHT11); // Connect DHT sensor to GPIO 17

  storer.attach(STOREDELAY, storeData);

}

void loop(void) {
  delay(dht.getMinimumSamplingPeriod());

  humidity = dht.getHumidity();
  temperature = dht.getTemperature();

  server.handleClient();
}

char buf[20 * NBPOINTS];
char * buildData() {
  Serial.printf("start buildData\n");
  Serial.println(millis());

  char *pos = buf;
  buf[0] = 0;
  for (int i = 0; i < NBPOINTS; i++) {
    if (i) {
      pos += sprintf(pos, ",\n");
    }
    pos += sprintf(pos, "{x: %d,y: %d}", epoch - i * 1000 * STOREDELAY, temps[(currentSlot+i)%NBPOINTS]);
  }
  Serial.printf("end buildData, size : %d/%d\n", strlen(buf), 20 * NBPOINTS);
  Serial.println(millis());
  return buf;
}

char date[12];
char * getDateString() {
  Serial.println("Start getDateString");
  epoch = ntpClient.getUnixTime();

  snprintf(date, 12, "%02dh%02dm%02ds",
           (epoch  % 86400L) / 3600,
           (epoch  % 3600) / 60,
           epoch % 60);

  return date;
}
