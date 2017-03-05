#include "light.h"
PROCESS(calculation_process, "Watch Calculation process");
PROCESS(negotiation_process , "Negotiation process");
PROCESS(calculation_broadcast , "Calculation Broadcast");
PROCESS(internode_process, "Internode communication process");
PROCESS(watch_listening_process, "Watch packet listening process");

AUTOSTART_PROCESSES(&watch_listening_process, &internode_process,
    &calculation_broadcast);


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*

             **************Handle Received Packets**************


*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
// If ttl is greater than 0, restransmit packet
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

bool node_in_array(linkaddr_t node)  {
  bool ret = false;
  for (int i = 0; i < curr_element_len; i++)  {
    if (linkaddr_cmp(&rssis[i].node_id,&node) != 0)  {
      ret = true;
      break;
    }
  }
  return ret;
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
          break;
        case ON_PACKET:
          //hid_set_intensity(light_intensity);
          hid_on();
          printf("received an on command \n");
          light_on = true;
          retransmit_settings(packetbuf_dataptr(),sizeof(data_packet_header)
              + sizeof(light_settings_packet),c);
          break;
        case OFF_PACKET:
          hid_off();
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
            curr_element_len = 0;
            printf("SHOULD CALL BROADCAST 200 HERE!!!\n");
            process_post_synch(&calculation_broadcast,200
                ,NULL );
          } else {
            printf("ignored repeated watch announce packet \n");
          }
        default:
          break;
      }
    }
  }
}

static void internode_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  // Lets determine the type of packet received
  data_packet_header data_header;
  light_time_packet data_time;
  announce_packet announce;
  memcpy(&data_header,packetbuf_dataptr(),sizeof(data_packet_header));
  char * char_ptr;
  if (data_header.system_code == SYSTEM_CODE)  {
    switch (data_header.packet_type)  {
      case INTER_NODE_PACKET:
        char_ptr = (char *) packetbuf_dataptr();
        memcpy(&data_time, char_ptr+sizeof(data_packet_header),
          sizeof(light_time_packet));
        if (data_time.timestamp > current_ack)  {
          printf(" Received newer timestamp, resetting decision period \n");
          current_ack = data_time.timestamp;
          etimer_set(&et, DECISION_WINDOW);
          curr_element_len = 0;
        }
        if (data_time.timestamp == current_ack &&
            linkaddr_cmp(&data_time.node_id,&linkaddr_node_addr) == 0 &&
            !node_in_array(data_time.node_id))  {
          rssis[curr_element_len] = data_time;
          curr_element_len++;
          printf("Received RSSi of %i from %d.%d\n",(int) data_time.rssi,
              data_time.node_id.u8[0],data_time.node_id.u8[1]);
          printf("Current cache length %i\n", curr_element_len +1);
        }
       if (data_time.timestamp < current_ack)  {
         printf("Dropped old packet \n");
       }  else {
         retransmit_settings(packetbuf_dataptr(), sizeof(data_packet_header)
          + sizeof(light_time_packet), c);
       }

        break;
      case ANNOUNCE_CLOSEST_PACKET:
        char_ptr = (char *) packetbuf_dataptr();
        printf("received announce packet \n");
        memcpy(&announce, char_ptr + sizeof(data_packet_header),
          sizeof(announce));

        if (data_header.ack_no == current_ack)  {
          announcers[curr_announce_elem_len] = announce;
          curr_announce_elem_len++;
          printf("Received num_comparisons of %i from %d.%d\n",(int) announce.num_comparisons,
              announce.closest_node.u8[0],announce.closest_node.u8[1]);
          printf("Current announce cache length %i\n", curr_announce_elem_len +1);
        } else {
          printf("we dropped an announce for some reason\n");
        }
        if (data_time.timestamp < current_ack)  {
          printf("Dropped old packet \n");
        }  else {

          retransmit_settings(packetbuf_dataptr(), sizeof(data_packet_header)
            + sizeof(announce_packet), c);
        }

        break;
      default:
        printf("received unknown system packet \n");
        break;
      if (data_time.timestamp < current_ack)  {
        printf("Dropped old packet \n");
      }  else {

      }
    }
  }
}
/*---------------------------------------------------------------------------*/
static const struct broadcast_callbacks watch_callbacks = {watch_recv};
static const struct broadcast_callbacks internode_callbacks = {internode_recv};
/*---------------------------------------------------------------------------*/

