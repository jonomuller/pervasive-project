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
  header.packet_type = LIGHT_SETTINGS_PACKET;
  light_time_packet packet;
  packet.timestamp = timestamp;
  packet.rssi = rssi;
  // memory stuff might not work here
  packetbuf_copyfrom(&packet, sizeof(light_time_packet));
  broadcast_send(&broadcast);
}


/*---------------------------------------------------------------------------*/

static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  // Lets determine the type of packet received
  data_packet_header data_header;
  memcpy(&data_header,packetbuf_dataptr(),sizeof(data_packet_header));
  if (data_header.system_code == SYSTEM_CODE)  {
    printf("received light system packet \n");
    switch (data_header.packet_type)  {
      case LIGHT_SETTINGS_PACKET:
        printf("received new light settings \n");
        memcpy(&settings, packetbuf_dataptr()+sizeof(data_packet_header)
            , sizeof(light_settings_packet));
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
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
/*---------------------------------------------------------------------------*/

static struct broadcast_conn broadcast;

PROCESS(example_process, "Example process");
AUTOSTART_PROCESSES(&example_process);

PROCESS_THREAD(example_process, ev, data)
{

  PROCESS_EXITHANDLER(broadcast_close(&broadcast));

  PROCESS_BEGIN();

  broadcast_open(&broadcast, 135, &broadcast_call);
  PROCESS_END();
}
