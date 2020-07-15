#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <TaskScheduler.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>


/*
 * PIN MAPPING
 */

static const uint8_t D0   = 16;
static const uint8_t D1   = 5;
static const uint8_t D2   = 4;
static const uint8_t D3   = 0;
static const uint8_t D4   = 2;
static const uint8_t D5   = 14;
static const uint8_t D6   = 12;
static const uint8_t D7   = 13;
static const uint8_t D8   = 15;
static const uint8_t D9   = 3;
static const uint8_t D10  = 1;



/*
 * WIFI/MQTT settings
 */

const char* ssid = "DownStair2.4";
const char* password = "MyEsp8266_&19!";
const char* mqtt_server = "mqtt.atrent.it";

WiFiClient espClient;



/*
 * MQTT settings 
 */

const char* connectionTopic = "valerio/connected";

const char* roomTemperatureTopic = "valerio/room/temperature";
const char* roomHumidityTopic = "valerio/room/humidity";
const char* roomHeatIndexTopic = "valerio/room/heatIndex";

const char* outTemperatureTopic = "valerio/out/temperature";
const char* outHumidityTopic = "valerio/out/humidity";
const char* outHeatIndexTopic = "valerio/out/heatIndex";

PubSubClient client(espClient);



/*
 * IR settings
 */


const int RECV_IR_PIN = D6;
const int SEND_IR_PIN = D5;    

IRrecv irrecv(RECV_IR_PIN);
decode_results results;

IRsend irsend(SEND_IR_PIN);  // Set the GPIO to be used to sending the message.

int CODES[8] = {
  0x8808440,
  0x8808541,
  0x8808642,
  0x8808743,
  0x8808844,
  0x8808945,
  0x8808A46,
  0x8808B47
};

int POWER_ON = 0x880064A;
int POWER_OFF = 0x88C0051;




/*
 * Temperature settings
 */

const int RECV_TEMPERATURE_PIN = D2;

DHT dht(RECV_TEMPERATURE_PIN, DHT22); //Inizializza oggetto chiamato "dht", parametri: pin a cui è connesso il sensore, tipo di dht 11/22
float roomTemperature;
float roomHumidity;
float roomHeatIndex;

float outTemperature;
float outHumidity;
float outHeatIndex;

/*
 * Other var
 */

Scheduler taskManager;
HTTPClient http;


/*
 * Task declaration
 */
void taskTest();
Task t0(1000, TASK_FOREVER, &taskTest, &taskManager, true);

void measureRoomTemperatureTask();
Task t1(1000 * 5, TASK_FOREVER, &measureRoomTemperatureTask, &taskManager, false);

void sendRoomTemperatureTask();
Task t2(1000 * 10, TASK_FOREVER, &sendRoomTemperatureTask, &taskManager, false);

void measureSimulateOutTemperatureTask();
Task t3(1000 * 5, TASK_FOREVER, &measureSimulateOutTemperatureTask, &taskManager, false);

void sendOutTemperatureTask();
Task t4(1000 * 10, TASK_FOREVER, &sendOutTemperatureTask, &taskManager, false);

void MQTTLoopTask();
Task t5(1000 * 2, TASK_FOREVER, &MQTTLoopTask, &taskManager, false);

void IRRecvTask();
Task t6(300, TASK_FOREVER, &IRRecvTask, &taskManager, false);

void IRSendTask();
Task t7(300, TASK_FOREVER, &IRSendTask, &taskManager, false);


/*
 * 
 */


void taskTest() {
  int digitalValue = digitalRead(D0);
  int analogValue = digitalRead(D1);
  
  // print out the value you read:
  Serial.print("ANALOG");
  Serial.println(analogValue);
  
  Serial.print("DIGITAL");
  Serial.println(digitalValue);
  
}
/*
 * TemperatureTask and functions
 */

byte measureRoomTemperatureTaskLog = false;
void measureRoomTemperatureTask() {
  roomHumidity = dht.readHumidity();
  roomTemperature = dht.readTemperature();
  roomHeatIndex = dht.computeHeatIndex(roomTemperature, roomHumidity, false);
  //Stampa umidità e temperatura tramite monitor seriale
  if (measureRoomTemperatureTask) {
    Serial.print("Umidità della stanza: ");
    Serial.print(roomHumidity);
    Serial.println(" %");
    
    Serial.print("Temperatura della stanza: ");
    Serial.print(roomTemperature);
    Serial.println(" Celsius");
    
    Serial.print("Indice di calore: ");
    Serial.print(roomHeatIndex);
    Serial.println(" Celsius");
    
  }
}

