#include "contiki.h"
#include "net/rime/rime.h"
#include "random.h"

#include "dev/button-sensor.h"

#include "dev/leds.h"

#include <stdio.h>
/*---------------------------------------------------------------------------*/
PROCESS(rssi_test_receive_process, "Broadcast example");
AUTOSTART_PROCESSES(&rssi_test_receive_process);
/*---------------------------------------------------------------------------*/
static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  int rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI) - 65536;
  printf("RSSI: %d\n", rssi);
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(rssi_test_receive_process, ev, data)
{
  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)

  PROCESS_BEGIN();

  broadcast_open(&broadcast, 137, &broadcast_call);

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
