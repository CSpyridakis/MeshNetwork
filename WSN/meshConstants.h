#ifndef MESHNETWORK_MYCONSTANTS_H
#define MESHNETWORK_MYCONSTANTS_H

#define SENSOR_NO 3
#define ANALOGPIN A0
#define DHTPin    12              //GPIO 12
#define DHTTYPE   DHT11
#define TIMER0_INTERRUPT_PIN 16   //GPIO 16. Interrupt attached. Also the Built in led pin.
#define CPU_SEC   80000000L       //80MHz -> 1 sec

#define   MESH_UPDATE_INTERVAL    100L          // microseconds between each broadcast
#define   SENSOR_UPDATE_INTERVAL  1000L         // microseconds between each sensor update
#define   BROADCAST_INTERVAL      5             // seconds between each broadcast

#define   MESH_PREFIX     "mesh"
#define   MESH_PASSWORD   "12345678"
#define   MESH_PORT       5555


/**
 * Thresholds for each sensor.
 * On MQ135 start the buzzer.
 * On LDR turn on the led strip.
 * On DHT11 turn on the fan.
 */
uint16_t MQ135_THRESHOLD = 200;
uint16_t LDR_THRESHOLD = 500;
uint16_t DHT11_TEMPERATURE_THRESHOLD = 25;


/**
 * The mesh handler. All actions such as Signal and Broadcast are invoked by this object.
 */
easyMesh mesh;

/**
 * Schedules the mesh maintenance tasks required in order to keep the mesh network
 * alive and balanced.
 */
os_timer_t meshUpdateTimer;

/**
 * Schedules the tasks required to read the values of the connected sensor(s) and store
 * them locally.
 */
os_timer_t readingsTimer;

/**
 * *********************** DHT 11 ****************************
 * Reading temperature or humidity takes about 250 milliseconds!
 * Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
 * Connect pin 1 (on the left) of the sensor to +5V
 * NOTE: If using a board with 3.3V logic like an Arduino Due connect pin 1
 * to 3.3V instead of 5V!
 * Connect pin 2 of the sensor to whatever your DHTPIN is
 * Connect pin 4 (on the right) of the sensor to GROUND
 * Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor
 */
DHT dht(DHTPin, DHTTYPE);

#endif
