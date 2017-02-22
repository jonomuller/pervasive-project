#include "hid_leds.h"



LPM_MODULE(led_module, NULL, NULL, NULL, LPM_DOMAIN_PERIPH);
uint32_t led_pin = DEV_LED_IOID_WHITE;
int intensity = 100;


/*
 * LED PWM by nchronas
 */

void led_pwm_start(int freq, uint32_t ioid_pin) {

	/* Enable GPT0 clocks under active, sleep, deep sleep */
	ti_lib_prcm_peripheral_run_enable(PRCM_PERIPH_TIMER0);
	ti_lib_prcm_peripheral_sleep_enable(PRCM_PERIPH_TIMER0);
	ti_lib_prcm_peripheral_deep_sleep_enable(PRCM_PERIPH_TIMER0);
	ti_lib_prcm_load_set();
	while(!ti_lib_prcm_load_get());

	/* Drive the I/O ID with GPT0 / Timer A */
	ti_lib_ioc_port_configure_set(ioid_pin, IOC_PORT_MCU_PORT_EVENT0,
	                            IOC_STD_OUTPUT);

	/* GPT0 / Timer A: PWM, Interrupt Enable */
	HWREG(GPT0_BASE + GPT_O_TAMR) = (TIMER_CFG_A_PWM & 0xFF) | GPT_TAMR_TAPWMIE;

	lpm_register_module(&led_module);

	ti_lib_timer_disable(GPT0_BASE, TIMER_A);


    ti_lib_timer_load_set(GPT0_BASE, TIMER_A, 14000);
    ti_lib_timer_match_set(GPT0_BASE, TIMER_A, 10);

    /* Start */
    ti_lib_timer_enable(GPT0_BASE, TIMER_A);

}

void led_pwm_stop(uint32_t ioid_pin)
{

  /*
   * Unregister the buzzer module from LPM. This will effectively release our
   * lock for the PERIPH PD allowing it to be powered down (unless some other
   * module keeps it on)
   */
  lpm_unregister_module(&led_module);

  /* Stop the timer */
  ti_lib_timer_disable(GPT0_BASE, TIMER_A);

  /*
   * Stop the module clock:
   *
   * Currently GPT0 is in use by clock_delay_usec (GPT0/TB) and by this
   * module here (GPT0/TA).
   *
   * clock_delay_usec
   * - is definitely not running when we enter here and
   * - handles the module clock internally
   *
   * Thus, we can safely change the state of module clocks here.
   */
  ti_lib_prcm_peripheral_run_disable(PRCM_PERIPH_TIMER0);
  ti_lib_prcm_peripheral_sleep_disable(PRCM_PERIPH_TIMER0);
  ti_lib_prcm_peripheral_deep_sleep_disable(PRCM_PERIPH_TIMER0);
  ti_lib_prcm_load_set();
  while(!ti_lib_prcm_load_get());

  /* Un-configure the pin */
  ti_lib_ioc_pin_type_gpio_input(ioid_pin);
  ti_lib_ioc_io_input_set(ioid_pin, IOC_INPUT_DISABLE);
}

void led_pwm_update(int freq, uint32_t ioid_pin) {

    ti_lib_timer_match_set(GPT0_BASE, TIMER_A, freq);

}

/*
 * Access methods
 */

void hid_set_intensity(int percent)  {
  intensity = 12000/100 * percent;
  led_pwm_update(intensity, led_pin);
}


void hid_set_colour_white() {
  led_pwm_stop(led_pin);
  led_pin = DEV_LED_IOID_WHITE;
  led_pwm_start(intensity,led_pin);
}

void hid_set_colour_green() {
  led_pwm_stop(led_pin);
  led_pin = DEV_LED_IOID_GREEN;
  led_pwm_start(intensity,led_pin);
}

void hid_set_colour_blue() {
  led_pwm_stop(led_pin);
  led_pin = DEV_LED_IOID_BLUE;
  led_pwm_start(intensity,led_pin);
}

void hid_set_colour_red() {
  led_pwm_stop(led_pin);
  led_pin = DEV_LED_IOID_RED;
  led_pwm_start(intensity,led_pin);
}
