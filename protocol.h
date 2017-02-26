#define SYSTEM_CODE 123456
#define BROADCAST_CHANNEL 135

#define NODE_TYPE_WATCH 0
#define NODE_TYPE_LIGHT 1

#define COLOUR_CODE_WHITE 0
#define COLOUR_CODE_RED 1
#define COLOUR_CODE_GREEN 2
#define COLOUR_CODE_BLUE 3

#define ON_PACKET 100
#define OFF_PACKET 200
#define LIGHT_SETTINGS_PACKET 300
#define WATCH_ANNOUNCE_PACKET 400
#define INTER_NODE_PACKET 500







/*
 * Structs for sending messages between nodes
 *
 */

typedef struct {
  int system_code;
  int source_node_type;
  int packet_type;
} data_packet_header;

typedef struct {
  int light_colour;
  int light_intensity;
} light_settings_packet;

typedef struct {
  int packet_type;
  int timestamp;
} light_time_packet;




