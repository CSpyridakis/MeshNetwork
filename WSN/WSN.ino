#include "easyMesh.h"
#include <DHT.h>

#define SENSOR_NO 3
#define ANALOGPIN A0
#define DHTPin    12 //GPIO 12
#define DHTTYPE   DHT11
#define LED       5             // GPIO number of connected LED.

//Mesh vars
#define   MESH_UPDATE_INTERVAL    500L          // microseconds between each broadcast
#define   SENSOR_UPDATE_INTERVAL  1000L         // microseconds between each sensor update
#define   BROADCAST_INTERVAL      1000L         // microseconds between each broadcast

#define   MESH_PREFIX     "mesh"
#define   MESH_PASSWORD   "12345678"
#define   MESH_PORT       5555

easyMesh  mesh;   

os_timer_t meshUpdateTimer;   //Maintenance mesh network tasks
os_timer_t readingsTimer;     //Readings from sensor (locally)
os_timer_t broadcastTimer;    //Broadcast local sensor values.

//*********************** DHT 11 ****************************
// Reading temperature or humidity takes about 250 milliseconds!
// Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
// Connect pin 1 (on the left) of the sensor to +5V
// NOTE: If using a board with 3.3V logic like an Arduino Due connect pin 1
// to 3.3V instead of 5V!
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor
DHT dht(DHTPin, DHTTYPE);
float DHT_temperature,DHT_heatIndex,DHT_humidity;

//*********************** MQ-135 ****************************
//A gas sensor.Needs about an hour of use to produce valid readings
int gasVal;


//*********************** LDR photoresistor *****************
int LDRval;



void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
 
  switch(SENSOR_NO){  //Initialize the correct sensors. 
    case 1:
      dht.begin();
      break;
    default:
      break;
  }
  
  // ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE 
  mesh.setDebugMsgTypes(ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE );  
  mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);

  //Attach the mesh callbacks
  mesh.setReceiveCallback(&receivedCallback);
  mesh.setNewConnectionCallback(&newConnectionCallback);

  timers_init();
}

void loop() {
  delay(0);
}

//Timer task that updates the mesh
void meshUpdate_timer_task() {
    mesh.update();
}

//Timer tasks that gets and stores locally the sensor readings.
void getReadings_timer_task() {
    getReadings();
} 

//Timer tasks that broadcasts the sensor readings.
void broadcastReadings_timer_task() {
    broadcastReadings();
} 

//Disarms,attaches callbacks and finally arms the timers.
//Call this method in setup().
void timers_init(void) {
    os_timer_disarm(&meshUpdateTimer);
    os_timer_disarm(&readingsTimer);
    os_timer_disarm(&broadcastTimer);

    os_timer_setfn(&meshUpdateTimer,(os_timer_func_t *) meshUpdate_timer_task, NULL); //Schedule the tasks
    os_timer_setfn(&readingsTimer,(os_timer_func_t *) getReadings_timer_task, NULL);
    os_timer_setfn(&broadcastTimer,(os_timer_func_t *) broadcastReadings_timer_task, NULL);

    //If the 3rd arg is true then the task is executed periodically.
    os_timer_arm(&meshUpdateTimer, MESH_UPDATE_INTERVAL, true); 
    os_timer_arm(&readingsTimer, SENSOR_UPDATE_INTERVAL, true);
    os_timer_arm(&broadcastTimer, BROADCAST_INTERVAL, true);
}

void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("Received from %d : %s\n", from, msg.c_str());
}

void newConnectionCallback(bool adopt) {
  Serial.printf("New Connection, adopt=%d\n", adopt);
  Serial.printf("Connection count: %d\n", mesh.connectionCount() );
}

//Broadcasts the existing values of the sensor(s) produced by getReadings().
//Uses interrupts. DO NOT call inside an ISR, instead use getReadings to
//obtain the values and then publish the results with this method.
void broadcastReadings(){
  String msg;
  switch(SENSOR_NO){
    case 1://DHT
      msg = "C:"+String(DHT_temperature)+"  H:"+String(DHT_humidity) + "  HIC:"+String(DHT_heatIndex);
      break;
    case 2://LDR
      msg = "LDR: " + String(LDRval); 
      break;
    case 3://MQ-135
      msg = "Gas PPM: " + String(gasVal);
      break;
    default:
      msg ="Error at publishValues default statement.";
      break;
  }
  mesh.sendBroadcast(msg);
  Serial.println(msg);
}

//This method SAVES locally the sensor results.
//Very-fast routine without interrupts, can be called by an ISR.
void getReadings(){
    switch(SENSOR_NO){
      case 1: //DHT11
         DHT_humidity = dht.readHumidity();
         DHT_temperature = dht.readTemperature();
         DHT_heatIndex = dht.computeHeatIndex(DHT_temperature, DHT_humidity, false); //Heat Index
        break;
      case 2: //Photoresistor
        LDRval = analogRead(ANALOGPIN);
        break;      
      case 3: //MQ135
        gasVal = analogRead(ANALOGPIN);
        break;      
      default:
        break;
    }
}

