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
#include "../protocol.h"
#include "lib/mmem.h"
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
#define DEBUG DEBUG_FULL


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static const struct broadcast_callbacks broadcast_call;
/*---------------------------------------------------------------------------*/

static struct broadcast_conn broadcast;

PROCESS(watch_button_process, "Watch process");
AUTOSTART_PROCESSES(&watch_button_process);

bool switch_pos = false;
int current_colour = COLOUR_CODE_WHITE;
int current_intensity = 100;
PROCESS_THREAD(watch_button_process, ev, data)
{
  static struct mmem mmem;
  PROCESS_EXITHANDLER(broadcast_close(&broadcast));
  PROCESS_BEGIN();
  broadcast_open(&broadcast, WATCH_BROADCAST_CHANNEL, &broadcast_call);
  mmem_init();
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event);
    if (data == CC26XX_DEMO_SENSOR_1)  {
      // Light switch button press
      packetbuf_clear();
      switch_pos = !switch_pos;
      data_packet_header header;
      header.system_code = SYSTEM_CODE;
      header.source_node_type = 0;
      header.packet_type = switch_pos? ON_PACKET: OFF_PACKET;
      packetbuf_copyfrom(&header,sizeof(data_packet_header));
      broadcast_send(&broadcast);
    } else if (data == CC26XX_DEMO_SENSOR_2)  {
      // Switch colour press
      if (current_colour < COLOUR_CODE_BLUE) {
        current_colour++;
      } else current_colour = COLOUR_CODE_WHITE;
      packetbuf_clear();
      switch_pos = !switch_pos;
      data_packet_header header;
      header.system_code = SYSTEM_CODE;
      header.source_node_type = 0;
      header.packet_type = LIGHT_SETTINGS_PACKET;
      light_settings_packet settings;
      settings.light_colour = current_colour;
      settings.light_intensity = current_intensity;

      int packet_size = sizeof(data_packet_header)
          + sizeof(light_settings_packet);
      if(mmem_alloc(&mmem, packet_size) == 0) {
        printf("memory allocation failed\n");
      } else {
        char * packet = (char *) MMEM_PTR(&mmem);
        memcpy(packet,&header,sizeof(data_packet_header));
        memcpy(packet+sizeof(data_packet_header),&settings,
            sizeof(light_settings_packet));
        void * void_ptr = (void *) packet;
        packetbuf_copyfrom(void_ptr,packet_size);
        broadcast_send(&broadcast);
      }
    }

  }
  PROCESS_END();
}

PROCESS_THREAD(watch_announce_process, ev, data)
{
  static struct mmem mmem;
  PROCESS_EXITHANDLER(broadcast_close(&broadcast));
  PROCESS_BEGIN();
  broadcast_open(&broadcast, BROADCAST_CHANNEL, &broadcast_call);
  mmem_init();
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event);
    if (data == CC26XX_DEMO_SENSOR_1)  {
      // Light switch button press
      packetbuf_clear();
      switch_pos = !switch_pos;
      data_packet_header header;
      header.system_code = SYSTEM_CODE;
      header.source_node_type = 0;
      header.packet_type = switch_pos? ON_PACKET: OFF_PACKET;
      packetbuf_copyfrom(&header,sizeof(data_packet_header));
      broadcast_send(&broadcast);
    } else if (data == CC26XX_DEMO_SENSOR_2)  {
      // Switch colour press
      if (current_colour < COLOUR_CODE_BLUE) {
        current_colour++;
      } else current_colour = COLOUR_CODE_WHITE;
      packetbuf_clear();
      switch_pos = !switch_pos;
      data_packet_header header;
      header.system_code = SYSTEM_CODE;
      header.source_node_type = 0;
      header.packet_type = LIGHT_SETTINGS_PACKET;
      light_settings_packet settings;
      settings.light_colour = current_colour;
      settings.light_intensity = current_intensity;

      int packet_size = sizeof(data_packet_header)
          + sizeof(light_settings_packet);
      if(mmem_alloc(&mmem, packet_size) == 0) {
        printf("memory allocation failed\n");
      } else {
        char * packet = (char *) MMEM_PTR(&mmem);
        memcpy(packet,&header,sizeof(data_packet_header));
        memcpy(packet+sizeof(data_packet_header),&settings,
            sizeof(light_settings_packet));
        void * void_ptr = (void *) packet;
        packetbuf_copyfrom(void_ptr,packet_size);
        broadcast_send(&broadcast);
      }
    }

  }
  PROCESS_END();
}
