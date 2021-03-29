#Intended to be included from the Makefile.  Builds the MQTT agent files along
#with the appropriate interface, as well as the coreMQTT library itself.


#Core files
COREMQTT_AGENT_DIR += ./../../lib/FreeRTOS/coreMQTT-Agent/source
COREMQTT_DIR = $(COREMQTT_AGENT_DIR)/dependency/coreMQTT/source

INCLUDE_DIRS += -I$(COREMQTT_AGENT_DIR)/include \
				-I$(COREMQTT_DIR)/include \
				-I$(COREMQTT_DIR)/interface

VPATH += $(COREMQTT_AGENT_DIR) $(COREMQTT_AGENT_DIR)/portable/freertos $(COREMQTT_DIR)
SOURCE_FILES += $(wildcard $(COREMQTT_AGENT_DIR)/*.c)
SOURCE_FILES += $(wildcard $(COREMQTT_AGENT_DIR)/portable/freertos/*.c)
SOURCE_FILES += $(wildcard $(COREMQTT_DIR)/*.c)

#MQTT agent interface
MQTT_AGENT_INTERFACE_DIR += ./../../lib/FreeRTOS/mqtt-agent-interface
INCLUDE_DIRS += -I$(MQTT_AGENT_INTERFACE_DIR)
VPATH += $(MQTT_AGENT_INTERFACE_DIR)
SOURCE_FILES += $(wildcard $(MQTT_AGENT_INTERFACE_DIR)/*.c)
