/*
 * Copyright (c) 2015, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/**
 * \file
 *         Orchestra: a slotframe with a single shared link, common to all nodes
 *         in the network, used for unicast and broadcast.
 *
 * \author Simon Duquennoy <simonduq@sics.se>
 *          Lucas Fache <lucas.jan.c.fache@vub.be>
 * 
 *          This code is based on ALICE:
 *          Kim, Seohyang & Kim, Hyung-sin & Kim, Chongkwon. (2019). ALICE: autonomous link-based cell scheduling 
 *          for TSCH. 10.1145/3302506.3310394. 

 *          and on OSCAR:
 *          Osman, Mohamed & Nabki, Frederic. (2021). OSCAR: An Optimized Scheduling Cell Allocation Algorithm 
 *          for Convergecast in IEEE 802.15.4e TSCH Networks. Sensors. 21. 2493. 10.3390/s21072493. 

 */

#include "contiki.h"
#include "orchestra.h"
#include "lib/memb.h"
#include "net/ipv6/uip-ds6-route.h"
#include "net/packetbuf.h"
#include "net/routing/routing.h"
#include "net/mac/tsch/tsch-log.h"
#include "net/mac/tsch/tsch.c"

#include "net/routing/rpl-classic/rpl-private.h"
#include "stdlib.h"

//#define DEBUG DEBUG_PRINT
#include "net/net-debug.h"

/* The absolute slotframe number for ALICE time varying scheduling */
uint16_t asfn_schedule=0; 
uint16_t nbr_extra_tx_slots = 0;
static uint16_t slotframe_handle = 0;
static uint16_t local_channel_offset;
static struct tsch_slotframe *sf_unicast;
/* The current class of the node */
uint16_t current_class;

/* The incoming packet count at a fixed time interval to measure the incoming traffic load */
int packet_count = 0;

#ifdef ALICE_TSCH_CALLBACK_SLOTFRAME_START
static void reschedule_unicast_slotframe(void);
/* Pre-allocated space for links */
//MEMB(link_memb, struct tsch_link, TSCH_SCHEDULE_MAX_LINKS);
#endif

//#if UIP_MAX_ROUTES != 0

#if ORCHESTRA_UNICAST_SENDER_BASED && ORCHESTRA_COLLISION_FREE_HASH
#define UNICAST_SLOT_SHARED_FLAG    ((ORCHESTRA_UNICAST_PERIOD < (ORCHESTRA_MAX_HASH + 1)) ? LINK_OPTION_SHARED : 0)
#else
#define UNICAST_SLOT_SHARED_FLAG      LINK_OPTION_SHARED
#endif

/* In this thread the incoming traffic load is measured */
PROCESS(traffic_load_process,"traffic load");

/*---------------------------------------------------------------------------*/ // LF
static uint16_t
get_node_timeslot(const linkaddr_t *addr)
{
  if(addr != NULL && ORCHESTRA_UNICAST_PERIOD > 0) {
    return real_hash((ORCHESTRA_LINKADDR_HASH(addr)+asfn_schedule), (ORCHESTRA_UNICAST_PERIOD)); //+asfn_schedule
  } else {
    return 0xffff;
  }
}
/*---------------------------------------------------------------------------*/  // LF
static uint16_t
get_node_channel_offset(const linkaddr_t *addr)
{
  int num_ch = (sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE)/sizeof(uint8_t))-1;

  if(addr != NULL && ORCHESTRA_UNICAST_MAX_CHANNEL_OFFSET >= ORCHESTRA_UNICAST_MIN_CHANNEL_OFFSET && num_ch > 0) {
    return 1+real_hash((ORCHESTRA_LINKADDR_HASH(addr)+asfn_schedule),num_ch); //+asfn_schedule
  } else {
    return 0xffff;
  }
}

