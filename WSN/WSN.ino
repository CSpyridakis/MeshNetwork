#include "easyMesh.h"
#include <DHT.h>

#define SENSOR_NO (3)
#define ANALOGPIN A0

//Mesh vars
#define   LED                     5             // GPIO number of connected LED.
#define   MESH_UPDATE_INTERVAL    1000L         // microseconds until cycle repeat
#define   BROADCAST_INTERVAL      5000L         // microseconds between each broadcast

#define   MESH_PREFIX     "mesh"
#define   MESH_PASSWORD   "12345678"
#define   MESH_PORT       5555

static easyMesh  mesh;   

os_timer_t meshUpdateTimer;   //Maintenance mesh network tasks
os_timer_t readingsTimer;     //Readings from sensor


// DHT vars
#define DHTTYPE DHT11   // DHT 11
const int DHTPin = 12;  //~D6
DHT dht(DHTPin, DHTTYPE);

void setup() {
  //Make sure the Watchdog bites every 8s.
  ESP.wdtDisable();
  ESP.wdtEnable(WDTO_8S);
  
	Serial.begin(115200);
	pinMode(LED, OUTPUT);
 
  switch(SENSOR_NO){  //Initialize the correct sensors. 
    case 1:
      dht.begin();
      break;
    default:
      break;
  }
  
  // Allowed types are: ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE 
	mesh.setDebugMsgTypes(ERROR | STARTUP);  
	mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);

  //Attach the needed callbacks
	mesh.setReceiveCallback(&receivedCallback);
	mesh.setNewConnectionCallback(&newConnectionCallback);

  timers_init();
}

void loop() {
  yield();
}

//Timer tasks and init_timer funcs.
//Turn off interupts when in an ISR.
void meshUpdate_timer_task() {
    os_intr_lock();
    mesh.update();
    os_intr_unlock();
}

void getReadings_timer_task() {
    os_intr_lock();
    getReadings();
    os_intr_unlock();
} 
   
void timers_init(void) {
    os_timer_disarm(&meshUpdateTimer);
    os_timer_disarm(&readingsTimer);
    
    os_timer_setfn(&meshUpdateTimer,(os_timer_func_t *) meshUpdate_timer_task, NULL); //Schedule the tasks
    os_timer_setfn(&readingsTimer,(os_timer_func_t *) getReadings_timer_task, NULL);

    os_timer_arm(&meshUpdateTimer, MESH_UPDATE_INTERVAL, true); //3rd arg -> Repeat task
    os_timer_arm(&readingsTimer, BROADCAST_INTERVAL, true);
}

void receivedCallback(uint32_t from, String &msg) {
	Serial.printf("Received from %d : %s\n", from, msg.c_str());
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
              return "DHT failed.";   
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

String getPhotoresistorReadings(){
  int sensorValue = analogRead(ANALOGPIN);
  float value = sensorValue * (100 / 1023.0);
  return "Light: " + String(value) + "%";  
}

String getGasReadings(){
  return "Gas PPM:" + String(analogRead(ANALOGPIN));
}

//This method sends data from the correct sensor(s).
void getReadings(){
    String message = "";
    switch(SENSOR_NO){
      case 1: //DHT11
        message = getDHTreadings();
        break;
      case 2: //Photoresistor
        message = getPhotoresistorReadings();
        break;      
      case 3: //MQ135
        message = getGasReadings();
        break;      
      default:
        message = "Failed to get a reading";
    }
    mesh.sendBroadcast(message);  //Send to mesh
    Serial.println(message);      //Send to self
}

