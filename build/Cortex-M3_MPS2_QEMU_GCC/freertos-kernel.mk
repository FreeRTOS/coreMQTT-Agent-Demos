KERNEL_DIR += ./../../lib/FreeRTOS/freertos-kernel
INCLUDE_DIRS += -I$(KERNEL_DIR)/include \
				-I$(KERNEL_DIR)/portable/GCC/ARM_CM3
VPATH += $(KERNEL_DIR) $(KERNEL_DIR)/portable/GCC/ARM_CM3 $(KERNEL_DIR)/portable/MemMang
SOURCE_FILES += $(KERNEL_DIR)/tasks.c
SOURCE_FILES += $(KERNEL_DIR)/list.c
SOURCE_FILES += $(KERNEL_DIR)/queue.c
SOURCE_FILES += $(KERNEL_DIR)/timers.c
SOURCE_FILES += $(KERNEL_DIR)/event_groups.c
SOURCE_FILES += $(KERNEL_DIR)/stream_buffer.c
SOURCE_FILES += $(KERNEL_DIR)/portable/MemMang/heap_4.c
SOURCE_FILES += $(KERNEL_DIR)/portable/GCC/ARM_CM3/port.c
