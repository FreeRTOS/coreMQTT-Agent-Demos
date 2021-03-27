#backoff algorithm files
BACKOFF_ALGORITHM_DIR += ./../../lib/FreeRTOS/utilities/backoffAlgorithm/source
VPATH += $(BACKOFF_ALGORITHM_DIR)
INCLUDE_DIRS += -I$(BACKOFF_ALGORITHM_DIR)/include
SOURCE_FILES += $(wildcard $(BACKOFF_ALGORITHM_DIR)/*.c)
