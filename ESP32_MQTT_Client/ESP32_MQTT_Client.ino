#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "HX711.h"

// WiFi
const char *ssid = "nodewifi"; // Enter your Wi-Fi name
const char *password = "30458kj;lw0ir[";  // Enter Wi-Fi password
WiFiClient espClient;

// HTTP
StaticJsonDocument<200> registerDoc;
StaticJsonDocument<200> requestDoc;
StaticJsonDocument<200> responseDoc;

// MQTT Broker
PubSubClient client(espClient);

// Weight sensor
unsigned long ADCTime;
const int LOADCELL_DOUT_PIN = 21;
const int LOADCELL_SCK_PIN = 19;
HX711 scale;



void setup() {
    // Initial serial setting
    Serial.begin(115200);

    // Initial wifi setting
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("Connecting to WiFi..");
    }
    Serial.println("Connected to the Wi-Fi network");

    // Get MQTT credential from mqtt server
    HTTPClient http;
    String serverPath = "192.168.2.252";
    http.begin(serverPath,3000,"/registerId");
    int httpResponseCode = 0;
    while(httpResponseCode <= 0){
        http.addHeader("Content-Type", "application/json");
        requestDoc["macAddress"] = WiFi.macAddress(); 
        String requestBody;
        serializeJson(requestDoc, requestBody);

        Serial.print("register body : ");
        Serial.println(requestBody);

        httpResponseCode = http.POST(requestBody);
        Serial.print("http response code : ");
        Serial.println(httpResponseCode);
        delay(1000);
    }

    http.begin(serverPath,3000,"/requestId");
    httpResponseCode = 0;
    while(httpResponseCode != 200){
        http.addHeader("Content-Type", "application/json");
        registerDoc["macAddress"] = "123412341234"; 
        String requestBody;
        serializeJson(registerDoc, requestBody);

        Serial.print("request body : ");
        Serial.println(requestBody);

        httpResponseCode = http.POST(requestBody);
        Serial.print("http response code : ");
        Serial.println(httpResponseCode);
        if(httpResponseCode==200){
            String response = http.getString();
            DeserializationError error = deserializeJson(responseDoc, response);
            } else {
                delay(1000);
        }
    }

    // Connect to MQTT server
    const char* mqtt_hostname  = responseDoc["mqtt_hostname"];
    const uint16_t mqtt_port = responseDoc["mqtt_port"];
    const char* mqtt_username = responseDoc["username"]; 
    const char* mqtt_password = responseDoc["password"];
    Serial.println(mqtt_port);

    client.setServer(mqtt_hostname, mqtt_port);
    client.setCallback(callback);
    while (!client.connected()) {
        String client_id = "esp32-client-";
        client_id += String(WiFi.macAddress());
        Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
        if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
            Serial.println("Public EMQX MQTT broker connected");
        } else {
            Serial.print("failed with state ");
            Serial.print(client.state());
            delay(2000);
        }
    }

    // Initial scale setting
    Serial.println("Setting scale");
    ADCTime = millis();
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    scale.set_scale(2280.f);
    scale.tare();

    // subscribe Topic
    // client.subscribe(topic);

    Serial.println("Finish initialize setting");
}

void callback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);
    Serial.print("Message:");
    for (int i = 0; i < length; i++) {
        Serial.print((char) payload[i]);
    }
    Serial.println();
    Serial.println("-----------------------");
}

void publishWeight() {
    const char *weightTopic = "buddy/weightTopic/esp32/";
    client.publish(weightTopic, "weight : 123");
}
void publishWaterTemp() {
    const char *waterTempTopic = "buddy/waterTempTopic/esp32/";
    client.publish(waterTempTopic, "waterTemp : 123");
}
void publishWaterLevel() {
    const char *waterLevelTopic = "buddy/waterLevelTopicc/esp32/";
    client.publish(waterLevelTopic, "waterLevel : 123");
}
void publishState() {
    const char *stateTopic = "buddy/stateTopic/esp32/";
    client.publish(stateTopic, "state : 123");
}



void loop() {
    client.loop();

    publishWeight();
    publishWaterTemp();
    publishWaterLevel();
    publishState();

    scale.power_down();
    if(millis() > ADCTime + 1000){
        scale.power_up();
    }
}