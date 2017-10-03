#include "easyMesh.h"
#include <PietteTech_DHT.h>
#include "meshConstants.h"

/**
 * The DHT-11 sensor values. The values are retrieved and broadcast in different time intervals.
 * For more info on this sensor check {@see myconstants.h}.
 * TODO consider changing from float to String or int.
 */
float DHT_temperature, DHT_humidity;


/**
 * The MQ-135 gas sensor value. This sensor needs to be warmed up for about 15min before
 * providing accurate results. The ADC is 10bit.
 */
uint16_t gasVal;


/**
 * A photoresistor (or light-dependent resistor, LDR) value. The ADC is 10bit.
 */
uint16_t LDRval;

/**
 * Broadcast flag. When active it`s time to broadcast our data.
 * Can be changes inside an ISR so it`s marked as volatile.
 */
volatile bool broadcast_ready;

/***
 * Runs only once before the first loop().
 * Some sensors may need initialization.
 * Some callbacks are attached to the mesh handler.
 * Interrupts need to be off while attaching ISRs to hardware timers.
 * Allowed debug types are:
 * ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE
 */
void setup() {
    Serial.begin(115200);
    pinMode(TIMER0_INTERRUPT_PIN, OUTPUT);

    mesh.setDebugMsgTypes(ERROR | MESH_STATUS | CONNECTION);
    mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);

    mesh.setReceiveCallback(&receivedCallback);
    mesh.setNewConnectionCallback(&newConnectionCallback);

    vars_init();
    timers_init();

    noInterrupts();
    timer0_isr_init();
    timer0_attachInterrupt(timer0_ISR);
    timer0_write(ESP.getCycleCount() + CPU_SEC);

    //timer1 hasn't been implemented in the current SDK version. Will remain offline for now.
    timer1_isr_init();
    timer1_attachInterrupt(timer1_ISR);
    timer1_write(ESP.getCycleCount() + CPU_SEC);
    interrupts();
}

/**
 * This routine is executed repeatedly when every other task has completed.
 * Always add a form of delay in the end (or yield) as the watchdog will bite otherwise.
 * At the end of this routine several network and scheduling background tasks are executed
 * so make sure to allow a loop() finish a few times per second. When the broadcast_ready is
 * true transmit the local sensor values.
 */
void loop() {
    switch (SENSOR_NO) {
        case 1: //DHT11
            if (DHT_temperature > DHT11_TEMPERATURE_THRESHOLD) {
                digitalWrite(TIMER0_INTERRUPT_PIN, LOW);
            } else {
                digitalWrite(TIMER0_INTERRUPT_PIN, HIGH);
            }
            break;
        case 2: //LDR
            if (LDRval < LDR_THRESHOLD) {
                digitalWrite(TIMER0_INTERRUPT_PIN, LOW);
            } else {
                digitalWrite(TIMER0_INTERRUPT_PIN, HIGH);
            }
            break;
        case 3: //MQ-135
            if (gasVal > MQ135_THRESHOLD) {
                digitalWrite(TIMER0_INTERRUPT_PIN, LOW);
            } else {
                digitalWrite(TIMER0_INTERRUPT_PIN, HIGH);
            }
            break;
        default:
            //Relay node, no sensors on board.
            break;
    }

    if (broadcast_ready) {
        broadcastReadings();
        broadcast_ready = false;
    }
}

/**
 * The timer0 Interrupt Service Routine. Must be very short.
 * Executed every X seconds by setting the timer as X * CPU_SEC
 * where CPU_SEC is 80.000.000 cycles (the esp8266 clock is 80MHz).
 * This routine broadcasts the locally already calculated sensor values in the mesh.
 * The built-in LED flashes while this routine is executed.
 */
void timer0_ISR() {
    timer0_write(ESP.getCycleCount() + BROADCAST_INTERVAL * CPU_SEC);
    broadcast_ready = true;
}

/**
 * Same as timer0_ISR (above). Does not work atm as it has not been implemented at the
 * official SDK.
 * TODO investigate further this issue.
 */
void timer1_ISR() {
    timer0_write(ESP.getCycleCount() + BROADCAST_INTERVAL * CPU_SEC); //Rearm the hardware timer.
    Serial.println("Timer1 works");
}

