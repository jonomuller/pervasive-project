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
bool light_on = false;
int light_colour = COLOUR_CODE_WHITE;
int light_intensity = 100;
light_settings_packet settings;

float rssi_from_watch = -INFINITY;
int watch_timestamp = 0;
int old_send_time = -1;

PROCESS(calculation_process, "Watch Calculation process");

static struct broadcast_conn broadcast_internode;
struct rssi_element {
  linkaddr_t node_id;
  float rssi;
};


void
broadcast_time_packet(int timestamp, float rssi)
{
  static struct mmem mmem;
  mmem_init();
  data_packet_header header;
  header.system_code = SYSTEM_CODE;
  header.source_node_type = 1;
  header.packet_type = INTER_NODE_PACKET;
  light_time_packet packet;
  packet.timestamp = timestamp;
  packet.rssi = rssi;
  header.ttl = 1;
  printf("SENT watch RSSI %d \n",(int)packet.rssi);
  header.ack_no = clock_time();
  int packet_size = sizeof(data_packet_header)
          + sizeof(light_time_packet);
  if(mmem_alloc(&mmem, packet_size) == 0) {
    printf("memory allocation failed\n");
  } else {
    char * packet_ptr = (char *) MMEM_PTR(&mmem);
    memcpy(packet_ptr,&header,sizeof(data_packet_header));
    memcpy(packet_ptr+sizeof(data_packet_header),&packet,
        sizeof(light_time_packet));
    void * void_ptr = (void *) packet_ptr;
    packetbuf_copyfrom(void_ptr,packet_size);
    broadcast_send(&broadcast_internode);
    mmem_free(&mmem);
  }
}

void retransmit_settings(void * packet, int size,
    struct broadcast_conn* broadcast)  {
    data_packet_header *header = (data_packet_header *) packet;

  if (header->ttl > 0)  {
    header->ttl -= 1;

    printf("retrasmitting packet %i %i \n",header->ack_no,header->ttl);
    packetbuf_copyfrom(packet,size);

    broadcast_send(broadcast);
  }

}

/*---------------------------------------------------------------------------*/
int last_ack = -1;
int current_ack = -1;
float current_rssi = -1000.0;
struct rssi_element rssis[NODE_CACHE_SIZE];
int curr_element_len = 0;
void clear_element_array()  {
  //for (int i = 0; i < NODE_CACHE_SIZE ; i++)  {
  //  rssis[i] = NULL;
  // }
  curr_element_len = 0;
}

static struct etimer et;
bool clock_started = false;
static void watch_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  // Lets determine the type of packet received
  data_packet_header data_header;
  memcpy(&data_header,packetbuf_dataptr(),sizeof(data_packet_header));
  if (data_header.system_code == SYSTEM_CODE)  {
    if (data_header.ack_no< last_ack && data_header.ack_no - last_ack > 1000)  {
      last_ack = data_header.ack_no;
    }
    if (data_header.ack_no >= last_ack) {
      last_ack = data_header.ack_no;
      printf("received light system packet \n");
      switch (data_header.packet_type)  {
        case LIGHT_SETTINGS_PACKET:
          printf("received new light settings \n");
          char * data_pt = (char*) packetbuf_dataptr();
          memcpy(&settings,data_pt+sizeof(data_packet_header)
              ,sizeof(light_settings_packet));
          //if (light_on) hid_off();
          printf("received header %i %i\n",data_header.ack_no,data_header.ttl);
          light_colour = settings.light_colour;
          light_intensity = settings.light_intensity;
          retransmit_settings(packetbuf_dataptr(),sizeof(data_packet_header)
              + sizeof(light_settings_packet),c);
          if (!light_on) break;
        case ON_PACKET:
          //hid_set_intensity(light_intensity);
          light_on = true;
          retransmit_settings(packetbuf_dataptr(),sizeof(data_packet_header)
              + sizeof(light_settings_packet),c);
          break;
        case OFF_PACKET:
          //hid_off();
          light_on = false;
          printf("Received Off command\n");
          retransmit_settings(packetbuf_dataptr(),sizeof(data_packet_header),c);
          break;
        case WATCH_ANNOUNCE_PACKET:
          //rssi_from_watch = (float) packetbuf_attr(PACKETBUF_ATTR_RSSI) - 65536.0;
          rssi_from_watch = packetbuf_attr(PACKETBUF_ATTR_RSSI) - 65536.0;
          if (data_header.ack_no > current_ack)  {
            current_ack = data_header.ack_no;
            current_rssi = rssi_from_watch;
            process_exit(&calculation_process);
            process_start(&calculation_process,NULL);
            clear_element_array();
            broadcast_time_packet(data_header.ack_no, rssi_from_watch);
          } else {
            printf("ignored repeated watch announce packet \n");
          }
        default:
          break;
      }
    }
  }
}

