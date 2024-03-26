#include <ArduinoJson.h>
#include <DallasTemperature.h>
#include <HTTPClient.h>
#include <HX711_ADC.h>
#include <OneWire.h>
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
char buffer[60];

#define INIT_STATE 0
#define CHECK_WATER_LEVEL 1
#define TARE_WEIGHT 2
#define ADD_MANGO 3
#define BOILING_MANGO 4
#define WAITED_REMOVE_MANGO 5
#define ERROR_WATER_LEVEL_LOW 6
#define ERROR_WATER_LEVEL_HIGH 7
#define ERROR_TEMPERATURE_LOW 8
#define ERROR_TEMPERATURE_HIGH 9
#define ERROR_WEIGHT_LOSS 10
int state = INIT_STATE;

#define BOIL_MANGO 1
#define REMOVE_MANGO 2

// Weight sensor
unsigned long delayReadTime;
unsigned long delayReadDuringBoilTime;

unsigned long stabilizingtime = 2000;
const int LOADCELL_DOUT_PIN = 21;
const int LOADCELL_SCK_PIN = 19;
float calibrationValue = 24.15;
bool _tare = true;
HX711_ADC scale(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
#define TOO_LOSS -500.0
int weightCounter;

// Temperature sensor
const int oneWireBus = 4;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);
float temperatureC;
#define TOO_HOT 55.0
#define TOO_COLD 30.0
int tempCounter;

// water leveling sensor
int levelCounter;

// Heater
unsigned long delayBoilTime;
bool isBoiling = 0;

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

    http.begin(serverPath, 3000, "/requestId");
    int httpResponseCode = 0;
    while (httpResponseCode != 200) {
        http.addHeader("Content-Type", "application/json");

        requestDoc["MAC_ADDRESS"] = WiFi.macAddress();
        String requestBody;
        serializeJson(requestDoc, requestBody);

        Serial.print("register body : ");
        Serial.println(requestBody);

        httpResponseCode = http.POST(requestBody);
        Serial.print("http response code : ");
        Serial.println(httpResponseCode);
        if (httpResponseCode == 200) {
            String response = http.getString();
            DeserializationError error = deserializeJson(responseDoc, response);
        } else {
            delay(30000);
        }
    }

    // Connect to MQTT server
    delaySendTime = millis();
    const char *node = responseDoc["node"];
    const char *mqtt_hostname = responseDoc["mqtt_hostname"];
    const uint16_t mqtt_port = responseDoc["mqtt_port"];
    const char *mqtt_username = responseDoc["username"];
    const char *mqtt_password = responseDoc["password"];

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
    state = INIT_STATE;
    publishState();

    // subscribe Topic
    Serial.println("Subscribe topic");
    cleanBuffer();
    sprintf(buffer, "%s/commandPrompt/", node);
    const char *commandTopic = buffer;
    client.subscribe(commandTopic);

    // Initial scale setting
    delayReadTime = millis();
    delayReadDuringBoilTime = millis();
    Serial.println("Setting scale");
    scale.begin();
    scale.start(stabilizingtime, _tare);
    scale.setCalFactor(calibrationValue);

    scale.start(stabilizingtime, _tare);
    scale.setCalFactor(calibrationValue);

    Serial.println("Setting water level");
    pinMode(35, INPUT);
    pinMode(32, INPUT);
    pinMode(33, INPUT);

    // initialize temperature setting
    sensors.begin();

    // init RERAY
    pinMode(5, OUTPUT);

    Serial.println("Finish initialize setting");

    preparation();
}

void callback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);
    Serial.print("Message:");
    String msg = "";
    for (int i = 0; i < length; i++) {
        msg.concat((char)payload[i]);
    }
    Serial.println(msg);
    Serial.println("-----------------------");

    const char *node = responseDoc["node"];

    if (state == ADD_MANGO) {
        cleanBuffer();
        sprintf(buffer, "%s/commandPromt/", node);
        if (strcmp(topic, buffer) && msg.equals("{\"command\" : 1}")) {
            Serial.println("Start Boiling Mango");
            state = BOILING_MANGO;
            isBoiling = 0;
            publishState();
            delayBoilTime = millis() - 595000;
        }
    } else if (state == WAITED_REMOVE_MANGO || state == ERROR_TEMPERATURE_LOW ||
               state == ERROR_TEMPERATURE_HIGH || ERROR_WEIGHT_LOSS ||
               ERROR_WATER_LEVEL_LOW || ERROR_WATER_LEVEL_HIGH) {
        if (strcmp(topic, buffer) && msg.equals("{command : 2}")) {
            Serial.println("Remove Mango");
            preparation();
        }
    }
}

void publishWeight() {
    const char *node = responseDoc["node"];
    cleanBuffer();
    sprintf(buffer, "weightTopic/%s/", node);
    const char *weightTopic = buffer;

    char payloadBuffer[60];
    scale.update();
    float weight = scale.getData();
    sprintf(payloadBuffer, "{\"weight\" : %f ,\n \"node\" : \"%s\"}", weight,
            node);
    const char *payload = payloadBuffer;
    client.publish(weightTopic, payload);
}

void publishWaterTemp() {
    const char *node = responseDoc["node"];
    cleanBuffer();
    sprintf(buffer, "waterTempTopic/%s/", node);
    const char *waterTempTopic = buffer;

    char payloadBuffer[60];
    sensors.requestTemperatures();
    temperatureC = sensors.getTempCByIndex(0);
    sprintf(payloadBuffer, "{\"waterTemp\" : %f ,\n \"node\" : \"%s\"}",
            temperatureC, node);
    const char *payload = payloadBuffer;

    client.publish(waterTempTopic, payloadBuffer);
}

