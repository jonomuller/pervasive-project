#include "light.h"
PROCESS(calculation_process, "Watch Calculation process");
PROCESS(negotiation_process , "Negotiation process");
PROCESS(calculation_broadcast , "Calculation Broadcast");
PROCESS(internode_process, "Internode communication process");
PROCESS(watch_listening_process, "Watch packet listening process");

AUTOSTART_PROCESSES(&watch_listening_process, &internode_process,
    &calculation_broadcast);

int get_time() {
  return clock_time() - time_of_announce;
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*

             **************Handle Received Packets**************


*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
// If ttl is greater than 0, retransmit packet
void retransmit_settings(void * packet, int size,
    struct broadcast_conn* broadcast)  {
    data_packet_header *header = (data_packet_header *) packet;

  if (header->ttl > 0)  {
    header->ttl -= 1;

    //printf("[%i][%i secs]retrasmitting packet %i %i \n",header->ack_no,header->ttl);
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
      //printf("[%i][%i secs]received light system packet \n");
      switch (data_header.packet_type)  {
        case LIGHT_SETTINGS_PACKET:
          printf("\n");
          char * data_pt = (char*) packetbuf_dataptr();
          memcpy(&settings,data_pt+sizeof(data_packet_header)
              ,sizeof(light_settings_packet));
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
          hid_on();
          printf("[%i][%i secs][WATCH COMMAND] ON RECEIVED\n",get_time(),get_time()/CLOCK_SECOND);
          light_on = true;
          retransmit_settings(packetbuf_dataptr(),sizeof(data_packet_header)
              + sizeof(light_settings_packet),c);
          break;
        case OFF_PACKET:
          hid_off();
          light_on = false;
          printf("[%i][%i secs][WATCH COMMAND] OFF RECEIVED\n",get_time(),get_time()/CLOCK_SECOND);
          retransmit_settings(packetbuf_dataptr(),sizeof(data_packet_header),c);
          break;
        case WATCH_ANNOUNCE_PACKET:
          if (data_header.ack_no > current_ack)  {
            printf("[0][0][WATCH COMMAND] RECEIVED NEW WATCH ANNOUNCE, RESET CLOCK\n");
            time_of_announce = clock_time();
            rssi_from_watch = packetbuf_attr(PACKETBUF_ATTR_RSSI) - 65536.0;
            current_ack = data_header.ack_no;
            current_rssi = rssi_from_watch;
            process_exit(&calculation_process);
            process_start(&calculation_process,NULL);
            curr_element_len = 0;
            process_post_synch(&calculation_broadcast,200
                ,NULL );
          } else {
            printf("[%i][%i secs][WATCH COMMAND] IGNORED REPEAT WATCH ANNOUNCE PACKET \n"
              ,get_time(),get_time()/CLOCK_SECOND);
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
          printf("[%i][%i secs][INTERNODE UPDATE] NEW TIMESTAMP RECEIVED - RESET \n"
            ,get_time(),get_time()/CLOCK_SECOND);
          current_ack = data_time.timestamp;
          etimer_set(&et, DECISION_WINDOW);
          curr_element_len = 0;
        }
        if (data_time.timestamp == current_ack &&
            linkaddr_cmp(&data_time.node_id,&linkaddr_node_addr) == 0 &&
            !node_in_array(data_time.node_id))  {
          rssis[curr_element_len] = data_time;
          curr_element_len++;
          printf("[%i][%i secs][INTERNODE UPDATE] RECEIVED RSSI OF %i FROM %d.%d \n",get_time()
            ,get_time()/CLOCK_SECOND,(int) data_time.rssi
            , data_time.node_id.u8[0],data_time.node_id.u8[1]);
        }
       if (data_header.ack_no < current_ack)  {
         printf("[%i][%i secs][INTERNODE UPDATE] DROPPED PACKET %i, CURRENT ACK is %i\n",get_time()
           ,get_time()/CLOCK_SECOND,
           data_header.ack_no, current_ack);
       }  else {
         retransmit_settings(packetbuf_dataptr(), sizeof(data_packet_header)
          + sizeof(light_time_packet), c);
       }

        break;
      case ANNOUNCE_CLOSEST_PACKET:
        char_ptr = (char *) packetbuf_dataptr();
        memcpy(&announce, char_ptr + sizeof(data_packet_header),
          sizeof(announce));
        if (data_header.ack_no == current_ack)  {
          announcers[curr_announce_elem_len] = announce;
          curr_announce_elem_len++;
          printf("[%i][%i secs][INTERNODE UPDATE] RECEIVED %i COMPARISONS FROM %d.%d \n"
            ,get_time(),get_time()/CLOCK_SECOND,
            (int) announce.num_comparisons,
            announce.closest_node.u8[0],announce.closest_node.u8[1]);
        } else {
          printf("[%i][%i secs][INTERNODE UPDATE] ANNOUNCE %i DROPPED , CURRENT ACK is %i\n"
          ,get_time(),get_time()/CLOCK_SECOND, data_header.ack_no, current_ack);
        }
        if (data_header.ack_no < current_ack)  {
          //printf("[%i][%i secs]Dropped old packet \n");
        }  else {
          retransmit_settings(packetbuf_dataptr(), sizeof(data_packet_header)
            + sizeof(announce_packet), c);
        }

        break;
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
    //printf("[%i][%i secs]calculation_broadcast called \n");
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


    header.ack_no = current_ack;
    int packet_size = sizeof(data_packet_header)
            + sizeof(light_time_packet);
    if(mmem_alloc(&mmem, packet_size) == 0) {
      printf("[%i][%i secs][ERROR] memory allocation failed\n",get_time(),get_time()/CLOCK_SECOND);
    } else {
      packet_ptr_calc = (char *) MMEM_PTR(&mmem);
      memcpy(packet_ptr_calc,&header,sizeof(data_packet_header));
      memcpy(packet_ptr_calc+sizeof(data_packet_header),&packet,
          sizeof(light_time_packet));
      void_ptr_calc = (void *) packet_ptr_calc;
      for (loop_counter = 0; loop_counter < NUM_OF_CALC_REBROADCASTS;
          loop_counter++) {
        //printf("[%i][%i secs]Sent the %i st time \n",loop_counter);
        t_rand = random_rand() % CLOCK_SECOND/2;
        etimer_set(&randt, t_rand);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&randt));
        printf("[%i][%i secs][INTERNODE UPDATE] SENT RSSI %d \n",get_time(),get_time()/CLOCK_SECOND,(int)packet.rssi);
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
          //printf("[%i][%i secs]Turning on light!");
          //printf("[%i][%i secs]Received On command \n");
}

void turn_off_led() {
  hid_off();
  //printf("[%i][%i secs]turning off light! \n");
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
  printf("[%i][%i secs][CALCULATION] STARTED WITH %i COMPARISONS \n",get_time()
    ,get_time()/CLOCK_SECOND,curr_element_len);
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
      printf("[%i][%i secs][ERROR] memory allocation failed\n",get_time(),get_time()/CLOCK_SECOND);
  } else {

    packet_ptr_2 = (char *) MMEM_PTR(&mmem2);
    memcpy(packet_ptr_2,&header,sizeof(data_packet_header));
    memcpy(packet_ptr_2+sizeof(data_packet_header),&packet,
        sizeof(announce_packet));
    void_ptr_2 = (void *) packet_ptr_2;
    for (loop_counter = 0; loop_counter < NUM_OF_ANNOUNCE_REBROADCASTS;
        loop_counter++) {
      //printf("[%i][%i secs]Sent the %i  time \n",loop_counter);
      t_rand = random_rand() % CLOCK_SECOND/2;
      etimer_set(&randt, t_rand);
      printf("[%i][%i secs][CALCULATION] DETERMINED NODE %d.%d IS CLOSEST - BROADCASTING RESULT \n"
        ,get_time(),get_time()/CLOCK_SECOND,closest_node.u8[0],closest_node.u8[1]);
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
  printf("[%i][%i secs][DECISION TIME] DECISION WINDOW STARTED %i SECONDS\n ",get_time(),get_time()/CLOCK_SECOND
     , NEGOTIATION_WINDOW/ CLOCK_SECOND);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et3));
  broadcast_open(&broadcast_internode, INTER_NODE_CHANNEL, &internode_callbacks);
  printf("[%i][%i secs][DECISION TIME] CHOOSING CLOSEST - %i COMPARISONS \n",get_time(),get_time()/CLOCK_SECOND
    ,curr_announce_elem_len);
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
    printf("[%i][%i secs][DECISION TIME] COMPUTED THAT I AM THE CLOSEST \n",get_time(),get_time()/CLOCK_SECOND);
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