PROCESS(internode_process, "Internode communication process");
/*---------------------------------------------------------------------------*/
static void internode_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  // Lets determine the type of packet received
  data_packet_header data_header;
  light_time_packet data_time;
  memcpy(&data_header,packetbuf_dataptr(),sizeof(data_packet_header));
  char * char_ptr;
  if (data_header.system_code == SYSTEM_CODE)  {
    printf("received internode \n");
    switch (data_header.packet_type)  {
      case INTER_NODE_PACKET:
        char_ptr = (char *) packetbuf_dataptr();
        memcpy(&data_time, char_ptr+sizeof(data_packet_header),
          sizeof(light_time_packet));
        struct rssi_element elem;
        if (data_time.timestamp > current_ack)  {
          current_ack = data_time.timestamp;
          etimer_set(&et, DECISION_WINDOW);
          clear_element_array();
        }
        if (data_time.timestamp == current_ack)  {
          memcpy(&elem.node_id, from, sizeof(linkaddr_t));
          elem.rssi = data_time.rssi;
          rssis[curr_element_len] = elem;
          curr_element_len++;
          printf("Received RSSi of %i from %d.%d\n",(int) elem.rssi,
              elem.node_id.u8[0],elem.node_id.u8[1]);
          printf("Current cache length %i\n", curr_element_len +1);
        }

      default:
        break;
    }
  }
}
/*---------------------------------------------------------------------------*/
static const struct broadcast_callbacks watch_callbacks = {watch_recv};
static const struct broadcast_callbacks internode_callbacks = {internode_recv};
/*---------------------------------------------------------------------------*/

static struct broadcast_conn broadcast_watch;

PROCESS(watch_listening_process, "Watch packet listening process");

AUTOSTART_PROCESSES(&watch_listening_process, &internode_process);
//AUTOSTART_PROCESSES(&watch_listening_process);
PROCESS_THREAD(watch_listening_process, ev, data)
{

  PROCESS_EXITHANDLER(broadcast_close(&broadcast_watch));

  PROCESS_BEGIN();
  broadcast_open(&broadcast_watch, WATCH_BROADCAST_CHANNEL, &watch_callbacks);
  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(internode_process, ev, data)
{
  PROCESS_EXITHANDLER(broadcast_close(&broadcast_internode));

  PROCESS_BEGIN();
  clock_init();
  broadcast_open(&broadcast_internode, INTER_NODE_CHANNEL, &internode_callbacks);
  PROCESS_END();
}


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void turn_on_led()  {
					//hid_on();
          printf("Turning on light!");
          printf("Received On command \n");
          if (light_colour == COLOUR_CODE_WHITE)  {
            //hid_set_colour_white();
          }
          if (light_colour == COLOUR_CODE_RED)  {
            //hid_set_colour_red();
          }
          if (light_colour == COLOUR_CODE_BLUE)  {
            //hid_set_colour_blue();
          }
          if (light_colour == COLOUR_CODE_GREEN)  {
            //hid_set_colour_green();
          }
          //hid_set_intensity(light_intensity);
}

void turn_off_led() {
  //hid_off();
  printf("turning off light! \n");
  light_on = false;
}

PROCESS_THREAD(calculation_process, ev, data)
{
  PROCESS_EXITHANDLER(broadcast_close(&broadcast_internode));
  PROCESS_BEGIN();
  printf("clock start \n");
  etimer_set(&et, DECISION_WINDOW);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  broadcast_open(&broadcast_internode, INTER_NODE_CHANNEL, &internode_callbacks);
    printf("calculation started - num of comparisons %i \n",curr_element_len);
    bool is_closest = true;
    for (int i = 0; i < curr_element_len; i++) {
      if (rssis[i].rssi > current_rssi)  {
        is_closest = false;
        break;
      }
    }
    if (is_closest) {
      printf("Im the closest !! \n");
      if (light_on) {
        turn_on_led();
      }
    } else {
      turn_off_led();
    }
  PROCESS_END();
}



