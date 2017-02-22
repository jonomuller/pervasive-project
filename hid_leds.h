/*
 * Provides an easy way of enabling and disabling the LEDS on the
 * TI LED-AUDIO DEVPACK
 */

#ifndef  HID_LEDS_H_
#define  HID_LEDS_H_

/*
 * Module Inports
 */

#include "contiki.h"
#include "dev/watchdog.h"
#include "board-peripherals.h"
#include "cc26xx-uart.h"
#include "sys/mt.h"
#include "lpm.h"
#include "ti-lib.h"

/*
 * LED PINS
 */
#define DEV_LED_IOID_WHITE				BOARD_IOID_DP0
#define DEV_LED_IOID_GREEN 				BOARD_IOID_DP1
#define DEV_LED_IOID_BLUE 				BOARD_IOID_DP2
#define DEV_LED_IOID_RED 				BOARD_IOID_DP3

#define DEV_LED_WHITE					(1 << DEV_LED_IOID_WHITE)
#define DEV_LED_GREEN 					(1 << DEV_LED_IOID_GREEN)
#define DEV_LED_BLUE 					(1 << DEV_LED_IOID_BLUE)
#define DEV_LED_RED 					(1 << DEV_LED_IOID_RED)


/*
 * Method declarations
 */

void hid_on();
void hid_off();

void hid_set_colour_white();
void hid_set_colour_green();
void hid_set_colour_blue();
void hid_set_colour_red();

void hid_set_intensity(int percent);
#endif
