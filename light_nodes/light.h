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
#include "lib/list.h"
#include "lib/random.h"

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
#define NODE_CACHE_SIZE 20
#define DECISION_WINDOW CLOCK_SECOND * 2
#define NEGOTIATION_WINDOW CLOCK_SECOND * 4
#define NUM_OF_CALC_REBROADCASTS 3
#define NUM_OF_ANNOUNCE_REBROADCASTS 2
#define INTERNODE_TTL 2




static struct broadcast_conn broadcast_internode;
static struct etimer randt;
static struct etimer et3;
clock_time_t t_rand;

bool light_on = false;
int light_colour = COLOUR_CODE_WHITE;
int light_intensity = 100;
light_settings_packet settings;

float rssi_from_watch = -INFINITY;
int watch_timestamp = 0;
int old_send_time = -1;

int last_ack = -1;
int current_ack = -1;
float current_rssi = -1000.0;
light_time_packet rssis[NODE_CACHE_SIZE];
announce_packet announcers[NODE_CACHE_SIZE];
int curr_element_len = 0;
int curr_announce_elem_len = 0;

static struct mmem mmem;
char * packet_ptr_calc;
void * void_ptr_calc;
int loop_counter;