static struct broadcast_conn broadcast_watch;
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
/*

***********************CALCULATION PROCESS************************************
*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(calculation_broadcast , ev, data)
{
  PROCESS_BEGIN();
  mmem_init();
  while(1) {
    printf("calculation_broadcast called \n");
    PROCESS_YIELD();
    if (ev != 200) continue;
    data_packet_header header;
    header.system_code = SYSTEM_CODE;
    header.source_node_type = 1;
    header.packet_type = INTER_NODE_PACKET;
    light_time_packet packet;
    packet.timestamp = current_ack;
    packet.rssi = current_rssi;
    packet.node_id = linkaddr_node_addr;
    header.ttl = INTERNODE_TTL;
    printf("SENT watch RSSI %d \n",(int)packet.rssi);
    header.ack_no = current_ack;
    int packet_size = sizeof(data_packet_header)
            + sizeof(light_time_packet);
    if(mmem_alloc(&mmem, packet_size) == 0) {
      printf("memory allocation failed\n");
    } else {
      packet_ptr_calc = (char *) MMEM_PTR(&mmem);
      memcpy(packet_ptr_calc,&header,sizeof(data_packet_header));
      memcpy(packet_ptr_calc+sizeof(data_packet_header),&packet,
          sizeof(light_time_packet));
      void_ptr_calc = (void *) packet_ptr_calc;
      for (loop_counter = 0; loop_counter < NUM_OF_CALC_REBROADCASTS;
          loop_counter++) {
        printf("Sent the %i st time \n",loop_counter);
        t_rand = random_rand() % CLOCK_SECOND/2;
        etimer_set(&randt, t_rand);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&randt));
        printf("broadcast internode sent \n");
        packetbuf_copyfrom(void_ptr_calc,packet_size);
        broadcast_send(&broadcast_internode);

      }
      mmem_free(&mmem);
    }
  }
  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void turn_on_led()  {
					hid_on();
          printf("Turning on light!");
          printf("Received On command \n");
}

void turn_off_led() {
  hid_off();
  printf("turning off light! \n");
  light_on = false;
}
static struct mmem mmem2;
char * packet_ptr_2;
void * void_ptr_2;
linkaddr_t closest_node;
float closest_rssi;

PROCESS_THREAD(calculation_process, ev, data)
{
  PROCESS_EXITHANDLER(broadcast_close(&broadcast_internode));
  PROCESS_BEGIN();
  etimer_set(&et, DECISION_WINDOW);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  broadcast_open(&broadcast_internode, INTER_NODE_CHANNEL, &internode_callbacks);
  printf("calculation started - num of comparisons %i \n",curr_element_len);
  closest_node = linkaddr_node_addr;
  closest_rssi = current_rssi;
  for (int i = 0; i < curr_element_len; i++) {
    if (rssis[i].rssi > closest_rssi)  {
      closest_rssi = rssis[i].rssi;
      closest_node = rssis[i].node_id;
      break;
    }
   }
  process_start(&negotiation_process,NULL);
  data_packet_header header;
  header.system_code = SYSTEM_CODE;
  header.source_node_type = 1;
  header.packet_type = ANNOUNCE_CLOSEST_PACKET;
  header.ack_no = current_ack;
  header.ttl = INTERNODE_TTL;
  announce_packet packet;
  packet.closest_node = closest_node;
  packet.num_comparisons = curr_element_len;
  int packet_size = sizeof(data_packet_header) + sizeof(announce_packet);
  if(mmem_alloc(&mmem2, packet_size) == 0) {
      printf("memory allocation failed\n");
  } else {

    packet_ptr_2 = (char *) MMEM_PTR(&mmem2);
    memcpy(packet_ptr_2,&header,sizeof(data_packet_header));
    memcpy(packet_ptr_2+sizeof(data_packet_header),&packet,
        sizeof(announce_packet));
    void_ptr_2 = (void *) packet_ptr_2;
    for (loop_counter = 0; loop_counter < NUM_OF_ANNOUNCE_REBROADCASTS;
        loop_counter++) {
      printf("Sent the %i  time \n",loop_counter);
      t_rand = random_rand() % CLOCK_SECOND/2;
      etimer_set(&randt, t_rand);
      printf("Decided closest , broadcasting decision \n");
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&randt));
      packetbuf_copyfrom(void_ptr_2,packet_size);
      broadcast_send(&broadcast_internode);

    }
    mmem_free(&mmem2);
  }
  PROCESS_END();
}



PROCESS_THREAD(negotiation_process, ev, data)
{
  PROCESS_EXITHANDLER(broadcast_close(&broadcast_internode));
  PROCESS_BEGIN();
  etimer_set(&et3, NEGOTIATION_WINDOW);
  printf("started negotiation time, wait %i\n ", NEGOTIATION_WINDOW
      / CLOCK_SECOND);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et3));
  broadcast_open(&broadcast_internode, INTER_NODE_CHANNEL, &internode_callbacks);
  printf("decisions started - num of comparisons %i \n",curr_announce_elem_len);
  linkaddr_t negotiated_closest_node = closest_node;
  int negotiated_comp = curr_element_len;
  for (int i = 0; i < curr_announce_elem_len; i++) {
    if (announcers[i].num_comparisons > negotiated_comp)  {
      negotiated_closest_node = announcers[i].closest_node;
      negotiated_comp = announcers[i].num_comparisons;
      break;
    }
  }
  // Reset array
  curr_announce_elem_len = 0;
  bool is_closest = linkaddr_cmp(&linkaddr_node_addr,&negotiated_closest_node)
    != 0;
  if (is_closest) {
    printf("Im the closest !! \n");
    if (light_on) {
      // turn_on_led();
      // change colour to green
      hid_set_colour_green();
    }
  } else {
    // turn_off_led();
    // change colour back to white
    hid_set_colour_white();
  }
  PROCESS_END();
}
