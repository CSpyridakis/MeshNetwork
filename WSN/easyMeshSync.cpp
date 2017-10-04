#include <Arduino.h>
#include <SimpleList.h>
#include <ArduinoJson.h>

#include "easyMesh.h"
#include "easyMeshSync.h"

extern easyMesh* staticThis;
uint32_t timeAdjuster = 0;

/**
 * Returns the adjusted node time.
 */
uint32_t ICACHE_FLASH_ATTR easyMesh::getNodeTime( void ) {
    uint32_t ret = system_get_time() + timeAdjuster;
    debugMsg( GENERAL, "getNodeTime(): time=%d\n", ret);
    return ret;
}

/**
 * Returns the timestamp of the mesh network.
 */
String ICACHE_FLASH_ATTR timeSync::buildTimeStamp( void ) {
    staticThis->debugMsg( SYNC, "buildTimeStamp(): num=%d\n", num);

    if ( num > TIME_SYNC_CYCLES )
        staticThis->debugMsg( ERROR, "buildTimeStamp(): timeSync not started properly\n");

    StaticJsonBuffer<75> jsonBuffer;
    JsonObject& timeStampObj = jsonBuffer.createObject();
    times[num] = staticThis->getNodeTime();
    timeStampObj["time"] = times[num];
    timeStampObj["num"] = num;
    bool remoteAdopt = !adopt;
    timeStampObj["adopt"] = remoteAdopt;

    String timeStampStr;
    timeStampObj.printTo( timeStampStr );

    staticThis->debugMsg( SYNC, "buildTimeStamp(): timeStamp=%s\n", timeStampStr.c_str() );
    return timeStampStr;
}

/**
 *  Sets the time related internal variables of the mesh.
 *  @param str the given timestamp
 */
bool ICACHE_FLASH_ATTR timeSync::processTimeStamp( String &str ) {
    staticThis->debugMsg( SYNC, "processTimeStamp(): str=%s\n", str.c_str());

    DynamicJsonBuffer jsonBuffer(50 );
    JsonObject& timeStampObj = jsonBuffer.parseObject(str);

    if ( !timeStampObj.success() ) {
        staticThis->debugMsg( ERROR, "processTimeStamp(): out of memory1?\n" );
        return false;
    }

    num = timeStampObj.get<uint32_t>("num");

    times[num] = timeStampObj.get<uint32_t>("time");
    adopt = timeStampObj.get<bool>("adopt");

    num++;

    if ( num < TIME_SYNC_CYCLES ) {
        str = buildTimeStamp();
        return true;
    }
    else {
        return false;
    }
}

/**
 *  Adjusts the mesh time periodically.
 */
void ICACHE_FLASH_ATTR timeSync::calcAdjustment ( bool odd ) {
    staticThis->debugMsg(SYNC, "calcAdjustment(): odd=%u\n", odd);

    uint32_t bestInterval = 0xFFFFFFFF;
    uint8_t bestIndex;
    uint32_t temp;

    for (int i = 0; i < TIME_SYNC_CYCLES; i++) {

        if (i % 2 == odd) {
            temp = times[i + 2] - times[i];

            if (i < TIME_SYNC_CYCLES - 2) {
                if (temp < bestInterval) {
                    bestInterval = temp;
                    bestIndex = i;
                }
            }
        }
    }
    staticThis->debugMsg(SYNC, "best interval=%u, best index=%u\n", bestInterval, bestIndex);

    uint32_t adopterTime = times[bestIndex] + (bestInterval / 2);
    uint32_t adjustment = times[bestIndex + 1] - adopterTime;

    staticThis->debugMsg(SYNC, "new calc time=%u, adoptedTime=%u\n", adopterTime + adjustment, times[bestIndex + 1]);

    timeAdjuster += adjustment;
}

/**
 * Starts syncing with another node by sending its subs and changing the status to IN_PROGRESS.
 */
void ICACHE_FLASH_ATTR easyMesh::startNodeSync( meshConnectionType *conn ) {
    debugMsg( SYNC, "startNodeSync(): with %u\n", conn->chipId);

    String subs = subConnectionJson( conn );
    sendMessage( conn, conn->chipId, NODE_SYNC_REQUEST, subs );
    conn->nodeSyncRequest = getNodeTime();
    conn->nodeSyncStatus = IN_PROGRESS;
}


/**
 * Takes action when a new sync request is made.
 * After gathering all the info from the node who requested the sync
 * we check to see if we are already connected somewhere and if so we drop the connection.
 * We then check for a change in subconnections and resync them at the end of this method if needed.
 * Finally, in case of a sync_request this node needs to send a sync_reply to its subs or in case of a
 * sync_reply the node needs to reset its sync timer.
 */
