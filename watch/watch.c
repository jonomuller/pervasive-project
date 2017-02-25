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
static const struct broadcast_callbacks broadcast_call;
/*---------------------------------------------------------------------------*/

static struct broadcast_conn broadcast;

PROCESS(example_process, "Example process");
AUTOSTART_PROCESSES(&example_process);

bool switch_pos = false;
PROCESS_THREAD(example_process, ev, data)
{

  PROCESS_EXITHANDLER(broadcast_close(&broadcast));

  PROCESS_BEGIN();
  broadcast_open(&broadcast, 135, &broadcast_call);
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event);
    if (data == CC26XX_DEMO_SENSOR_1)  {
      packetbuf_clear();
      switch_pos = !switch_pos;
      data_packet_header header;
      header.system_code = 123456;
      header.source_node_type = 0;
      header.packet_type = ON_PACKET;
      watch_to_light_packet packet;
      packet.light_on = switch_pos;
      packet.light_colour = 1;
      packet.light_intensity = 100;
      data_packet data_to_send = { header, packet};
      packetbuf_copyfrom(&data_to_send,sizeof(data_packet));
      broadcast_send(&broadcast);
      switch_pos? printf("on signal sent"): printf("off signal sent");
    }
  }
  PROCESS_END();
}
