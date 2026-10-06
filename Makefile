obj-m := pubsub_driver.o
pubsub_driver-objs := main_driver.o list_driver.o
BUILDROOT_DIR := ../..
KDIR := $(BUILDROOT_DIR)/output/build/linux-custom
COMPILER := $(BUILDROOT_DIR)/output/host/bin/i586-buildroot-linux-gnu-gcc

all:
	$(MAKE) -C $(KDIR) M=$$PWD
	$(MAKE) -C $(KDIR) M=$$PWD modules_install INSTALL_MOD_PATH=../../target
	$(COMPILER) -o test_pubsub_driver test_pubsub_driver.c
	$(COMPILER) -o test_pubsub_driver_it test_pubsub_driver_it.c
	cp test_pubsub_driver $(BUILDROOT_DIR)/output/target/bin
	cp test_pubsub_driver_it $(BUILDROOT_DIR)/output/target/bin

test:
	mkdir -p build
	$(CC) -std=gnu11 -Wall -Wextra -Werror -DUNIT_TEST -I. \
		-o build/test_list_driver tests/test_list_driver.c list_driver.c
	./build/test_list_driver

clean:
	rm -f *.o *.ko .*.cmd
	rm -f modules.order
	rm -f Module.symvers
	rm -f pubsub_driver.mod.c
	rm -f test_pubsub_driver
	rm -f test_pubsub_driver_it
	rm -rf build

.PHONY: all clean test
