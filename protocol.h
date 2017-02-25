#define COLOUR_CODE_WHITE 0
#define COLOUR_CODE_RED 1
#define COLOUR_CODE_GREEN 2
#define COLOUR_CODE_BLUE 3

#define ON_PACKET 100







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
  bool light_on;
  int light_colour;
  int light_intensity;
} watch_to_light_packet;

typedef struct {
  data_packet_header header;
  watch_to_light_packet data;
} data_packet;



