#include <HTTPClient.h>
#include <PubSubClient.h>
#include <WiFi.h>

// Time
unsigned long lastTime;

// WiFi
const char *ssid = "nodewifi";       // Enter your Wi-Fi name
const char *password = "node123456"; // Enter Wi-Fi password

// http server
String http_hostname = "192.168.2.251";
uint16_t http_port = 3000;

const char *publishTopic[] = {
    "buddy/publish/",
};
int publishNumber = 1;

const char *subscribeTopic[] = {"buddy/command/1", "buddy/command/2",
                                "buddy/command/3"};
int subscribeNumber = 3;

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {

    // Set software serial baud to 115200;
    Serial.begin(115200);

    // Connecting to a WiFi network
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("Connecting to WiFi..");
    }
    Serial.println("Connected to the Wi-Fi network");
    HTTPClient http;
    String payload;

    // regiter esp32
    String macAddress = WiFi.macAddress();
    Serial.println(macAddress);
    Serial.println("registering mac address");
    http.begin(http_hostname, 3000, "/registerId");
    payload = "{" + jsonFormat("macAddress", macAddress) + "}";
    unsigned int lenghtOfPayload = (unsigned int) sizeof(payload);
    char *packet;
    String sPacket = payload;
    sPacket.toCharArray(packet, sPacket.length()+1);
    while (true) {
        if (lastTime + 5000 < millis()) {
            lastTime = millis();
            http.addHeader("Content-Type", "application/json");
            int httpCode = http.POST(payload);
            if (httpCode > 0) {
                // file found at server
                if (httpCode == HTTP_CODE_OK) {
                    payload = http.getString();
                    Serial.println(payload);
                    Serial.println("registered mac address");
                    break;
                } else {
                    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
                }
            } else {
                Serial.println(httpCode);
                Serial.printf("[HTTP] GET... failed, error: %s\n",
                http.errorToString(httpCode).c_str());
            }
        }
    }
    http.end();

    // request mqtt id
    Serial.println("request mqtt identification");
    http.begin(http_hostname, 3000, "/requestId");
    while (true) {
        if (lastTime + 5000 < millis()) {
            lastTime = millis();
            int httpCode =
                http.POST("{}");
            if (httpCode > 0) {
                // file found at server
                if (httpCode == HTTP_CODE_OK) {
                    payload = http.getString();
                    Serial.println(payload);
                    Serial.println("recieve mqtt identification");
                    break;
                } else {
                    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
                }
            } else {
                Serial.println(httpCode);
                Serial.printf("[HTTP] GET... failed, error: %s\n",
                              http.errorToString(httpCode).c_str());
            }
        }
    }
    http.end();
    // add 1 include end of character
    Serial.println(jsonExtract(payload,"username"));
    String hostname = jsonExtract(payload,"mqtt_hostname");
    char mqtt_brokerArr[hostname.length()+1]; 
    hostname.toCharArray(mqtt_brokerArr, hostname.length()+1);
    char *mqtt_broker = mqtt_brokerArr;

    String username = jsonExtract(payload,"username");
    char mqtt_usernameArr[username.length()+1];
    username.toCharArray(mqtt_usernameArr, username.length()+1);
    char *mqtt_username = mqtt_usernameArr;

    String password = jsonExtract(payload,"password");
    char mqtt_passwordArr[password.length()+1];
    password.toCharArray(mqtt_passwordArr, password.length()+1);
    char *mqtt_password = mqtt_passwordArr;

    int mqtt_port = jsonExtract(payload,"mqtt_port").toInt();

    // connecting to a mqtt broker
    Serial.println("connecting mqtt server");
    client.setServer(mqtt_broker, mqtt_port);
    client.setCallback(callback);
    while (!client.connected()) {
        String client_id = "esp32-client-";
        client_id += String(WiFi.macAddress());
        Serial.printf("The client %s connects to the public MQTT broker\n",
                      client_id.c_str());
        if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
            Serial.println("Public EMQX MQTT broker connected");
        } else {
            Serial.println("failed with state ");
            Serial.println(client.state());
            delay(2000);
        }
    }

    // Publish and subscribe
    for (int i = 0; i < publishNumber; i++) {
        client.publish(publishTopic[i], "Hi, I'm ESP32 ^^");
    }
    for (int i = 0; i < subscribeNumber; i++) {
        client.subscribe(subscribeTopic[i]);
    }
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

String jsonFormat(String head, String data) {
    return "\"" + head + "\" : \"" + data + "\"";
}

String jsonExtract(String json, String head) {
    for (int i = 0; i < json.length(); i++) {
        int count = 0;
        for (int j = 0; j < head.length(); j++) {
            if (i + j == json.length()){
                return "-1";
            }
            if (json[i + j] == head[j]) {
                count++;
            } else {
                count = 0;
                break;
            }
        }
        if (count != 0) {
            String value = "";
            bool start1 = 0;
            bool start2 = 0;
            for (int k = i; k < json.length(); k++) {
                if (json[k] == ':') {
                    start1 = 1;
                } else if (start1 == 1 && json[k] == '\"') {
                    if(start2==1){
                        return value;
                    }else{
                        start2 = 1;
                    }
                } else if (start2 == 1) {
                    value += json[k];
                }
            }
        }
    }
    return "-1";
}

void loop() { client.loop(); }
