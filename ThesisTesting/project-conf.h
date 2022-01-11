/*
 * Copyright (c) 2016, Inria.
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
#define TCPIP_CONF_ANNOTATE_TRANSMISSIONS 1
#define LOG_CONF_LEVEL_RPL LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_MAC LOG_LEVEL_DBG

#define ENERGEST_CONF_ON 1

#define CONF_SEND_INTERVAL    6
#define CONF_MESSAGES         50
#define CONF_START_DELAY      240

#define RPL_CONF_WITH_STORING 1

/**********************************************************************/
/*******   configuration for time varying slotframe scheduling   ******/

//#define ALICE_TSCH_CALLBACK_SLOTFRAME_START alice_callback_slotframe_start //alice time varying slotframe schedule

#define OSCAR_OPTIMIZED_SCHEDULING 1

//Using the optimized scheduling (base on OSCAR)
//#define OPTIMIZED_SCHEDULING    1

#define ALICE_UNICAST_SF_ID     2 //slotframe handle of unicast slotframe

//#define LOG_CONF_LEVEL_RPL LOG_LEVEL_DBG

#define LOG_CONF_WITH_ANNOTATE  1 //show RPL tree

/**********************************************************************/
