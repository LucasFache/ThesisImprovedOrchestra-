all: dis-sender sender-node receiver-node root-node
CONTIKI=..

MAKE_MAC = MAKE_MAC_TSCH
MAKE_ROUTING = MAKE_ROUTING_RPL_CLASSIC

MODULES += os/services/orchestra
#ORCHESTRA_EXTRA_RULES += &tvss_oscar
#ORCHESTRA_EXTRA_RULES += &unicast_per_neighbor_rpl_storing

#CFLAGS += -DORCHESTRA_CONF_RULES="{&eb_per_time_source,$(ORCHESTRA_EXTRA_RULES),&default_common}"

include $(CONTIKI)/Makefile.include
