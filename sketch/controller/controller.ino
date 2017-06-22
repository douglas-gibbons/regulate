#include <MQTTClient.h>
#include <WiFi101.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include <WiFi101.h>
#include <ArduinoJson.h>

char ssid[] = "TODO_SSID";      //  your network SSID (name)
char pass[] = "TODO_WIFI_PASSWORD";   // your network password

char regkey[] = "TODO_REGISTRATION_PW";
char tregHost[] = "TODO_TSTACK_DOMAIN_NAME";

const int sensorPin = A0;
const int controlPin =  9;

const unsigned long HTTP_TIMEOUT = 10000;  // max respone time from server
// The type of data that we want to extract from the page
int status = WL_IDLE_STATUS;

struct RegData {
  char username[128];
  char password[128];
  char broker[256];
};

// WiFiClient client;
WiFiSSLClient client;
MQTTClient mqtt;
RegData regData;

void setup() {
  Serial.begin(9600);      // initialize serial communication
  // attempt to connect to Wifi network:
  Serial.println("Connecting to WiFi");
  while ( status != WL_CONNECTED) {
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    // wait for connection:
    delay(1 * 1000);
  }
  printWifiStatus();
  reg(&regData);
  Serial.println("Registered");
  mqttConnect();
  controlSubscribe();
}

void loop() {
  mqtt.loop(); // Required for topic subscriptions
  String topic = String(regData.username) + "/temperature";
  float temperature = readTemperature(sensorPin);
  mqtt.publish(topic, String(temperature));
  delay(1000);
}

/* 
 * Subscribe to USERNAME/control topic 
 */
void controlSubscribe() {
  char  topic[128];
  strcpy(topic, regData.username);
  strcat(topic,"/control");
  Serial.print("Subscribing to ");
  Serial.print(topic);
  Serial.println(".");
  mqtt.subscribe(topic);
  Serial.println("Subscribed");
}

/* 
 * Process MQTT subscriptions.
 * 
 * Only one subscription is needed, so no need to read the topic, just use the payload 
 */
void messageReceived(String topic, String payload, char * bytes, unsigned int length) {
  Serial.print("incoming: ");
  Serial.print(topic);
  Serial.print(" - ");
  Serial.print(payload);
  Serial.println();
  int state = payload.toInt();
  digitalWrite(controlPin, state);
}


/*
 * Connnect to message broker
 */
void mqttConnect() {
  char mqttHostname[256];
  hostFromUrl(&regData.broker[0], &mqttHostname[0]);
  mqtt.begin(mqttHostname, 8883, client);
  Serial.print("Connecting to MQTT host ");
  Serial.println(mqttHostname);
  while (!mqtt.connect("arduino", regData.username, regData.password)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nConnected");
}

/*
 * Skip HTTP headers so that we are at the beginning of the response body
 */
bool skipResponseHeaders() {
  // HTTP headers end with an empty line
  // char endOfHeaders[] = "\r\n\r\n";
  char endOfHeaders[] = "\r\n\r\n";
  client.setTimeout(HTTP_TIMEOUT);
  bool ok = client.find(endOfHeaders);
  if (!ok) {
    Serial.println("No response or invalid response!");
  }
  return ok;
}

/*
 * Register with treg service and fill registration data struct
 */
bool reg(struct RegData * r) {
  bool success;
  
  String PostData = "{\"RegistrationKey\": \"";
  PostData = PostData + regkey;
  PostData = PostData + "\",\"DeviceType\": \"Test\"}";

  Serial.println("\nStarting connection to treg server...");
  if (client.connect(tregHost, 443)) {
    Serial.println("Connected to server");
    // Make a HTTP request:
    client.println("POST /register.json HTTP/1.0");
    client.print("Host: ");
    client.println(tregHost);
    client.println("Content-Type: application/json; charset=utf-8");
    client.print("Content-Length: ");
    client.println(PostData.length());
    client.println();
    client.println(PostData);
    Serial.println("Request sent");
  } else {
    Serial.println("Connection failed");
  }
  skipResponseHeaders();
  
  if (readReponseContent(r)) {
    Serial.println("Response read");
    printRegData(r);
    success = true;
  } else {
    Serial.println("Error reading resonse data");
    success = false;
  }
  client.stop();
  return success;
}

/*
 * Reads JSON response from treg into RegData struct
 */
bool readReponseContent(struct RegData* regData) {
  StaticJsonBuffer<10000> jsonBuffer;
  char c;
  int i = 0;
  char response[5000];
  Serial.println("--------------------");
  client.setTimeout(1000);
  while (client.connected()) {
    if (client.available() == 0) {
      delay(500);
    }
    if (client.available() == 0) {
      client.stop();
    } else {
      char c = client.read();
      response[i] = c;
      i++;
    }
  }
  response[i] = '\0'; // Terminate the array
  Serial.println(response);
  Serial.println("--------------------");
  JsonObject& root = jsonBuffer.parseObject(response);
  if (!root.success()) {
    Serial.println("JSON parsing failed!");
    return false;
  }
  // Here were copy the strings we're interested in
  strcpy(regData->username, root["Name"]);
  strcpy(regData->password, root["Password"]);
  strcpy(regData->broker, root["Broker"]);

  
  
  return true;
}

// Print data extracted from the JSON
void printRegData(const struct RegData* regData) {
  Serial.print("Name = ");
  Serial.println(regData->username);
}


/*
 * Prints out WiFi status
 */
void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  Serial.println(ip);
}


/*
 * Outputs the temperature reading for a TMP36 sensor connected
 * to the given ADC pin
  */
float readTemperature(int pin) {
  int sensorValue = analogRead(sensorPin);
  // Voltage in mV
  float voltage = sensorValue * 3.3 * 1000 / 1023;
  // Conversion from voltage to temperature for TMP36 sensor
  float temperature = (voltage - 500) / 10;
  return temperature;
}


/*
 * Fills hostname with the hostname found in given URL
 */
void hostFromUrl(char * url, char * hostname) {
  Serial.print("Broker URL: ");
  Serial.println(url);
  Serial.println(strlen(url));
  int startof = 0;
  int endof = 0;
  for (int i = 0; i < strlen(url); i++) {
    if (startof == 0) {
      if (i > 0 && url[i-1] == '/' && url[i] == '/') {
        startof = i+1;
      }
    } else if (endof == 0 && ( url[i] == '/' || url[i] == ':')) {
      endof = i-1;
    }
  }
  // if endof was never found
  if (endof == 0) {
    endof = strlen(url) -1;
  }
  int alength = endof - startof + 1;
  // char hostname[alength];
  
  for (int i = 0; i < alength; i++) {
    hostname[i] = url[startof + i];
  }
  hostname[alength] = '\0';
}
