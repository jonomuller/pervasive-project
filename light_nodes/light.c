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

bool light_on = false;
int light_colour = COLOUR_CODE_WHITE;
int light_intensity = 100;
light_settings_packet settings;
light_time_packet node_packet;

float rssi_from_watch = -INFINITY;
int watch_timestamp = 0;
float rssis[2];
int rssi_count = 0;

static struct broadcast_conn broadcast;

void
broadcast_time_packet(int timestamp, float rssi)
{
  data_packet_header header;
  header.system_code = SYSTEM_CODE;
  header.source_node_type = 1;
  header.packet_type = INTER_NODE_PACKET;
  light_time_packet packet;
  packet.timestamp = timestamp;
  packet.rssi = rssi;
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
    broadcast_send(&broadcast);
  }
}


/*---------------------------------------------------------------------------*/

static void watch_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  // Lets determine the type of packet received
  data_packet_header data_header;
  memcpy(&data_header,packetbuf_dataptr(),sizeof(data_packet_header));
  if (data_header.system_code == SYSTEM_CODE)  {
    printf("received light system packet \n");
    switch (data_header.packet_type)  {
      case LIGHT_SETTINGS_PACKET:
        printf("received new light settings \n");
        char * data_pt = (char*) packetbuf_dataptr();
        memcpy(&settings,data_pt+sizeof(data_packet_header)
            ,sizeof(light_settings_packet));
        if (light_on) hid_off();
        light_colour = settings.light_colour;
        light_intensity = settings.light_intensity;
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
        break;
      case OFF_PACKET:
        hid_off();
        light_on = false;
        printf("Received Off command\n");
        break;
      case WATCH_ANNOUNCE_PACKET:
        watch_timestamp = packetbuf_attr(PACKETBUF_ATTR_TIMESTAMP);
        rssi_from_watch = (float) packetbuf_attr(PACKETBUF_ATTR_RSSI) - 65536;
        clock_wait(0.5);
        broadcast_time_packet(watch_timestamp, rssi_from_watch);
      default:
        break;
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
    printf("received light system packet \n");
    switch (data_header.packet_type)  {
      case INTER_NODE_PACKET:
        memcpy(&node_packet, packetbuf_dataptr()+sizeof(data_packet_header),
          sizeof(light_time_packet));

        float rssi = node_packet.rssi;
        rssis[rssi_count] = rssi;
        rssi_count++;
        bool closest = true;

        // check if array of other nodes is full
        if (rssi_count == 2) {
          for (int i = 0; i < rssi_count; i++) {
            if (rssis[i] > rssi_from_watch) {
              closest = false;
            }
          }
        }

        // if node is closest to watch, turn on light
        if (closest) {
          hid_on();
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

static struct broadcast_conn broadcast;

PROCESS(watch_listening_process, "Watch packet listening process");
PROCESS(internode_process, "Internode communication process");
AUTOSTART_PROCESSES(&watch_listening_process, &internode_process);

PROCESS_THREAD(watch_listening_process, ev, data)
{

  PROCESS_EXITHANDLER(broadcast_close(&broadcast));

  PROCESS_BEGIN();

  broadcast_open(&broadcast, WATCH_BROADCAST_CHANNEL, &watch_callbacks);
  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(internode_process, ev, data)
{
  PROCESS_EXITHANDLER(broadcast_close(&broadcast));

  PROCESS_BEGIN();

  broadcast_open(&broadcast, INTER_NODE_CHANNEL, &internode_callbacks);
  PROCESS_END();
}
