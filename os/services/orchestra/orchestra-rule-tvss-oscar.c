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
#include "net/ipv6/uip-ds6-route.h"
#include "net/packetbuf.h"
#include "net/routing/routing.h"
#include "net/mac/tsch/tsch-log.h"
#include "net/mac/tsch/tsch.c"

#define DEBUG DEBUG_PRINT
#include "net/net-debug.h"

uint16_t asfn_schedule=0; //absolute slotframe number for ALICE time varying scheduling
static uint16_t slotframe_handle = 0;
static uint16_t local_channel_offset;
static struct tsch_slotframe *sf_unicast;

static void reschedule_unicast_slotframe(void);


//#if UIP_MAX_ROUTES != 0

#if ORCHESTRA_UNICAST_SENDER_BASED && ORCHESTRA_COLLISION_FREE_HASH
#define UNICAST_SLOT_SHARED_FLAG    ((ORCHESTRA_UNICAST_PERIOD < (ORCHESTRA_MAX_HASH + 1)) ? LINK_OPTION_SHARED : 0)
#else
#define UNICAST_SLOT_SHARED_FLAG      LINK_OPTION_SHARED
#endif

/*---------------------------------------------------------------------------*/ //OK
static uint16_t
get_node_timeslot(const linkaddr_t *addr)
{
  printf("GET_NODE_TIMESLOT tvs-oscar\n");
  if(addr != NULL && ORCHESTRA_UNICAST_PERIOD > 0) {
    return real_hash((ORCHESTRA_LINKADDR_HASH(addr)+asfn_schedule), (ORCHESTRA_UNICAST_PERIOD));
  } else {
    return 0xffff;
  }
}
/*---------------------------------------------------------------------------*/ //OK
static uint16_t
get_node_channel_offset(const linkaddr_t *addr)
{
  printf("GET_NODE_CHANNEL_OFFSET tvs-oscar\n");
  int num_ch = (sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE)/sizeof(uint8_t))-1;

  if(addr != NULL && ORCHESTRA_UNICAST_MAX_CHANNEL_OFFSET >= ORCHESTRA_UNICAST_MIN_CHANNEL_OFFSET && num_ch > 0) {
    return 1+real_hash((ORCHESTRA_LINKADDR_HASH(addr)+asfn_schedule),num_ch);
  } else {
    return 0xffff;
  }
}

/*---------------------------------------------------------------------------*/ //ksh. slotframe_callback. 
#ifdef ALICE_TSCH_CALLBACK_SLOTFRAME_START
void alice_callback_slotframe_start (uint16_t sfid, uint16_t sfsize){  
  asfn_schedule=sfid; //ksh.. update curr asfn_schedule.
  reschedule_unicast_slotframe();
}
#endif

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
  printf("ADD_UC_LINK vts-oscar\n"); 
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
    tsch_schedule_add_link(sf_unicast, link_options, LINK_TYPE_NORMAL, &tsch_broadcast_address,
          timeslot, local_channel_offset, 1);
  }
}
/*---------------------------------------------------------------------------*/
static void
remove_uc_link(const linkaddr_t *linkaddr)
{
  uint16_t timeslot;
  struct tsch_link *l;

  if(linkaddr == NULL) {
    return;
  }

  timeslot = get_node_timeslot(linkaddr);
  l = tsch_schedule_get_link_by_timeslot(sf_unicast, timeslot, local_channel_offset);
  if(l == NULL) {
    return;
  }
  if(!ORCHESTRA_UNICAST_SENDER_BASED) {
    /* Packets to this address were marked with this slotframe and neighbor-specific timeslot;
     * make sure they don't remain stuck in the queues after the link is removed. */
    tsch_queue_free_packets_to(linkaddr);
  }

  /* Does our current parent need this timeslot? */
  if(timeslot == get_node_timeslot(&orchestra_parent_linkaddr)) {
    /* Yes, this timeslot is being used, return */
    return;
  }
  /* Does any other child need this timeslot?
   * (lookup all route next hops) */
  nbr_table_item_t *item = nbr_table_head(nbr_routes);
  while(item != NULL) {
    linkaddr_t *addr = nbr_table_get_lladdr(nbr_routes, item);
    if(timeslot == get_node_timeslot(addr)) {
      /* Yes, this timeslot is being used, return */
      return;
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
}
/*---------------------------------------------------------------------------*/
static void
child_removed(const linkaddr_t *linkaddr)
{
  remove_uc_link(linkaddr);
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

/*---------------------------------------------------------------------------*/ //Time varying slotframe

static void
reschedule_unicast_slotframe(void)
{
  printf("RESCHEDULE_UNICAST_SLOTFRAME\n");
  //remove the whole links scheduled in the unicast slotframe
  struct tsch_link *l;
  l = list_head(sf_unicast->links_list);
  //printf("size of l %d \n",sizeof(l));

//remove the whole links scheduled in the unicast slotframe
  while(l!=NULL) {    
    tsch_schedule_remove_link(sf_unicast, l);
    l = list_head(sf_unicast->links_list);
  }

//scheduling the links
  nbr_table_item_t *item = nbr_table_head(nbr_routes);
  while(item != NULL) {
    linkaddr_t *addr = nbr_table_get_lladdr(nbr_routes, item);

    //add link
    add_uc_link(addr);

    //move to the next item
    item = nbr_table_next(nbr_routes, item);
  }
  /*
  while (l != NULL)
  {
    // Check pointer
    const linkaddr_t linkaddr = l->addr;
    remove_uc_link(&linkaddr);
    add_uc_link(&linkaddr);

    
    l = list_item_next(l);
  }
  */

}

/*---------------------------------------------------------------------------*/
static void
init(uint16_t sf_handle)
{
  //LOG_INFO("INIT default common.c file");
  printf("INIT\n");
  uint16_t timeslot;
  linkaddr_t *local_addr = &linkaddr_node_addr;

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
};