byte measureSimulateOutTemperatureTaskLog = false;
void measureSimulateOutTemperatureTask() {
  http.begin("http://api.openweathermap.org/data/2.5/weather?lat=45.469706&units=metric&lon=9.237468&appid=4cdeae287c5efcdb83c9503436abf8d5");
  int httpCode = http.GET();

  if (httpCode > 0) { //Check the returning code
    
    StaticJsonDocument<64> filter;
    filter["main"]["temp"] = true;
    filter["main"]["humidity"] = true;
    
    String json = http.getString();   //Get the request response payload

    // https://arduinojson.org/v6/assistant/
    const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + 2*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(13);
    DynamicJsonDocument doc(capacity);
    deserializeJson(doc, json);
    JsonObject root = doc["main"].as<JsonObject>();
    

    outHumidity= root["humidity"].as<float>();
    outTemperature = root["temp"].as<float>();
    outHeatIndex = dht.computeHeatIndex(outTemperature, outHumidity, false);

    if (measureSimulateOutTemperatureTaskLog) {
      //serializeJsonPretty(doc, Serial);
      Serial.print("Umidità out: ");
      Serial.print(outHumidity);
      Serial.println(" %");
    
      Serial.print("Temperatura out: ");
      Serial.print(outTemperature);
      Serial.println(" Celsius");
    
      Serial.print("Indice di calore out: ");
      Serial.print(outHeatIndex);
      Serial.println(" Celsius");
    
    }
  }
 
  http.end();   //Close connection
}

// https://create.arduino.cc/projecthub/somenjana/sensing-the-comfort-level-of-atmosphere-using-humidex-3ee0df
float calcHumidex(float temperature, float humidity) {
  float B=(log(humidity/100)+((17.27*temperature)/(237.3+temperature)))/17.27; // value of "B", the intermediate dimentionless parameter has been calculated.
  float dew=(237.3*B)/(1-B); // The value of dew point has been calculated
  float humidex=temperature+0.5555*(6.11*exp(5417.753*(1/273.16-1/(273.15+dew)))-10); // the value of HUMIDEX has been claculated.
  return humidex;
}


/*
 * MQTTTask and functions
 */

const byte sendRoomTemperatureTaskLog = false;
void sendRoomTemperatureTask() {
  char data[8];
  
  dtostrf(roomTemperature, 6, 2, data);
  client.publish(roomTemperatureTopic, data);

  dtostrf(roomHumidity, 6, 2, data);
  client.publish(roomHumidityTopic, data);

  dtostrf(roomHeatIndex, 6, 2, data);
  client.publish(roomHeatIndexTopic, data);

  if (sendRoomTemperatureTaskLog) {
    Serial.print("Publish message roomHeatIndex: ");
    Serial.println(roomHeatIndex);
  }
}


const byte sendOutTemperatureTaskLog = false;
void sendOutTemperatureTask() {
  char data[8];
  
  dtostrf(outTemperature, 6, 2, data);
  client.publish(outTemperatureTopic, data);

  dtostrf(outHumidity, 6, 2, data);
  client.publish(outHumidityTopic, data);

  dtostrf(outHeatIndex, 6, 2, data);
  client.publish(outHeatIndexTopic, data);

  if (sendOutTemperatureTaskLog) {
    Serial.print("Publish message outHeatIndex: ");
    Serial.println(roomHeatIndex);
  }
}

const byte MQTTLoopTaskLog = true;
void MQTTLoopTask() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}

void reconnect() {
  while (!client.connected()) {
    if (MQTTLoopTaskLog)
      Serial.print("Attempting MQTT connection...");
    
    String clientId = "ValerioESP8266Client-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      if (MQTTLoopTaskLog)
        Serial.println("connected");

      client.publish(connectionTopic, "true");
      
      //client.subscribe("inTopic");
    } else {
      if (MQTTLoopTaskLog) {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
      }
      delay(5000);
    }
  }
}


/*
 * IRTask
 */

void IRRecvTask() {
 if (irrecv.decode(&results)){
    int value = results.value;
    Serial.println(value, HEX);
    irrecv.resume();
    Serial.println();
  }
  else {
    Serial.println("PASSATO");
  }
}


void IRSendTask() {
  irsend.sendNEC(0x880064A, 28);
}

/*
 * INIT
 */

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


void initPin() {
  pinMode(D1, INPUT);
  pinMode(D0, INPUT);  
}


void initObjects() {
  setup_wifi();
  
  client.setServer(mqtt_server, 1883);
  
  dht.begin();

  irrecv.enableIRIn();
  
  irsend.begin();
}

void setup() {
  Serial.begin(9600);
  initPin();
  initObjects();
}

void loop() {
  taskManager.execute();  
}