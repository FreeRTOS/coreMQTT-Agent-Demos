#Core files
FREERTOS_TCP_DIR += ./../../lib/FreeRTOS/freertos-plus-tcp

INCLUDE_DIRS += -I$(FREERTOS_TCP_DIR)/include \
				-I$(FREERTOS_TCP_DIR)/portable/Compiler/GCC \
				-I$(FREERTOS_TCP_DIR)/tools/tcp_utilities/include
VPATH += $(FREERTOS_TCP_DIR) $(FREERTOS_TCP_DIR)/portable/BufferManagement $(FREERTOS_TCP_DIR)/tools/tcp_utilities/
SOURCE_FILES += $(wildcard $(FREERTOS_TCP_DIR)/tools/tcp_utilities/*.c)
SOURCE_FILES += $(wildcard $(FREERTOS_TCP_DIR)/*.c)
SOURCE_FILES += $(FREERTOS_TCP_DIR)/portable/BufferManagement/BufferAllocation_2.c


#Ethernet driver files
ETHERNET_DRIVER_DIR += ./../../lib/FreeRTOS/freertos-plus-tcp/portable/NetworkInterface/MPS2_AN385

INCLUDE_DIRS += -I$(ETHERNET_DRIVER_DIR)
VPATH += $(ETHERNET_DRIVER_DIR) $(ETHERNET_DRIVER_DIR)/ether_lan9118
SOURCE_FILES += $(wildcard $(ETHERNET_DRIVER_DIR)/*.c)
SOURCE_FILES += $(wildcard $(ETHERNET_DRIVER_DIR)/ether_lan9118/*.c)
