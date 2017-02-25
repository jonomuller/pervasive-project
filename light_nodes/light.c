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
#include "net/rime/timesynch.h"
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

#define SPEED_OF_LIGHT 299792458
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static int authority_level;
static rtimer_clock_t offset;

static void
adjust_offset(rtimer_clock_t authoritative_time, rtimer_clock_t local_time)
{
  offset = authoritative_time - local_time;
}


// reverses a string 'str' of length 'len'
void reverse(char *str, int len)
{
    int i=0, j=len-1, temp;
    while (i<j)
    {
        temp = str[i];
        str[i] = str[j];
        str[j] = temp;
        i++; j--;
    }
}
 
 // Converts a given integer x to string str[].  d is the number
 // of digits required in output. If d is more than the number
 // of digits in x, then 0s are added at the beginning.
int intToStr(int x, char str[], int d)
{
    int i = 0;
    while (x)
    {
        str[i++] = (x%10) + '0';
        x = x/10;
    }
 
    // If number of digits required is more, then
    // add 0s at the beginning
    while (i < d)
        str[i++] = '0';
 
    reverse(str, i);
    str[i] = '\0';
    return i;
}

// Converts a floating point number to string.
void ftoa(float n, char *res, int afterpoint)
{
    // Extract integer part
    int ipart = (int)n;
 
    // Extract floating part
    float fpart = n - (float)ipart;
 
    // convert integer part to string
    int i = intToStr(ipart, res, 0);
 
    // check for display option after point
    if (afterpoint != 0)
    {
        res[i] = '.';  // add dot
 
        // Get the value of fraction part upto given no.
        // of points after dot. The third parameter is needed
        // to handle cases like 233.007
        fpart = fpart * pow(10, afterpoint);
 
        intToStr((int)fpart, res + i + 1, afterpoint);
    }
}

/*---------------------------------------------------------------------------*/
static struct broadcast_conn broadcast;
static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  data_packet_header packet;

  memcpy(&packet, packetbuf_dataptr(), sizeof(packet));

  // clock sync method
  if(packet.authority_level < authority_level) {
  adjust_offset(packet.timestamp + packet.authority_offset,
                packetbuf_attr(PACKETBUF_ATTR_TIMESTAMP));
  timesynch_set_authority_level(packet.authority_level + 1);
  }

  int start_time = packet.timestamp;
  int end_time = (int) timesynch_time();
  int difference = end_time - start_time; // need to convert to seconds
  int distance = SPEED_OF_LIGHT * difference;

  // printf("start: %d, end: %d, difference: %d\n", start_time, end_time, difference);
  // leds_toggle(LEDS_RED);
  // printf("distance = %d\n", distance);


  // rssi method
  float rssi = (float) packetbuf_attr(PACKETBUF_ATTR_RSSI) - 65536;
  float strength_1m = -50.0;
  float n = 2.0;
  float d = pow(10.0, ((rssi - strength_1m) / (-10.0 * n)));
  
  char rssi_string[20];
  ftoa(rssi, rssi_string, 2);
  char d_string[20];
  ftoa(d, d_string, 2);

  printf("rssi = %s, d = %s\n", rssi_string, d_string);
}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
/*---------------------------------------------------------------------------*/

static struct broadcast_conn broadcast;

PROCESS(example_process, "Example process");
AUTOSTART_PROCESSES(&example_process);

PROCESS_THREAD(example_process, ev, data)
{
  static struct etimer et;
  data_packet_header packet;
  timesynch_init();
  authority_level = 0;

  PROCESS_EXITHANDLER(broadcast_close(&broadcast));

  PROCESS_BEGIN();

  broadcast_open(&broadcast, 135, &broadcast_call);
  hid_on();
  while(1) {
    etimer_set(&et, CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    leds_toggle(LEDS_ALL);

    // packet.timestamp = timesynch_time();
    packet.authority_offset = offset;
    packet.authority_level = authority_level;
    packet.timestamp = timesynch_time();

    packetbuf_copyfrom(&packet, sizeof(packet));
    packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE,
                       PACKETBUF_ATTR_PACKET_TYPE_TIMESTAMP);
    broadcast_send(&broadcast);

    // printf("broadcast message sent\n");
    leds_toggle(LEDS_ALL);

    }
  PROCESS_END();
}
