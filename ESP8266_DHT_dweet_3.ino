#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <Ticker.h>

#include "DHT.h"      //Adafruit DHT-sensor-library
#define DHTPIN 5      // GPIO05 == D1
#define DHTTYPE DHT22 // DHT11, DHT21, DHT22
DHT dht(DHTPIN, DHTTYPE,
        15); // ESP8266 initialize DHT as follows: DHT dht(DHTPIN, DHTTYPE, 15);

// WiFi parameters
// const char* ssid = "........";        //WiFi.SSID()
// const char* pass = "........";        //WiFi.psk()
unsigned long ulReconncount = 0; // how often did we connect to WiFi

Ticker ticker;
// flag changed in the ticker function every 1 minutes
bool readyForUpdate = true;

WiFiClient client; // must be global

String Payload; // must be global

#define NUMBER_OF_STRINGS 18
char *meteo[NUMBER_OF_STRINGS];

void setReadyForUpdate() {
  Serial.printf("Setting readyForUpdate %i\n", millis());
  readyForUpdate = true;
}

void setup() {
  Serial.begin(115200);
  WiFiStart();
  dht.begin();

  // flag changed in the ticker function every 1 minutes
  ticker.attach(1 * 60, setReadyForUpdate);
}

void loop() {
  if (readyForUpdate) {
    readyForUpdate = false;
    update();
  }
}

void update() {
  float h = dht.readHumidity();    // Luftfeuchte auslesen
  float t = dht.readTemperature(); // Temperatur auslesen

  if (isnan(t) || isnan(h)) {
    Serial.println("DHT22 konnte nicht ausgelesen werden");
    return;
  }

  // String humm = String(h);
  // String temp = String(t);
  char str_humm[6];
  dtostrf(h, 3, 2, str_humm);
  char str_temp[5];
  dtostrf(t, 2, 2, str_temp);

  /*
  String url = "/dweet/for/kuchyn?temperature=";
  url += temp;
  url += "&humidity=" + humm;
  */

  static char url[64];

  sprintf(url, "/dweet/for/kuchyn?temperature=%s&humidity=%s", str_temp,
          str_humm);
  Serial.printf("%s\n", url);
  httpGET("dweet.io", 80, url);
  handleResponse();
  client.stop(); // Serial.printf("\nClosing connection to %s\n", host);

  delay(10);

  sprintf(url, "/meteo/lastmeteodata");
  httpGET("www.hvezdarna.cz", 80, url);
  handleResponse();
  client.stop();
  split();

  sprintf(url, "/dweet/for/kravak?temperature=%s&humidity=%s", meteo[4],
          meteo[7]);
  httpGET("dweet.io", 80, url);
  handleResponse();
  client.stop(); // Serial.printf("\nClosing connection to %s\n", host);
}

int handleResponse() {
  // Read all the lines of the reply from server and print them to Serial
  int i = 0;
  int size = -1;
  int csize = -1;
  int returnCode = -1;

  long localEpoc = 0;

  while ((size = client.available()) > 0) {
    String line = client.readStringUntil('\n');
    line.trim(); // remove \r
    i++;

    if (line.startsWith("HTTP/1.")) {
      returnCode = line.substring(9, line.indexOf(' ', 9)).toInt();
    } else if (line.indexOf(':')) {
      String headerName = line.substring(0, line.indexOf(':'));
      String headerValue = line.substring(line.indexOf(':') + 2);

      // Serial.printf("[header %2i] %s: %s\n", i, headerName.c_str(),
      // headerValue.c_str());

      if (headerName.equalsIgnoreCase("Content-Length")) {
        csize = headerValue.toInt();
      }

      if (headerName.equalsIgnoreCase("Date")) {
        // String parsedDate = line.substring(6,22); //Fri, 01 Jan 2016
        int parsedYear = line.substring(18, 22).toInt();
        int parsedDay = line.substring(11, 13).toInt();
        String parsedMonth = line.substring(14, 17);
        int parsedHours = line.substring(23, 25).toInt();
        int parsedMinutes = line.substring(26, 28).toInt();
        int parsedSeconds = line.substring(29, 31).toInt();
        localEpoc =
            (parsedHours * 60 * 60 + parsedMinutes * 60 + parsedSeconds);

        Serial.printf(
            "Parsed time: %02i:%02i:%02i %02i %s %i\n localEpoc: %i\n",
            parsedHours + 1, parsedMinutes, parsedSeconds, parsedDay,
            parsedMonth.c_str(), parsedYear, localEpoc);
      }
    }

    if (line == "") { // end of header
      Serial.printf(
          "[header end] return code = %i, numlines = %i, Content-Length = %i\n",
          returnCode, i, csize);
      Payload = client.readStringUntil('\n');
      Payload.trim();
    }
  } // while
  Serial.printf("payload: %s\n", Payload.c_str());

  return returnCode;

} // handleResponse()

void split() {
  char separator[] = " ";
  char *payload = strdup(Payload.c_str());
  int i = 0;

  meteo[i] = strtok(payload, separator);
  while (meteo[i] != NULL) {
    meteo[++i] = strtok(NULL, separator);
  }

  free(payload);

  Serial.printf("cas: %s venkovni teplota: %s°C venkovni vlhkost: %s%\n",
                meteo[2], meteo[4], meteo[7]);
} // split()

void httpGET(
    const char *server, int port,
    const char *url) { // this method makes a HTTP connection to the server:
  // WiFiClient client; // must be global
  Serial.printf(
      "\n---------------- Connecting to %s ---------------------------\n",
      server);

  if (!client.connect(server, port)) {
    Serial.println("connection failed");
    return;
  }

  // This will send the request to the server with minimum http protocol header
  // String req = "GET " + url + " HTTP/1.1\r\nHost: " + server +
  // "\r\nConnection: close\r\n\r\n";
  static char req[116];
  sprintf(req, "GET %s HTTP/1.1\r\nHost: %s \r\nConnection: close\r\n\r\n", url,
          server);
  Serial.printf("Sending HTTP req %s\n", req);
  client.print(req);
  /*client.print("GET " + url + " HTTP/1.1\r\n" +
      "Host: " + server + "\r\n" +
      "Connection: close\r\n\r\n");
  */
  delay(10);
} // httpGET

void WiFiStart() {
  ulReconncount++;

  // We start by connecting to a WiFi network
  Serial.printf("\n\nConnecting to %s\n", WiFi.SSID().c_str());

  // WiFi.begin(ssid, pass);
  WiFi.begin();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.printf("\nWiFi connected\nIP address: %s\n",
                WiFi.localIP().toString().c_str());
  Serial.printf("Channel: %i\n", WiFi.channel());
}

// t is time in seconds = millis()/1000;
char *TimeToString(unsigned long t) {
  static char str[12];
  long h = t / 3600;
  t = t % 3600;
  int m = t / 60;
  int s = t % 60;
  // sprintf(str, "%04ld:%02d:%02d", h, m, s);
  sprintf(str, "%02d:%02d:%02d", h, m, s);
  return str;
}
