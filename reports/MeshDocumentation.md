#Intro to easyMesh
easyMesh is a library that takes care of the particulars for creating a simple mesh network using Arduino and esp8266.  The goal is to allow the programmer to work with a mesh network without having to worry about how the network is structured or managed.  

###Dependencies
easyMesh makes use of the following libraries.  They can both be installed through Arduino Library Manager
- SimpleList *** Available here... https://github.com/blackhack/ArduLibraries/tree/master/SimpleList
- ArduinoJson *** Available here... https://github.com/bblanchon/ArduinoJson

#easyMesh API
Using easyMesh is easy!

First include the library and create an easyMesh object like this…

```
#include <easyMesh.h>
easyMesh  mesh;
```

##Member Functions

###void easyMesh::init( String prefix, String password, uint16_t port )
Add this to your setup() function.
Initialize the mesh network.  This routine does the following things…
- Starts a wifi network
- Begins searching for other wifi networks that are part of the mesh
- Logs on to the best mesh network node it finds… if it doesn’t find anything, it starts a new search in 5 seconds.

prefix = the name of your mesh.  The wifi ssid of this node will be prefix + chipId
password = wifi password to your mesh
port = the TCP port that you want the mesh server to run on

###void easyMesh::update( void )
Add this to your loop() function
This routine runs various maintainance tasks... Not super interesting, but things don't work without it.


###void easyMesh::setReceiveCallback( &receivedCallback )
Set a callback routine for any messages that are addressed to this node.  The callback routine has the following structure…

`void receivedCallback( uint32_t from, String &msg )`

Every time this node receives a message, this callback routine will the called.  “from” is the id of the original sender of the message, and “msg” is a string that contains the message.  The message can be anything.  A JSON, some other text string, or binary data.


###void easyMesh::setNewConnectionCallback( &newConnectionCallback )
This fires every time the local node makes a new connection.   The callback has the following structure…

`void newConnectionCallback( bool adopt )`

`adopt` is a boolean value that indicates whether the mesh has determined to adopt the remote nodes timebase or not.  If `adopt == true`, then this node has adopted the remote node’s timebase.

The mesh does a simple calculation to determine which nodes adopt and which nodes don’t.  When a connection is made, the node with the smaller number of connections to other nodes adopts the timebase of the node with the larger number of connections to other nodes.  If there is a tie, then the AP (access point) node wins.

######Example 1:
There are two separate meshes (Mesh A and Mesh B) that have discovered each other and are connecting.  Mesh A has 7 nodes and Mesh B has 8 nodes.  When the connection is made, Mesh B has more nodes in it, so Mesh A adopts the timebase of Mesh B.

######Example 2:
A brand new mesh is starting.  There are only 2 nodes (Node X and Node Y) and they both just got turned on.  They find each other, and as luck would have it, Node X connects as a Station to the wifi network established by Node Y’s AP (access point)… which means that Node X is the wifi client and Node Y is the wifi server in the particular relationship.  In this case, since both nodes have zero (0) other connections, Node X adopts Node Y’s timebase because the tie (0 vs 0) goes to the AP. 

###bool easyMesh::sendBroadcast( String &msg)
Sends msg to every node on the entire mesh network.

returns true if everything works, false if not.  Prints an error message to Serial.print, if there is a failure.

###bool easyMesh::sendSingle(uint32_t dest, String &msg)
Sends msg to the node with Id == dest.

returns true if everything works, false if not.  Prints an error message to Serial.print, if there is a failure.

###uint16_t easyMesh::connectionCount()
Returns the total number of nodes connected to this mesh.

###uint32_t easyMesh::getChipId( void )
Return the chipId of the node that we are running on.

###uint32_t easyMesh::getNodeTime( void )
Returns the mesh timebase microsecond counter.  Rolls over 71 minutes from startup of the first node.
