#include "contiki.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "dev/leds.h"
#include "dev/watchdog.h"
#include "random.h"
#include "button-sensor.h"
#include "batmon-sensor.h"
#include "board-peripherals.h"
#include "rf-core/rf-ble.h"
#include "packetbuf.h"
#include "ti-lib.h"
#include "net/rime/rime.h"
#include <stdio.h>
#include <stdint.h>
#include "hid_leds.h"
#include "../protocol.h"
#include <math.h>
#include "lib/mmem.h"
#include "clock.h"

/*---------------------------------------------------------------------------*/
#define CC26XX_DEMO_LOOP_INTERVAL       (CLOCK_SECOND * 20)
#define CC26XX_DEMO_LEDS_PERIODIC       LEDS_YELLOW
#define CC26XX_DEMO_LEDS_BUTTON         LEDS_RED
#define CC26XX_DEMO_LEDS_REBOOT         LEDS_ALL
/*---------------------------------------------------------------------------*/
#define CC26XX_DEMO_SENSOR_NONE         (void *)0xFFFFFFFF

#define CC26XX_DEMO_SENSOR_1     &button_left_sensor
#define CC26XX_DEMO_SENSOR_2     &button_right_sensor

#if BOARD_SENSORTAG
#define CC26XX_DEMO_SENSOR_3     CC26XX_DEMO_SENSOR_NONE
#define CC26XX_DEMO_SENSOR_4     CC26XX_DEMO_SENSOR_NONE
#define CC26XX_DEMO_SENSOR_5     &reed_relay_sensor
#elif BOARD_LAUNCHPAD
#define CC26XX_DEMO_SENSOR_3     CC26XX_DEMO_SENSOR_NONE
#define CC26XX_DEMO_SENSOR_4     CC26XX_DEMO_SENSOR_NONE
#define CC26XX_DEMO_SENSOR_5     CC26XX_DEMO_SENSOR_NONE
#else
#define CC26XX_DEMO_SENSOR_3     &button_up_sensor
#define CC26XX_DEMO_SENSOR_4     &button_down_sensor
#define CC26XX_DEMO_SENSOR_5     &button_select_sensor
#endif
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
// Default light settings


#define NUM_OTHER_LIGHTS 2

bool light_on = false;
int light_colour = COLOUR_CODE_WHITE;
int light_intensity = 100;
light_settings_packet settings;
light_time_packet node_packet;

float rssi_from_watch = -INFINITY;
int watch_timestamp = 0;
float rssis[NUM_OTHER_LIGHTS];
int rssi_count = 0;
int old_send_time = -1;

static struct broadcast_conn broadcast_internode;
void
broadcast_time_packet(int timestamp, float rssi)
{
  static struct mmem mmem;
  mmem_init();
  data_packet_header header;
  header.system_code = SYSTEM_CODE;
  header.source_node_type = 1;
  header.packet_type = INTER_NODE_PACKET;
  light_time_packet packet;
  packet.timestamp = timestamp;
  packet.rssi = rssi;
  header.ttl = 1;
  printf("SENT watch RSSI %d \n",(int)packet.rssi);
  header.ack_no = clock_time();
  int packet_size = sizeof(data_packet_header)
          + sizeof(light_time_packet);
  if(mmem_alloc(&mmem, packet_size) == 0) {
    printf("memory allocation failed\n");
  } else {
    char * packet = (char *) MMEM_PTR(&mmem);
    memcpy(packet,&header,sizeof(data_packet_header));
    memcpy(packet+sizeof(data_packet_header),&settings,
        sizeof(light_time_packet));
    void * void_ptr = (void *) packet;
    packetbuf_copyfrom(void_ptr,packet_size);
    broadcast_send(&broadcast_internode);
  }
}

void retransmit_settings(void * packet, int size,
    struct broadcast_conn* broadcast)  {
    data_packet_header *header = (data_packet_header *) packet;

  if (header->ttl > 0)  {
    header->ttl -= 1;

    printf("retrasmitting packet %i %i \n",header->ack_no,header->ttl);
    packetbuf_copyfrom(packet,size);

    broadcast_send(broadcast);
  }

}