int checkWaterLevel() {
    int level = -1;
    if (analogRead(35) < 3000) {
        level = 3;
    } else if (analogRead(32) < 3000) {
        level = 2;
    } else if (analogRead(33) < 3000) {
        level = 1;
    } else {
        level = 0;
    }
    return level;
}

void publishWaterLevel() {
    const char *node = responseDoc["node"];
    cleanBuffer();
    sprintf(buffer, "waterLevelTopic/%s/", node);
    const char *waterLevelTopic = buffer;

    char payloadBuffer[60];
    int level = checkWaterLevel();
    sprintf(payloadBuffer, "{\"waterLevel\" : %d,\n \"node\" : \"%s\"}", level,
            node);
    const char *payload = payloadBuffer;
    client.publish(waterLevelTopic, payload);
}

void publishState() {
    const char *node = responseDoc["node"];
    cleanBuffer();
    sprintf(buffer, "stateTopic/%s/", node);
    const char *stateTopic = buffer;

    char payloadBuffer[60];
    sprintf(payloadBuffer, "{\"state\" : %d,\n \"node\" : \"%s\"}", state,
            node);
    const char *payload = payloadBuffer;
    client.publish(stateTopic, payload);
}

void cleanBuffer() {
    for (int i = 0; i < 60; i++) {
        buffer[i] = '\0';
    }
}

void preparation() {
    const char *node = responseDoc["node"];
    cleanBuffer();
    sprintf(buffer, "commandPromt/%s/", node);
    // Checking water level
    Serial.println("Checking Water level");
    state = CHECK_WATER_LEVEL;
    int waterLevel = -1;
    int oldWaterLevel = -1;
    while (true) {
        if (millis() > delayReadTime + 5000) {
            publishWaterLevel();
            waterLevel = checkWaterLevel();
            Serial.println(waterLevel);
            if (waterLevel != oldWaterLevel) {
                if (waterLevel == 2) {
                    state = TARE_WEIGHT;
                    publishState();
                    break;
                } else if (waterLevel < 2) {
                    state = ERROR_WATER_LEVEL_LOW;
                    publishState();
                } else {
                    state = ERROR_WATER_LEVEL_HIGH;
                    publishState();
                }
            }
            oldWaterLevel = waterLevel;
            delayReadTime = millis();
        }
    }

    // tare weight of water off
    Serial.println("Tare weight");
    scale.tare();
    Serial.println("Ready to add Mango");
    state = ADD_MANGO;
    publishState();
}

void loop() {
    client.loop();
    const char *node = responseDoc["node"];
    cleanBuffer();
    sprintf(buffer, "stateTopic/%s/", node);
    const char *stateTopic = buffer;

    if (state == BOILING_MANGO) {
        if (millis() > delayBoilTime + 600000) {
            if (!isBoiling) {
                // turn on boiling code
                Serial.println("TURN ON HEATER");
                isBoiling = 1;
                digitalWrite(5, HIGH);
                tempCounter = 0;
                weightCounter = 0;
                levelCounter = 0;
            } else {
                // turn off boiling code
                Serial.println("TURN OFF HEATER");
                isBoiling = 0;
                state = WAITED_REMOVE_MANGO;
                publishState();
                digitalWrite(5, LOW);
            }
            delayBoilTime = millis();
        }

        // if temp too low or hot for 2min turn off
    }

    if (millis() > delaySendTime + 5000) {
        publishWeight();
        publishWaterTemp();
        publishWaterLevel();
        delaySendTime = millis();
    }

    if (millis() > delayReadTime + 100) {
        scale.update();
        scale.getData();
        delayReadTime = millis();
    }

    if (millis() > delayReadDuringBoilTime + 500) {
        if (state == BOILING_MANGO) {
            sensors.requestTemperatures();
            temperatureC = sensors.getTempCByIndex(0);
            if (temperatureC > TOO_HOT) {
                if (tempCounter < 0) {
                    tempCounter = 0;
                }
                Serial.println("too hot");
                tempCounter++;
                if (tempCounter > 60) {
                    state = ERROR_TEMPERATURE_HIGH;
                    publishState();
                    digitalWrite(5, LOW);
                }
                // } else if (temperatureC < TOO_COLD) {
                //     Serial.println("too cold");
                //     tempCounter--;
                //     if (tempCounter < -240) {
                //         state = ERROR_TEMPERATURE_LOW;
                //         publishState();
                //         digitalWrite(5, LOW);
                //     }
                // }
                // scale.update();
                // if (scale.getData() < TOO_LOSS) {
                //     weightCounter++;
                //     if (weightCounter == 10) {
                //         state = ERROR_WEIGHT_LOSS;
                //         publishState();
                //         digitalWrite(5, LOW);
                //     }
                // }

                if (checkWaterLevel() > 2) {
                    levelCounter--;
                    if (levelCounter == -10) {
                        state = ERROR_WATER_LEVEL_LOW;
                        publishState();
                        digitalWrite(5, LOW);
                    }
                } else if (checkWaterLevel() == 3) {
                    levelCounter++;
                    if (levelCounter == 10) {
                        state = ERROR_WATER_LEVEL_HIGH;
                        publishState();
                        digitalWrite(5, LOW);
                    }
                }
            }
            delayReadDuringBoilTime = millis();
        }
    }
}