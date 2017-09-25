#include "easyMesh.h"
#include <DHT.h>

//Mesh vars
#define   LED             5       // GPIO number of connected LED
#define   BLINK_PERIOD    1000000 // microseconds until cycle repeat
#define   BLINK_DURATION  100000  // microseconds LED is on for
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
	mesh.update();

	// run the blinky
	bool  onFlag = false;
	uint32_t cycleTime = mesh.getNodeTime() % BLINK_PERIOD;
	for (uint8_t i = 0; i < (mesh.connectionCount() + 1); i++) {
		uint32_t onTime = BLINK_DURATION * i * 2;

		if (cycleTime > onTime && cycleTime < onTime + BLINK_DURATION)
			onFlag = true;
	}
	digitalWrite(LED, onFlag);

	// get next random time for send message
	if (sendMessageTime == 0) {
		sendMessageTime = mesh.getNodeTime() + random(1000000, 5000000);
	}

	// if the time is ripe, send everyone a message!
	if (sendMessageTime != 0 && sendMessageTime < mesh.getNodeTime()) {
    getReadings(mesh);
		sendMessageTime = 0;
	}
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
              Serial.println("Failed to read from DHT sensor!");
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

