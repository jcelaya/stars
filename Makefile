CONFIG ?= Debug
BUILD_DIR ?= build
MAKEFILE_PATH := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))

all: $(BUILD_DIR)/$(CONFIG)/Makefile
	$(MAKE) -C $(BUILD_DIR)/$(CONFIG)

$(BUILD_DIR)/$(CONFIG)/Makefile: $(BUILD_DIR)/$(CONFIG)
	cmake -E chdir $(BUILD_DIR)/$(CONFIG) cmake -G "Unix Makefiles" $(MAKEFILE_PATH) -DCMAKE_BUILD_TYPE:STRING=$(CONFIG)

$(BUILD_DIR)/$(CONFIG):
	mkdir -p $(BUILD_DIR)/$(CONFIG)

.PHONY: clean

clean:
	rm -fr $(BUILD_DIR)/$(CONFIG)
	
