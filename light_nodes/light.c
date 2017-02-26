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
/*---------------------------------------------------------------------------*/
static struct broadcast_conn broadcast;
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
