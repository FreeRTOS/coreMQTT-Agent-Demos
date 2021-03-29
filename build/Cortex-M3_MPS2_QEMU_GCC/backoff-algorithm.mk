#Intended to be included from the Makefile.  Builds the utility library that
#implements the algorithm used to back off the time and add randomness to the
#reconnect attempts.

BACKOFF_ALGORITHM_DIR += ./../../lib/FreeRTOS/utilities/backoffAlgorithm/source
VPATH += $(BACKOFF_ALGORITHM_DIR)
INCLUDE_DIRS += -I$(BACKOFF_ALGORITHM_DIR)/include
SOURCE_FILES += $(wildcard $(BACKOFF_ALGORITHM_DIR)/*.c)