/*---------------------------------------------------------------------------*/ //ksh. slotframe_callback.  LF
#ifdef ALICE_TSCH_CALLBACK_SLOTFRAME_START
void alice_callback_slotframe_start (uint16_t sfid, uint16_t sfsize)
{  
  //Before rescheduling check if there are packets in the queue and if the ASFN is increassed.
  if (tsch_queue_global_packet_count() != 0 && sfid != asfn_schedule) {
    asfn_schedule = sfid; //update curr asfn_schedule.
    printf("TVSS new slotframenumber = %u\n",asfn_schedule);
    reschedule_unicast_slotframe();
  }
}
#endif 
/*---------------------------------------------------------------------------*/
// Increase the number of allocated slots according to the class of the node.
static void
allocate_more_slots(uint16_t new_class)
{
  uint16_t nbr_tx_slots = 0;
  if(new_class == 1) nbr_tx_slots = 3;
  if(new_class == 2) nbr_tx_slots = 2;
  if(new_class == 3) nbr_tx_slots = 1;

  while(nbr_extra_tx_slots < nbr_tx_slots) {
    nbr_extra_tx_slots++;
    //printf("Routing class = %u added an extra slot nbr: %u\n",new_class,nbr_extra_tx_slots);
    linkaddr_t *local_addr = &linkaddr_node_addr;                 
    local_channel_offset = get_node_channel_offset(local_addr);   
    uint16_t timeslot = get_node_timeslot(&tsch_broadcast_address);
    uint8_t link_options = ORCHESTRA_UNICAST_SENDER_BASED ? LINK_OPTION_RX : LINK_OPTION_TX | UNICAST_SLOT_SHARED_FLAG;


    tsch_schedule_add_link(sf_unicast, link_options, LINK_TYPE_NORMAL, &tsch_broadcast_address,
          timeslot, local_channel_offset, 1);
    
    link_options = ORCHESTRA_UNICAST_SENDER_BASED ? LINK_OPTION_TX | UNICAST_SLOT_SHARED_FLAG: LINK_OPTION_RX;
    tsch_schedule_add_link(sf_unicast, link_options, LINK_TYPE_NORMAL, &tsch_broadcast_address,
          timeslot, local_channel_offset, 1);

  }
}
/*---------------------------------------------------------------------------*/
// Reduce the number of allocated slots according to the class of the node.
static void
reduce_allocated_slots(uint16_t new_class)
{
  uint16_t nbr_tx_slots = 0;
  if(new_class == 1) nbr_tx_slots = 3;
  if(new_class == 2) nbr_tx_slots = 2;
  if(new_class == 3) nbr_tx_slots = 1;

  uint8_t link_options_one = ORCHESTRA_UNICAST_SENDER_BASED ? LINK_OPTION_RX : LINK_OPTION_TX | UNICAST_SLOT_SHARED_FLAG; 
  uint8_t link_options_two = ORCHESTRA_UNICAST_SENDER_BASED ? LINK_OPTION_TX | UNICAST_SLOT_SHARED_FLAG: LINK_OPTION_RX;
  bool option_one = false;
  bool option_two = false;

  while(nbr_extra_tx_slots > nbr_tx_slots) {
    nbr_extra_tx_slots--;
    //printf("Routing class = %u REMOVED an extra slot nbr: %u\n",new_class,nbr_extra_tx_slots);
    
    struct tsch_link *l = list_head(sf_unicast->links_list);

    while(l!=NULL) { 
      if(!option_one) {
        if(&l->addr == &tsch_broadcast_address && l->link_options == link_options_one) {
          tsch_schedule_remove_link(sf_unicast, l);
          option_one = true;
        }
      }
      if(!option_one) {
        if(&l->addr == &tsch_broadcast_address && l->link_options == link_options_two) {
          tsch_schedule_remove_link(sf_unicast, l);
          option_two = true;
        }
      }

      if(option_one && option_two) {
        break;
      }
      l = l->next;
    }    
  }
}
/*---------------------------------------------------------------------------*/ 
/*---------------------------------------------------------------------------*/ 
// Check to see if the node is a root node.
uint16_t
is_root(){
  rpl_instance_t *instance = rpl_get_default_instance();
  if(instance!=NULL && instance->current_dag!=NULL) {
       uint16_t min_hoprankinc = instance->min_hoprankinc;
       uint16_t rank=(uint16_t)instance->current_dag->rank;
       if(min_hoprankinc == rank){
          return 1;
       }
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
// This method checks the new assigned class and changes the number of allocated slots when the class is changed.
static void
reschedule_timeslots(uint16_t new_class)
{
  if(new_class == current_class) {
    return;
  }

  if(new_class < current_class) {
    allocate_more_slots(new_class);
  }
  else {
    reduce_allocated_slots(new_class);
  }
}
/*---------------------------------------------------------------------------*/ 
// At this point the rpl-rk is not used to calculate the class. This option can be enabled when using more classes and when you are dealing with bigger networks.
/*
static uint16_t
get_rpl_rank()
{
	rpl_dag_t *dag = rpl_get_any_dag();
  uint16_t rank;

  if(dag != NULL && dag->instance != NULL) {
		rank = dag->rank;
	}

  uint16_t div = rank/100;
 
  if (div <= 1) {
    return 1;
  }
  else if(div > MAX_NODE_CLASS) {
    return 6;
  }
  else {
      return div;
  }
}
*/
/*---------------------------------------------------------------------------*/ 
/*
* Calculate the class of the node
* In the latest implementation the RPL-rank is not used to calculate the class of the node.
*/
static void 
set_node_class()
{
	//uint16_t rpl_rank = get_rpl_rank();
	uint16_t subtree_size = uip_ds6_route_num_routes();
	uint16_t new_class;  //root is class 1 max class is 4

  //printf("Routing PACKET_COUNT = %u\n",packet_count);

  if (subtree_size == 0) {
    new_class = MAX_NODE_CLASS;
  }
  else if (is_root()) {
    new_class = 1;
  }
  else {
    if(subtree_size >= SUBTREE_THRESHOLD || packet_count > TRAFFIC_LOAD_THRESHOLD) {
      new_class = 2;
    }
    else { 
      new_class = 3;
    }
  }

  //printf("NODE_CLASS_INFO (Current class) \t subtree_size = %u \t new_class = %u\n",subtree_size,new_class);

  reschedule_timeslots(new_class);
  current_class = new_class;
}

/*---------------------------------------------------------------------------*/
static int
neighbor_has_uc_link(const linkaddr_t *linkaddr)
{
  if(linkaddr != NULL && !linkaddr_cmp(linkaddr, &linkaddr_null)) {
    if((orchestra_parent_knows_us || !ORCHESTRA_UNICAST_SENDER_BASED)
       && linkaddr_cmp(&orchestra_parent_linkaddr, linkaddr)) {
      return 1;
    }
    if(nbr_table_get_from_lladdr(nbr_routes, (linkaddr_t *)linkaddr) != NULL) {
      return 1;
    }
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static void
add_uc_link(const linkaddr_t *linkaddr)
{
  if(linkaddr != NULL) {
    linkaddr_t *local_addr = &linkaddr_node_addr;                 // LF
    local_channel_offset = get_node_channel_offset(local_addr);   // LF
    uint16_t timeslot = get_node_timeslot(linkaddr);
    uint8_t link_options = ORCHESTRA_UNICAST_SENDER_BASED ? LINK_OPTION_RX : LINK_OPTION_TX | UNICAST_SLOT_SHARED_FLAG;

    if(timeslot == get_node_timeslot(&linkaddr_node_addr)) {
      /* This is also our timeslot, add necessary flags */
      link_options |= ORCHESTRA_UNICAST_SENDER_BASED ? LINK_OPTION_TX | UNICAST_SLOT_SHARED_FLAG: LINK_OPTION_RX;
    }
    /* Add/update link.
     * Always configure the link with the local node's channel offset.
     * If this is an Rx link, that is what the node needs to use.
     * If this is a Tx link, packet's channel offset will override the link's channel offset.
     */ 
    //&tsch_broadcast_address was used to set the address, but is not necessary broadcast is set if needed when link is used to set packet in queue
    tsch_schedule_add_link(sf_unicast, link_options, LINK_TYPE_NORMAL, linkaddr,
          timeslot, local_channel_offset, 1);
  }
}
/*---------------------------------------------------------------------------*/
static void
remove_uc_link(const linkaddr_t *linkaddr)
{
  uint16_t timeslot = 0;
  uint16_t timeslot_orchestra_parent;

  if(linkaddr == NULL) {
    return;
  }

  struct tsch_link *l = list_head(sf_unicast->links_list);
  // Searching for the link in the linklist
  while(l!=NULL) { 
    if(&l->addr == linkaddr){
      timeslot = l->timeslot;
      break;
    }
    if(&l->addr == &orchestra_parent_linkaddr){
      timeslot_orchestra_parent = l->timeslot;
      break;
    }
    l = list_item_next(l);
  }

  // Did not find linkaddr in the slotframe link_list.
  if(timeslot == 0) {
    return;
  }

  if(!ORCHESTRA_UNICAST_SENDER_BASED) {
    /* Packets to this address were marked with this slotframe and neighbor-specific timeslot;
     * make sure they don't remain stuck in the queues after the link is removed. */
    tsch_queue_free_packets_to(linkaddr);
  }

  /* Does our current parent need this timeslot? */
  if(timeslot == timeslot_orchestra_parent) {
    /* Yes, this timeslot is being used, return */
    return;
  }

  /* Does any other child need this timeslot?
   * (lookup all route next hops) */
  nbr_table_item_t *item = nbr_table_head(nbr_routes);
  while(item != NULL) {
    linkaddr_t *addr = nbr_table_get_lladdr(nbr_routes, item);
    
    struct tsch_link *ll = list_head(sf_unicast->links_list);

    while(ll!=NULL) { 
      if(&ll->addr == addr){
        /* Yes, this timeslot is being used, return */
        return;
      }
      ll = list_item_next(ll);
    }

    item = nbr_table_next(nbr_routes, item);
  }

  /* Do we need this timeslot? */
  if(timeslot == get_node_timeslot(&linkaddr_node_addr)) {
    /* This is our link, keep it but update the link options */
    uint8_t link_options = ORCHESTRA_UNICAST_SENDER_BASED ? LINK_OPTION_TX | UNICAST_SLOT_SHARED_FLAG: LINK_OPTION_RX;
    tsch_schedule_add_link(sf_unicast, link_options, LINK_TYPE_NORMAL, &tsch_broadcast_address,
              timeslot, local_channel_offset, 1);
  } else {
    /* Remove link */
    tsch_schedule_remove_link(sf_unicast, l);
  }
}
/*---------------------------------------------------------------------------*/
static void
child_added(const linkaddr_t *linkaddr)
{
  add_uc_link(linkaddr);
  set_node_class();
}
/*---------------------------------------------------------------------------*/
static void
child_removed(const linkaddr_t *linkaddr)
{
  remove_uc_link(linkaddr);
  set_node_class();
}
/*---------------------------------------------------------------------------*/
static int
select_packet(uint16_t *slotframe, uint16_t *timeslot, uint16_t *channel_offset)
{
  /* Select data packets we have a unicast link to */
  const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
  if(packetbuf_attr(PACKETBUF_ATTR_FRAME_TYPE) == FRAME802154_DATAFRAME
     && !orchestra_is_root_schedule_active(dest)
     && neighbor_has_uc_link(dest)) {
    if(slotframe != NULL) {
      *slotframe = slotframe_handle;
    }
    if(timeslot != NULL) {
      *timeslot = ORCHESTRA_UNICAST_SENDER_BASED ? get_node_timeslot(&linkaddr_node_addr) : get_node_timeslot(dest);
    }
    /* set per-packet channel offset */
    if(channel_offset != NULL) {
      *channel_offset = get_node_channel_offset(dest);
    }
    return 1;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static void
new_time_source(const struct tsch_neighbor *old, const struct tsch_neighbor *new)
{
  if(new != old) {
    const linkaddr_t *old_addr = tsch_queue_get_nbr_address(old);
    const linkaddr_t *new_addr = tsch_queue_get_nbr_address(new);
    if(new_addr != NULL) {
      linkaddr_copy(&orchestra_parent_linkaddr, new_addr);
    } else {
      linkaddr_copy(&orchestra_parent_linkaddr, &linkaddr_null);
    }
    remove_uc_link(old_addr);
    add_uc_link(new_addr);
  }
}

/*---------------------------------------------------------------------------*/ 
// Time varying slotframe: Rescheduling of the unicast slotframe
#ifdef ALICE_TSCH_CALLBACK_SLOTFRAME_START
static void
reschedule_unicast_slotframe(void)
{
  printf("Routing RESCHEDULING\n");
  //reschedule all the links
  //linkaddr_t *local_addr = &linkaddr_node_addr; 
  LIST(temp_list);
  /*
  linkaddr_list_t * head = list_head(temp_list);
  
  head = (linkaddr_list_t *) malloc(sizeof(linkaddr_list_t));
  LIST_STRUCT_INIT(head, temp_links_list);
  */
  struct tsch_link *ll = list_head(sf_unicast->links_list);
  struct tsch_link *link = malloc(sizeof(struct tsch_link));

  while(ll!=NULL) {    
    printf("Routing into first while loop : ");
    link->addr = ll->addr;
    link->link_options = ll->link_options;
    link->next = malloc(sizeof(struct tsch_link)); //memb_alloc(&link_memb);
    // for testing without impacting the scheduler
    // you can disable the two lines underneed, disable line 479 and enable line 461
    list_add(temp_list, link);
    printf("%u",tsch_schedule_remove_link(sf_unicast, ll));
    
    //ll = list_head(sf_unicast->links_list);
    ll = ll->next;
    printf("\nRouting Link-option = %u\n",link->link_options);
    link = link->next;
  }


  struct tsch_link *l = list_head(temp_list);
  printf("Routing HEAD Link-option head = %u\n",l->link_options);
  //printf("TVSS list = %u \n",l->timeslot);
  /*while(l!=NULL) {    
    printf("Routing HEAD Link-option head = %u\n",l->link_options);
    
    const linkaddr_t *linkaddr = &link->addr;
    uint16_t timeslot = get_node_timeslot(linkaddr);
    uint16_t local_channel_offset = get_node_channel_offset(local_addr);
    uint8_t link_options = link->link_options;
    
    //printf("Routing ADDR = %p, timeslot = %u, channel_off = %u, linkoptions = %u \n",link->addr, timeslot,local_channel_offset,link_options);
    
    tsch_schedule_add_link(sf_unicast, link_options, LINK_TYPE_NORMAL, linkaddr, timeslot, local_channel_offset, 1);
    
    //printf(" TVSS timeslot = %u \t channel_offset = %u  \t new timeslot = %u new channel_offset = %u  asfn = %u\n",l->timeslot,l->channel_offset,timeslot,local_channel_offset,asfn_schedule);
    //link = link->next;
    //printf("Routing HEAD Link-option head = %u\n",link->link_options);
    list_remove(temp_list,l);
    l = list_head(temp_list);
  }*/

  free(link);
  //free(head);
}
#endif
/*---------------------------------------------------------------------------*/
/**
 * This thread will retrieve the number of incoming packets at a fixed time intervall
 */
PROCESS_THREAD(traffic_load_process, ev, data)
{
  PROCESS_BEGIN();

  static struct etimer timer;
  etimer_set(&timer, CLOCK_SECOND * 30); // Every 30 seconds

  while(1) {
    int temp_packet_count = packet_count;
    packet_count = get_rx_packet_count();
    etimer_reset(&timer);
    if(packet_count == 0 && current_class < MAX_NODE_CLASS){
      //printf("Routing HIGHER CLASS IS SET\n");
      reschedule_timeslots(current_class+1);
      current_class++;
    }
    else if (temp_packet_count == 0 && packet_count != 0){
      //printf("Routing BACK TO INITIAL CLASS\n");
      set_node_class();
    }

    PROCESS_WAIT_UNTIL(etimer_expired(&timer));
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
static void
init(uint16_t sf_handle)
{
  printf("tvss-oscar\n");
  uint16_t timeslot;
  linkaddr_t *local_addr = &linkaddr_node_addr;

  process_start(&traffic_load_process, NULL);
  set_node_class();
  printf("INIT Current class = %u\n",current_class);

  slotframe_handle = sf_handle;
  /* Slotframe for unicast transmissions */
  sf_unicast = tsch_schedule_add_slotframe(slotframe_handle, ORCHESTRA_UNICAST_PERIOD);
  asfn_schedule = tsch_schedule_get_current_asfn(sf_unicast);//ksh..   LF

  local_channel_offset = get_node_channel_offset(local_addr);
  timeslot = get_node_timeslot(local_addr);
  tsch_schedule_add_link(sf_unicast,
            ORCHESTRA_UNICAST_SENDER_BASED ? LINK_OPTION_TX | UNICAST_SLOT_SHARED_FLAG: LINK_OPTION_RX,
            LINK_TYPE_NORMAL, &tsch_broadcast_address,
            timeslot, local_channel_offset, 1);
      
}
/*---------------------------------------------------------------------------*/
struct orchestra_rule tvss_oscar = {
  init,
  new_time_source,
  select_packet,
  child_added,
  child_removed,
  NULL,
  "time varying slotframe schedule and oscar",
  ORCHESTRA_UNICAST_PERIOD,
  set_node_class,
};