void ICACHE_FLASH_ATTR easyMesh::handleNodeSync( meshConnectionType *conn, JsonObject& root ) {
    debugMsg(SYNC, "handleNodeSync(): with %u\n", conn->chipId);

    meshPackageType type = (meshPackageType) (int) root["type"];
    uint32_t remoteChipId = (uint32_t) root["from"];
    uint32_t destId = (uint32_t) root["dest"];
    bool reSyncAllSubConnections = false;

    if ((destId == 0) && (findConnection(remoteChipId) != NULL)) {
        debugMsg(SYNC, "handleNodeSync(): Already connected to node %d.  Dropping\n", conn->chipId);
        closeConnection(conn);
        return;
    }

    if (conn->chipId != remoteChipId) {
        debugMsg(SYNC, "handleNodeSync(): conn->chipId updated from %d to %d\n", conn->chipId, remoteChipId);
        conn->chipId = remoteChipId;

    }

    // check to see if subs have changed.
    String inComingSubs = root["subs"];
    if (!conn->subConnections.equals(inComingSubs)) {  // change in the network
        reSyncAllSubConnections = true;
        conn->subConnections = inComingSubs;
    }

    switch (type) {
        case NODE_SYNC_REQUEST: {
            debugMsg(SYNC, "handleNodeSync(): valid NODE_SYNC_REQUEST %d sending NODE_SYNC_REPLY\n", conn->chipId);
            String myOtherSubConnections = subConnectionJson(conn);
            sendMessage(conn, _chipId, NODE_SYNC_REPLY, myOtherSubConnections);
            break;
        }
        case NODE_SYNC_REPLY:
            debugMsg(SYNC, "handleNodeSync(): valid NODE_SYNC_REPLY from %d\n", conn->chipId);
            conn->nodeSyncRequest = 0;  //reset nodeSyncRequest Timer  ????
            if (conn->lastTimeSync == 0)
                startTimeSync(conn);
            break;
        default:
            debugMsg(ERROR, "handleNodeSync(): weird type? %d\n", type);
    }

    if (reSyncAllSubConnections) {
        SimpleList<meshConnectionType>::iterator connection = _connections.begin();
        while (connection != _connections.end()) {
            connection->nodeSyncStatus = NEEDED;
            connection++;
        }
    }

    conn->nodeSyncStatus = COMPLETE;  // mark this connection nodeSync'd
}

/**
 * Checks if timebase needs to be adopted and starts the time sync.
 */
void ICACHE_FLASH_ATTR easyMesh::startTimeSync( meshConnectionType *conn ) {
    debugMsg( SYNC, "startTimeSync(): with %d\n", conn->chipId );

    if ( conn->time.num > TIME_SYNC_CYCLES ) {
        debugMsg( ERROR, "startTimeSync(): Error timeSync.num not reset conn->time.num=%d\n", conn->time.num );
    }

    conn->time.num = 0;

    conn->time.adopt = adoptionCalc( conn ); // do I adopt the estblished time? See below

    String timeStamp = conn->time.buildTimeStamp();
    staticThis->sendMessage( conn, _chipId, TIME_SYNC, timeStamp );

    conn->timeSyncStatus = IN_PROGRESS;
}


/**
 * Make the adoption calculation.
 * Figure out how many nodes I am connected to and answer positively only if my subs are less than the remote subs.
 */
bool ICACHE_FLASH_ATTR easyMesh::adoptionCalc( meshConnectionType *conn ) {
    uint16_t mySubCount = connectionCount( conn );  //exclude this connection.
    uint16_t remoteSubCount = jsonSubConnCount( conn->subConnections );

    bool ret = mySubCount > remoteSubCount ? false : true;

    debugMsg( GENERAL, "adoptionCalc(): mySubCount=%d remoteSubCount=%d ret = %d\n", mySubCount, remoteSubCount, ret);

    return ret;
}

/**
 * Update the timestamp of the connection.
 * Find and flag all connections for re-timeSync
 */
void ICACHE_FLASH_ATTR easyMesh::handleTimeSync( meshConnectionType *conn, JsonObject& root ) {

    String timeStamp = root["msg"];
    debugMsg( SYNC, "handleTimeSync(): with %d in timestamp=%s\n", conn->chipId, timeStamp.c_str());

    conn->time.processTimeStamp( timeStamp );  //varifies timeStamp and updates it with a new one.

    debugMsg( SYNC, "handleTimeSync(): with %d out timestamp=%s\n", conn->chipId, timeStamp.c_str());


    if ( conn->time.num < TIME_SYNC_CYCLES ) {
        staticThis->sendMessage( conn, _chipId, TIME_SYNC, timeStamp );
    }

    uint8_t odd = conn->time.num % 2;

    if ( (conn->time.num + odd) >= TIME_SYNC_CYCLES ) {   // timeSync completed
        if ( conn->time.adopt ) {
            conn->time.calcAdjustment( odd );

            SimpleList<meshConnectionType>::iterator connection = _connections.begin();
            while ( connection != _connections.end() ) {
                if ( connection != conn ) {  // exclude this connection
                    connection->timeSyncStatus = NEEDED;
                }
                connection++;
            }
        }
        conn->lastTimeSync = getNodeTime();
        conn->timeSyncStatus = COMPLETE;
    }
}