/**
 * Timer task that updates the mesh
 */
void meshUpdate_timer_task() {
    mesh.update();
    //Serial.println("Updating mesh...");
}

/**
 * Timer tasks that gets and stores locally the sensor readings.
 */
void getReadings_timer_task() {
    getReadings();
    //Serial.println("Getting readings...");
}

/**
 * Initialize the sketch variables in case of a premature broadcast.
 */
void vars_init() {
    DHT_temperature = DHT11_TEMPERATURE_THRESHOLD;
    DHT_humidity = 0;
    gasVal = MQ135_THRESHOLD;
    LDRval = LDR_THRESHOLD;
    broadcast_ready = false;
    acquirestatus = 0;
    acquireresult = 0;
}

/**
 * Disarms,attaches callbacks and finally arms the timers.
 * Call this method once.In os_timer_arm if the 3rd arg is true then the task is
 * executed periodically.
 */
void timers_init() {
    os_timer_disarm(&meshUpdateTimer);
    os_timer_disarm(&readingsTimer);

    os_timer_setfn(&meshUpdateTimer, (os_timer_func_t *) meshUpdate_timer_task, NULL);
    os_timer_setfn(&readingsTimer, (os_timer_func_t *) getReadings_timer_task, NULL);

    os_timer_arm(&meshUpdateTimer, MESH_UPDATE_INTERVAL, true);
    os_timer_arm(&readingsTimer, SENSOR_UPDATE_INTERVAL, true);
}

/**
 * Set a callback routine for any messages that are addressed to this node.
 * @param from the id of the original sender of the message
 * @param msg a string that contains the message.The message can be anything.
 * A JSON, some other text string, or binary data.
 */
void receivedCallback(uint32_t from, String &msg) {
    Serial.printf("Received from %d : %s\n", from, msg.c_str());
}

/**
 * This fires every time the local node makes a new connection.
 * @param adopt a boolean value that indicates whether the mesh has determined to adopt
 * the remote nodes timebase or not.  If `adopt == true`, then this node has adopted the remote
 * node’s timebase.
 */
void newConnectionCallback(bool adopt) {
    Serial.printf("New Connection, adopt=%d\n", adopt);
    Serial.printf("Connection count: %d\n", mesh.connectionCount());
}

/**
 * Broadcasts the existing values of the sensor(s) produced by getReadings().
 * Uses interrupts so DO NOT call inside an ISR, instead use getReadings to obtain
 * the values and then publish the results with this method.
 * case1 -> DHT11
 * case2 -> Photoresistor
 * case3 -> MQ135
 */
void broadcastReadings() {
    String msg;
    switch (SENSOR_NO) {
        case 1:
            msg = "C:" + String(DHT_temperature) + "  H:" + String(DHT_humidity);
            if (acquirestatus == 1) {
                dht.reset();
            }
            if (!bDHTstarted) {
                dht.acquire();// non blocking method
                bDHTstarted = true;
            }
            break;
        case 2:
            msg = "LDR: " + String(LDRval);
            break;
        case 3:
            msg = "Gas PPM: " + String(gasVal);
            break;
        default:
            //Relay node, no sensors on board.
            return;
    }
    mesh.sendBroadcast(msg);
    Serial.println(msg);
}

/**
 * This method SAVES locally the sensor results.
 * Very-fast routine without interrupts, can be called by an ISR.
 * case1 -> DHT11
 * case2 -> Photoresistor
 * case3 -> MQ135
 */
void getReadings() {
    switch (SENSOR_NO) {
        case 1:
            if (bDHTstarted) {
                acquirestatus = dht.acquiring();
                if (!acquirestatus) {
                    acquireresult = dht.getStatus();
                    if (acquireresult == 0) {
                        DHT_temperature = dht.getCelsius();
                        DHT_humidity = dht.getHumidity();
                    }
                    bDHTstarted = false;
                }
            }
            break;
        case 2:
            LDRval = analogRead(ANALOGPIN);
            break;
        case 3:
            gasVal = analogRead(ANALOGPIN);
            break;
        default:
            //Relay node.
            return;
    }
}
