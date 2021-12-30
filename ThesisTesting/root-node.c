/*
 * Copyright (c) 2012, Thingsquare, www.thingsquare.com.
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
 */


#include "contiki.h"
#include "lib/random.h"
#include "sys/ctimer.h"
#include "sys/etimer.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ipv6/uip-debug.h"
#include "net/routing/routing.h"
#include "simple-udp.h"

#include <stdio.h>
#include <string.h>
#include "sys/energest.h"

#define UDP_PORT 1234
#define SERVICE_ID 190

static struct simple_udp_connection unicast_connection;

uint32_t    hops;
static unsigned long tx, rx, cpu, lpm;

#if defined(CONF_SEND_INTERVAL)& defined(CONF_MESSAGES) && defined(CONF_START_DELAY)
#define WAIT_FOR_END ((CONF_SEND_INTERVAL * CONF_MESSAGES) + CONF_START_DELAY + 163)
#else
#define WAIT_FOR_END   6500
#endif


/*---------------------------------------------------------------------------*/
PROCESS(unicast_receiver_process, "Unicast receiver example process");
AUTOSTART_PROCESSES(&unicast_receiver_process);
/*---------------------------------------------------------------------------*/
static void
receiver(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  hops = uip_ds6_if.cur_hop_limit - UIP_IP_BUF->ttl + 1;

  printf("Data received from ");
  uip_debug_ipaddr_print(sender_addr);
  printf(" on port %d from port %d with length %d: '%s'\n",
         receiver_port, sender_port, datalen, data);
  printf("IN;");
  uip_debug_ipaddr_print(sender_addr); 
  
  printf(";%s;%d\n", data ,hops); 
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(unicast_receiver_process, ev, data)
{
  static struct etimer et;

  PROCESS_BEGIN();

  etimer_set(&et, WAIT_FOR_END*CLOCK_SECOND);

  NETSTACK_ROUTING.root_start();

  simple_udp_register(&unicast_connection, UDP_PORT,
                      NULL, UDP_PORT, receiver);

  while(1) {
    PROCESS_WAIT_EVENT();
    if(ev == PROCESS_EVENT_TIMER) {
      if (etimer_expired(&et)){
        cpu = energest_type_time(ENERGEST_TYPE_CPU);
        lpm = energest_type_time(ENERGEST_TYPE_LPM);
        tx = energest_type_time(ENERGEST_TYPE_TRANSMIT);
        rx = energest_type_time(ENERGEST_TYPE_LISTEN);
        printf("EG;%lu;%lu;%lu;%lu \n",cpu, lpm, rx,tx) ;
      }
    }
     
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/