#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HX711_ADC.h>
#include <PubSubClient.h>
#include <WiFi.h>

// WiFi
const char *ssid = "nodewifi";           // Enter your Wi-Fi name
const char *password = "30458kj;lw0ir["; // Enter Wi-Fi password
WiFiClient espClient;

// HTTP
StaticJsonDocument<200> registerDoc;
StaticJsonDocument<200> requestDoc;
StaticJsonDocument<200> responseDoc;

// MQTT Broker
PubSubClient client(espClient);
unsigned long delaySendTime;

// Weight sensor
unsigned long delayReadTime;
unsigned long stabilizingtime = 2000;
const int LOADCELL_DOUT_PIN = 21;
const int LOADCELL_SCK_PIN = 19;
float calibrationValue = 24.15;
bool _tare = true;
HX711_ADC scale(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

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
    http.begin(serverPath, 3000, "/registerId");
    int httpResponseCode = 0;
    while (httpResponseCode <= 0) {
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

    http.begin(serverPath, 3000, "/requestId");
    httpResponseCode = 0;
    while (httpResponseCode != 200) {
        http.addHeader("Content-Type", "application/json");
        registerDoc["macAddress"] = "123412341234";
        String requestBody;
        serializeJson(registerDoc, requestBody);

        Serial.print("request body : ");
        Serial.println(requestBody);

        httpResponseCode = http.POST(requestBody);
        Serial.print("http response code : ");
        Serial.println(httpResponseCode);
        if (httpResponseCode == 200) {
            String response = http.getString();
            DeserializationError error = deserializeJson(responseDoc, response);
        } else {
            delay(1000);
        }
    }

    // Connect to MQTT server
    delaySendTime = millis();
    const char *mqtt_hostname = responseDoc["mqtt_hostname"];
    const uint16_t mqtt_port = responseDoc["mqtt_port"];
    const char *mqtt_username = responseDoc["username"];
    const char *mqtt_password = responseDoc["password"];
    Serial.println(mqtt_port);

    client.setServer(mqtt_hostname, mqtt_port);
    client.setCallback(callback);
    while (!client.connected()) {
        String client_id = "esp32-client-";
        client_id += String(WiFi.macAddress());
        Serial.printf("The client %s connects to the public MQTT broker\n",
                      client_id.c_str());
        if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
            Serial.println("Public EMQX MQTT broker connected");
        } else {
            Serial.print("failed with state ");
            Serial.print(client.state());
            delay(2000);
        }
    }

    // Initial scale setting
    delayReadTime = millis();
    Serial.println("Setting scale");
    scale.begin();
    scale.start(stabilizingtime, _tare);
    scale.setCalFactor(calibrationValue);
    // subscribe Topic
    // client.subscribe(topic);
    scale.start(stabilizingtime, _tare);
    scale.setCalFactor(calibrationValue);
    
    Serial.println("Setting water level");
    pinMode(35,INPUT);
    pinMode(32,INPUT);
    pinMode(33,INPUT);
    
    Serial.println("Finish initialize setting");
}

void callback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);
    Serial.print("Message:");
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();
    Serial.println("-----------------------");
}

void publishWeight() {
    const char *weightTopic = "buddy/weightTopic/esp32/";
    char buffer[200];
    scale.update();
    float weight = scale.getData();
    sprintf(buffer, "weight : %f", weight);
    const char *payload = buffer;
    client.publish(weightTopic, payload);
}

void publishWaterTemp() {
    const char *waterTempTopic = "buddy/waterTempTopic/esp32/";
    client.publish(waterTempTopic, "waterTemp : 123");
}

void publishWaterLevel() {
    const char *waterLevelTopic = "buddy/waterLevelTopicc/esp32/";
    int level = -1;
    if(analogRead(35) < 3000){
        level = 3;
    } else if(analogRead(32) < 3000){
        level = 2;
    } else if(analogRead(33) < 3000){
        level = 1;
    } else {
        level = 0;
    }
    Serial.print("26 : ");
    Serial.println(analogRead(4));

    char buffer[200];
    sprintf(buffer, "water level : %d", level);
    const char *payload = buffer;
    client.publish(waterLevelTopic, payload);
}

void publishState() {
    const char *stateTopic = "buddy/stateTopic/esp32/";
    client.publish(stateTopic, "state : 123");
}

void loop() {
    client.loop();

    if (millis() > delaySendTime + 5000) {
        publishWeight();
        publishWaterTemp();
        publishWaterLevel();
        publishState();
        delaySendTime = millis();
    }

    if (millis() > delayReadTime + 100) {
        scale.update();
        scale.getData();
        delayReadTime = millis();
    }
}