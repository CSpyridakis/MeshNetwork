#include "easyMesh.h"
#include <DHT.h>

//Mesh vars
#define   LED             5           // GPIO number of connected LED.
#define   UPDATE_INTERVAL    10000    // Sensor update interval in ms.
#define   LED_DURATION       2000     // Sensor update interval in ms.

#define   MESH_PREFIX     "mesh"
#define   MESH_PASSWORD   "12345678"
#define   MESH_PORT       5555
easyMesh  mesh;   
uint32_t sendMessageTime = 0;

// DHT vars
#define DHTTYPE DHT11   // DHT 11
const int DHTPin = 12;  //~D6
DHT dht(DHTPin, DHTTYPE);

void setup() {
	Serial.begin(115200);
	pinMode(LED, OUTPUT);
  
  dht.begin();
  
	//mesh.setDebugMsgTypes(ERROR | STARTUP);  // Allowed types are: ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE

	mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
 
	mesh.setReceiveCallback(&receivedCallback);
	mesh.setNewConnectionCallback(&newConnectionCallback);

	randomSeed(analogRead(A0));
}

void loop() {
  digitalWrite(LED, true);
	mesh.update();
  getReadings(mesh);
  delay(LED_DURATION);
  digitalWrite(LED, false);
  delay(UPDATE_INTERVAL-LED_DURATION);
}

void receivedCallback(uint32_t from, String &msg) {
	Serial.printf("Received from %d msg=%s\n", from, msg.c_str());
}

void newConnectionCallback(bool adopt) {
	Serial.printf("New Connection, adopt=%d\n", adopt);
  Serial.printf("Connection count: %d\n", mesh.connectionCount() );
}

//Select this method only when DHT11 is attached!
String getDHTreadings(){
            char celsiusTemp[7];
            char fahrenheitTemp[7];
            char humidityTemp[7];
            float hic;
            float hif; 
             
            // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
            // Read the humidity value.
            float h = dht.readHumidity();
            // Read temperature as Celsius (the default)
            float t = dht.readTemperature();
            // Read temperature as Fahrenheit (isFahrenheit = true)
            float f = dht.readTemperature(true);
            // Check if any reads failed and exit early (to try again).
            if (isnan(h) || isnan(t) || isnan(f)) {
              strcpy(celsiusTemp,"Failed");
              strcpy(fahrenheitTemp, "Failed");
              strcpy(humidityTemp, "Failed");      
              return "";   
            }else{
              // Computes temperature values in Celsius + Fahrenheit and Humidity
              hic = dht.computeHeatIndex(t, h, false);       
              dtostrf(hic, 6, 2, celsiusTemp);             
              hif = dht.computeHeatIndex(f, h);
              dtostrf(hif, 6, 2, fahrenheitTemp);         
              dtostrf(h, 6, 2, humidityTemp);
            }   
            return "  C:"+String(celsiusTemp)+" F:"+String(fahrenheitTemp)+"  H:"+String(humidityTemp)+"%";
}

//This method sends OTA the data from the correct sensor(s).
//TODO put a switch statement and send data according to the connected sensors.
void getReadings(easyMesh mesh){
    String message = getDHTreadings();
    //Serial.println("SENDING: "+message);
    
    if(message.equals("")){ //Sensor failed
        
    }else{
        mesh.sendBroadcast(message);
    }
}

