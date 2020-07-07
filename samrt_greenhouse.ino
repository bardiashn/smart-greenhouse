//IN THE NAME OF WISDOM//
//Smart Greenhouse//
//Amirhossein Shoaraye Nejati//


#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#define DHTTYPE DHT11   // DHT 11

const char* ssid = "NEJATI";
const char* password = "amir 1377";
const char* mqtt_server = "192.168.1.106";

WiFiClient espClient;
PubSubClient client(espClient);

#define DHTTYPE DHT11 
#define DHTPIN D4
#define LDRPIN A0
#define relay_fan D3
#define relay_leds1 D7
#define relay_leds2 D9
#define relay_leds3 D10
#define relay_leds4 D11

// Initialize DHT sensor.
DHT dht(DHTPIN, DHTTYPE);


//Define variables
float hum = 0;
float temp = 0;
int lum =0;
long lastRecu =0;
float situ;

//define treshold for variables
//tresholds in % now
int lum_limit = 80;
int temp_limit = 29.1;
int time_FAN_ON = 2000; 
bool leds_ON = false;
//Flag master if command received => the actuators won't be triggered while flag_master == 1
int Flag_master_LED =0;

//Delcaration of prototypes 
void getData();

// connecting ESP8266 to router
void setup_wifi() {
  delay(10);
 
  //connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected - ESP IP address: ");
  Serial.println(WiFi.localIP());
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void callback(String topic, byte* message, unsigned int length) {
  
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String payload;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    payload+= (char)message[i];
  }
  Serial.println();

  // If a message is received on the topic Garden/Lights,check if the message is either on or off. Turns the lamp GPIO according to the message
     if(topic=="Garden/Lights")
      {
          Serial.print(" Switching Leds ");
          if(payload == "true"){
            digitalWrite(relay_leds1, HIGH);
            digitalWrite(relay_leds2, HIGH);
            digitalWrite(relay_leds3, HIGH);
            digitalWrite(relay_leds4, HIGH);
            delay(15000);
            Serial.print("On");
          }
          else if(payload == "false"){
            digitalWrite(relay_leds1, LOW);
            digitalWrite(relay_leds2, LOW);
            digitalWrite(relay_leds3, LOW);
            digitalWrite(relay_leds4, LOW);
            Serial.print("Off");
            }
        
      }
      else if(topic=="Garden/temp")
      {
      Serial.print(" Switching fan ");
      if(payload == "true"){
        digitalWrite(relay_fan,HIGH);
        delay(15000);
        Serial.print("On");
      }
      else if(payload == "false"){
        digitalWrite(relay_fan, LOW);
        Serial.print("Off");
      }
  }
  
  Serial.println();
}


//reconnecting ESP8266 to MQTT broker
void reconnect() 
{
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");  
      // Subscribe or resubscribe to a topic
      // You can subscribe to more topics (to control more LEDs in this example)
      client.subscribe("Garden/Lights");
      client.subscribe("Garden/temp");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// The setup function sets nodeMCU GPIOs to Outputs, starts the serial communication at a baud rate of 115200
void setup() {
  Serial.println("start setup \n");
  //Relays
  pinMode(relay_fan, OUTPUT);
  pinMode(relay_leds1, OUTPUT);
  pinMode(relay_leds2, OUTPUT);
  pinMode(relay_leds3, OUTPUT);
  pinMode(relay_leds4, OUTPUT);
  digitalWrite(relay_leds1, HIGH);
  digitalWrite(relay_leds2, HIGH);
  digitalWrite(relay_leds3, HIGH);
  digitalWrite(relay_leds4, HIGH);
  dht.begin();
  Serial.begin(115200);
  setup_wifi();
  // Set the callback function, the server IP and port to communicate with
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  client.subscribe("Garden/Lights");
  client.subscribe("Garden/temp");
  delay(2000);

Serial.println("End setup \n");

}

void loop() 
{
 if (!client.connected()) {
    reconnect();
  }
  if(!client.loop())
    client.connect("ESP8266Client");
    long now= millis();
getData();
delay(5000);
client.loop();

}

void getData()
 {
  
  Serial.println("analysis started");
  getDhtData();
  getLDRData();
  ProcessingLights();
  Processing_Fan();
 }

 void getDhtData(void)
{
  float tempIni = temp;
  float humIni = hum;
  temp = dht.readTemperature();
  hum = dht.readHumidity();
  if (isnan(hum) || isnan(temp))   // Check if any reads failed and exit early (to try again).
  {
    Serial.println("Failed to read from DHT sensor!");
    temp = tempIni;
    hum = humIni;
    return;
  }
  Serial.println("Temperature :");
  Serial.println(temp);
  Serial.println("Humidity :");
  Serial.println(hum);
  
  // To send data via MQTT we need to compute our datas into a table of char
  float hic = dht.computeHeatIndex(temp, hum, false);
  static char temperatureTemp[7];
  dtostrf(hic, 6, 2, temperatureTemp);

  static char humidityTemp[7];
  dtostrf(hum, 6, 2, humidityTemp);
  // we finaly publish in the topic our values, it'll be read by the MQTT broker and sent to the dashboard in our case
  client.publish("Garden/Temperature", temperatureTemp);
  client.publish("Garden/Humidity", humidityTemp);

  
}

void getLDRData(void)
{ 
  lum = analogRead(LDRPIN);
  lum = (lum *100)/1024;
  static char lumTemp[7];
  dtostrf(lum, 6, 2, lumTemp);
  client.publish("Garden/Lum", lumTemp);
  
  Serial.println("Light measured:");
  Serial.println(lum);
}
//funtion processing the data coming from the LDR, turning OFF and ON the artificial light system if needed.
void ProcessingLights()
{
  if(Flag_master_LED ==0)
  {
  Serial.println("Start processing Light \n");
  if(lum < lum_limit && (leds_ON == false ))
  {
    Serial.println("LEDs ON");
    digitalWrite(relay_leds1, HIGH);
    digitalWrite(relay_leds2, HIGH);
    digitalWrite(relay_leds3, HIGH);
    digitalWrite(relay_leds4, HIGH);
    situ= 1;
    static char situation[7];
    dtostrf(situ, 6, 2, situation);
    client.publish("lightsystem(ON)", situation);
  }
  else
  {
    Serial.println("LEDs OFF");
    digitalWrite(relay_leds1, LOW);
    digitalWrite(relay_leds2, LOW);
    digitalWrite(relay_leds3, LOW);
    digitalWrite(relay_leds4, LOW);
    situ= 2;
    static char situation[7];
    dtostrf(situ, 6, 2, situation);
    client.publish("lightsystem(OFF)", situation);
  }
  }
  else 
  {
    Serial.println("flag_master_LEDs : ON");
  }
  delay(200);
}

//function running the processing of temperature Data, and triggering the FAN if needed
void Processing_Fan()
{
  Serial.println("Processing fan \n");
  if(temp > temp_limit)
  {
    Serial.println("FAN ON");
    digitalWrite(relay_fan,HIGH);
    situ= 3;
    static char situation[7];
    dtostrf(situ, 6, 2, situation);
    client.publish("fansystem(ON)", situation);
    delay(2000);

  }
  else
  {
     Serial.println("FAN OFF");
     digitalWrite(relay_fan,LOW);
     situ= 4;
    static char situation[7];
    dtostrf(situ, 6, 2, situation);
    client.publish("fansystem(OFF)", situation);
 
}
}
