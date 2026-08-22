# Target hardware configuration settings
BOARD = rp2040:rp2040:adafruit_feather_rfm

# Fallback target node if no node= parameter is specified
node ?= central_control
SKETCH = $(node)/$(node).ino
BUILD_DIR = $(node)/build
STAMP = $(BUILD_DIR)/.compiled

# Serial settings
BAUD ?= 115200

# Dynamically find the connected serial port for the target board.
# Falls back to /dev/ttyACM0 since picotool doesn't need a real port,
# and the board won't identify by name while sitting in BOOTSEL mode.
PORT := $(shell arduino-cli board list | grep "adafruit_feather_rfm" | awk '{print $$1}')
ifeq ($(PORT),)
PORT := /dev/ttyACM0
endif

# All source files that should trigger a recompile when changed
SOURCES := $(wildcard $(node)/*.ino $(node)/*.c $(node)/*.cpp $(node)/*.h $(node)/*.hpp common/*.c common/*.cpp common/*.h common/*.hpp)

.PHONY: all upload flash clean setup-deps monitor list size force-compile

all: compile

# Compiles only if sources changed since last successful build
compile: $(STAMP)

LIBFLAGS = --library $(CURDIR)/common

# Optimization overrides to inject -O3 flag
O3_FLAGS = --build-property "compiler.c.extra_flags=-O3" --build-property "compiler.cpp.extra_flags=-O3"

# Node address for receiver-nonlatching: make node=receiver-nonlatching FOUNTAIN=2
FOUNTAIN ?= 1
ifeq ($(strip $(FOUNTAIN)),2)
FOUNTAIN_DEFINE = NODE_FOUNTAIN2
else
FOUNTAIN_DEFINE = NODE_FOUNTAIN1
endif
# Appended after O3_FLAGS so the last --build-property wins. Both .c and .cpp
# extra_flags get -O3 plus -DFOUNTAIN_NODE_ID so any language sees the define.
FOUNTAIN_FLAGS = --build-property "compiler.c.extra_flags=-O3 -DFOUNTAIN_NODE_ID=$(FOUNTAIN_DEFINE)" \
                 --build-property "compiler.cpp.extra_flags=-O3 -DFOUNTAIN_NODE_ID=$(FOUNTAIN_DEFINE)"

$(STAMP): $(SOURCES)
	@if [ ! -d "$(node)" ] || [ ! -f "$(SKETCH)" ]; then \
		echo "ERROR: Target directory or sketch file '$(SKETCH)' does not exist!"; \
		exit 1; \
	fi
	arduino-cli compile --fqbn $(BOARD) --build-path $(BUILD_DIR) $(LIBFLAGS) $(O3_FLAGS) $(FOUNTAIN_FLAGS) --only-compilation-database $(SKETCH)
	@cp $(BUILD_DIR)/compile_commands.json .
	arduino-cli compile --fqbn $(BOARD) --build-path $(BUILD_DIR) $(LIBFLAGS) $(O3_FLAGS) $(FOUNTAIN_FLAGS) $(SKETCH)
	@touch $(STAMP)

# Force a recompile regardless of timestamps
force-compile:
	@rm -f $(STAMP)
	@$(MAKE) compile

# Uploads the specific built binary output to the connected hardware
upload:
	arduino-cli upload -p $(PORT) --fqbn $(BOARD) --upload-property upload.tool=picotool --input-dir $(BUILD_DIR) $(SKETCH)

# Compile then upload in one step (compile only recompiles if needed)
flash: compile upload

# Opens a serial monitor to the board at BAUD (default 115200)
monitor:
	arduino-cli monitor -p $(PORT) -c baudrate=$(BAUD)

# Lists connected boards/ports as arduino-cli sees them, useful for debugging
list:
	arduino-cli board list

# Wipes the hidden workspace caches and tracking paths
clean:
	rm -rf $(node)/build/
	rm -f compile_commands.json

# Installs global structural dependencies for your environment setup
# (arduino-cli must be on PATH first; see README/install notes)
setup-deps:
	arduino-cli config init
	arduino-cli config add board_manager.additional_urls https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
	arduino-cli core update-index
	arduino-cli core install rp2040:rp2040
	arduino-cli lib update-index
	arduino-cli lib install "Adafruit NeoPixel" "Adafruit GFX Library" "Adafruit SH110X" "Adafruit BusIO" "_RFM69" "SPIFlash_LowPowerLab" "RTClib"
