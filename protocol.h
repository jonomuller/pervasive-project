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




