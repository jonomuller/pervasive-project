/*
 * Structs for sending messages between nodes
 *
 */

struct {
  int system_code;
  int source_node_type;
  int packet_type;
} data_packet_header;

struct {
  bool light_on;
  int light_colour;
  int light_intensity;
} watch_to_light_packet;




