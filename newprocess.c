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

#include "ti-lib.h"

#include <stdio.h>
#include <stdint.h>
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


  static void
init_tmp_reading()
{
  SENSORS_ACTIVATE(bmp_280_sensor);
}


PROCESS(example_process, "Example process");
AUTOSTART_PROCESSES(&example_process);

PROCESS_THREAD(example_process, ev, data)
{
  static struct etimer et;
  PROCESS_BEGIN();
  init_tmp_reading();

  /* Delay 1 second */
  etimer_set(&et, CLOCK_SECOND);
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    int value;
		init_tmp_reading();
    /* Reset the etimer to trig again in 1 second */
    etimer_reset(&et);
    leds_toggle(LEDS_RED);
		value = bmp_280_sensor.value(BMP_280_SENSOR_TYPE_TEMP);

		printf("BAR: Temp=%d.%02d C\n", value / 100, value % 100);

		SENSORS_DEACTIVATE(bmp_280_sensor);


    /* ... */
  }
  PROCESS_END();
}
