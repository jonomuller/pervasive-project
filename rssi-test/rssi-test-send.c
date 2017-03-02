#include "contiki.h"
#include "net/rime/rime.h"
#include "random.h"

#include "dev/button-sensor.h"

#include "dev/leds.h"

#include <stdio.h>
/*---------------------------------------------------------------------------*/
PROCESS(rssi_test_send_process, "Broadcast example");
AUTOSTART_PROCESSES(&rssi_test_send_process);
/*---------------------------------------------------------------------------*/
static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  int rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI) - 65536;
  printf("RSSI: %d\n", rssi);
}

static struct broadcast_conn broadcast;
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(rssi_test_send_process, ev, data)
{
  static struct etimer et;

  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)

  PROCESS_BEGIN();

  broadcast_open(&broadcast, 137, &broadcast_call);

  while(1) {
    etimer_set(&et, CLOCK_SECOND * 2);

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    packetbuf_copyfrom("Hello", 6);
    broadcast_send(&broadcast);
    printf("broadcast message sent\n");
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
