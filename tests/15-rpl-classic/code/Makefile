all: dis-sender sender-node receiver-node root-node
CONTIKI=../../..

MAKE_MAC = MAKE_MAC_TSCH
MAKE_ROUTING = MAKE_ROUTING_RPL_CLASSIC

MODULES += $(CONTIKI_NG_SERVICES_DIR)/orchestra
ORCHESTRA_EXTRA_RULES += &tvss_oscar

CFLAGS += -DORCHESTRA_CONF_RULES="{&eb_per_time_source,$(ORCHESTRA_EXTRA_RULES),&default_common}"

include $(CONTIKI)/Makefile.include
