#include <Arduino.h>
#include <ArduinoJson.h>
#include <SimpleList.h>

extern "C" {
#include "user_interface.h"
#include "espconn.h"
}

#include "easyMesh.h"
#include "easyMeshSync.h"


easyMesh* staticThis;
uint16_t  count = 0;

/**
 * Initializes the mesh network by reseting the wifi connection and assigning blank values to the mesh variables.
 * Call in the setup phase of the project but ONLY ONCE.
 * @param prefix The network prefix of every node in the mesh network.
 * @param password The string required to be allowed access in the network.
 * @param port The port this network will be built on. Used for TCP connections.
 */
void ICACHE_FLASH_ATTR easyMesh::init( String prefix, String password, uint16_t port ) {
    // shut everything down, start with a blank slate.
    debugMsg( STARTUP, "init():\n",    wifi_station_set_auto_connect( 0 ));
    
    if ( wifi_station_get_connect_status() != STATION_IDLE ) {
        debugMsg( ERROR, "Station is doing something... wierd!? status=%d\n", wifi_station_get_connect_status());
        wifi_station_disconnect();
    }
    wifi_softap_dhcps_stop();
    
    wifi_set_event_handler_cb( wifiEventCb );
    
    staticThis = this;  // provides a way for static callback methods to access "this" object;
    
    // start configuration
    debugMsg( GENERAL, "wifi_set_opmode(STATIONAP_MODE) succeeded? %d\n", wifi_set_opmode( STATIONAP_MODE ) );
    
    _meshPrefix = prefix;
    _meshPassword = password;
    _meshPort = port;
    _chipId = system_get_chip_id();
    _mySSID = _meshPrefix + String( _chipId );
    
    apInit();       // setup AP
    stationInit();  // setup station
    
    debugMsg( GENERAL, "init(): tcp_max_con=%u\n", espconn_tcp_get_max_con() );
}

/**
 * Starts the update sequence.
 */
void ICACHE_FLASH_ATTR easyMesh::update( void ) {
    manageStation();
    manageConnections();
    return;
}

/**
 * Sends a message only once to a specific node in the mesh.
 * @param destId The chip unique ID of the receiver node.
 * @param msg The message to be sent.
 */
bool ICACHE_FLASH_ATTR easyMesh::sendSingle( uint32_t &destId, String &msg ){
    debugMsg( COMMUNICATION, "sendSingle(): dest=%d msg=%s\n", destId, msg.c_str());
    sendMessage( destId, SINGLE, msg );
}

/**
 * Sends a message to every node in the network.
 * @param msg The message to be broadcast.
 */
bool ICACHE_FLASH_ATTR easyMesh::sendBroadcast( String &msg ) {
    debugMsg( COMMUNICATION, "sendBroadcast(): msg=%s\n", msg.c_str());
    broadcastMessage( _chipId, BROADCAST, msg );
}