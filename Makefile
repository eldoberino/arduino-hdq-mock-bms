# Build / upload the HDQ heartbeat sketch.
# Defaults target an Arduino Nano (ATmega328P, old bootloader) on /dev/ttyUSB1.
# Override on the command line, e.g.: make upload PORT=/dev/ttyUSB0

ARDUINO ?= arduino
SKETCH  ?= arduino-hdq-mock-bms.ino
BOARD   ?= arduino:avr:pro:cpu=16MHzatmega328
DEV_BOARD ?= arduino:avr:nano:cpu=atmega328old
PORT    ?= /dev/ttyUSB1

.PHONY: upload verify tests

verify:
	$(ARDUINO) --verify --board $(BOARD) $(SKETCH)

upload:
	$(ARDUINO) --upload --board $(BOARD) --port $(PORT) $(SKETCH)

tests:
	cmake -S . -B build
	cmake --build build
	ctest --test-dir build --output-on-failure