/*---------------------------------------------------------------------------*/
int last_ack = -1;
static void watch_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  // Lets determine the type of packet received
  data_packet_header data_header;
  memcpy(&data_header,packetbuf_dataptr(),sizeof(data_packet_header));
  if (data_header.system_code == SYSTEM_CODE)  {
    if (data_header.ack_no< last_ack && data_header.ack_no - last_ack > 1000)  {
      last_ack = data_header.ack_no;
    }
    if (data_header.ack_no >= last_ack) {
      last_ack = data_header.ack_no;
      printf("received light system packet \n");
      switch (data_header.packet_type)  {
        case LIGHT_SETTINGS_PACKET:
          printf("received new light settings \n");
          char * data_pt = (char*) packetbuf_dataptr();
          memcpy(&settings,data_pt+sizeof(data_packet_header)
              ,sizeof(light_settings_packet));
          if (light_on) hid_off();
          printf("received header %i %i\n",data_header.ack_no,data_header.ttl);
          light_colour = settings.light_colour;
          light_intensity = settings.light_intensity;
          retransmit_settings(packetbuf_dataptr(),sizeof(data_packet_header)
              + sizeof(light_settings_packet),c);
          if (!light_on) break;
        case ON_PACKET:
          hid_on();
          printf("Received On command \n");
          if (light_colour == COLOUR_CODE_WHITE)  {
            hid_set_colour_white();
          }
          if (light_colour == COLOUR_CODE_RED)  {
            hid_set_colour_red();
          }
          if (light_colour == COLOUR_CODE_BLUE)  {
            hid_set_colour_blue();
          }
          if (light_colour == COLOUR_CODE_GREEN)  {
            hid_set_colour_green();
          }
          hid_set_intensity(light_intensity);
          light_on = true;
          retransmit_settings(packetbuf_dataptr(),sizeof(data_packet_header)
              + sizeof(light_settings_packet),c);
          break;
        case OFF_PACKET:
          hid_off();
          light_on = false;
          printf("Received Off command\n");
          retransmit_settings(packetbuf_dataptr(),sizeof(data_packet_header),c);
          break;
        case WATCH_ANNOUNCE_PACKET:
          //rssi_from_watch = (float) packetbuf_attr(PACKETBUF_ATTR_RSSI) - 65536.0;
          rssi_from_watch = packetbuf_attr(PACKETBUF_ATTR_RSSI) - 65536.0;
          broadcast_time_packet(data_header.ack_no, rssi_from_watch);
        default:
          break;
      }
    }
  }
}

/*---------------------------------------------------------------------------*/
static void internode_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  // Lets determine the type of packet received
  data_packet_header data_header;
  memcpy(&data_header,packetbuf_dataptr(),sizeof(data_packet_header));
  if (data_header.system_code == SYSTEM_CODE)  {
    printf("received internode \n");
    switch (data_header.packet_type)  {
      case INTER_NODE_PACKET:
        memcpy(&node_packet, packetbuf_dataptr()+sizeof(data_packet_header),
          sizeof(light_time_packet));

        float rssi = node_packet.rssi;
        rssis[rssi_count] = rssi;
        rssi_count++;
        bool closest = true;

        // check if array of other nodes is full
        if (rssi_count == NUM_OTHER_LIGHTS) {
          for (int i = 0; i < rssi_count; i++) {
            if (rssis[i] > rssi_from_watch) {
              closest = false;
            }
          }

          // if node is closest to watch, turn on light
          if (closest) {
            hid_on();
          }
        }

      default:
        break;
    }
  }
}
/*---------------------------------------------------------------------------*/
static const struct broadcast_callbacks watch_callbacks = {watch_recv};
static const struct broadcast_callbacks internode_callbacks = {internode_recv};
/*---------------------------------------------------------------------------*/

static struct broadcast_conn broadcast_watch;

PROCESS(watch_listening_process, "Watch packet listening process");
PROCESS(internode_process, "Internode communication process");
AUTOSTART_PROCESSES(&watch_listening_process, &internode_process);
//AUTOSTART_PROCESSES(&watch_listening_process);
PROCESS_THREAD(watch_listening_process, ev, data)
{

  PROCESS_EXITHANDLER(broadcast_close(&broadcast_watch));

  PROCESS_BEGIN();
  broadcast_open(&broadcast_watch, WATCH_BROADCAST_CHANNEL, &watch_callbacks);
  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(internode_process, ev, data)
{
  PROCESS_EXITHANDLER(broadcast_close(&broadcast_internode));

  PROCESS_BEGIN();
  clock_init();
  broadcast_open(&broadcast_internode, INTER_NODE_CHANNEL, &internode_callbacks);
  PROCESS_END();
}

