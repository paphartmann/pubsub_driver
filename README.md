# pubsub_driver

A small Linux kernel character device driver that implements an in-kernel publish/subscribe mechanism for processes on the same system. User processes interact with the driver via `/dev/pubsub` and use simple textual commands to subscribe, unsubscribe, publish messages to topics, and fetch messages addressed to them.

Key files
- `main_driver.c` — kernel module (device registration, file operations, command parsing).
- `list_driver.c` / `list_driver.h` — topic and per-process message list management.
- `params.h` — external module parameters (`max_msgs`, `max_msg_size`).
- `test_pubsub_driver.c` — simple test program demonstrating subscribe/publish/fetch.
- `Makefile` — build helpers (expects a Buildroot environment; variables at the top of the Makefile point to the Buildroot layout).

## Features
- Character device accessible at `/dev/pubsub`.
- Topics are maintained in-kernel; each subscribed process gets a circular buffer of messages.
- Simple text command protocol written to the device:
  - `/subscribe <topic>`
  - `/unsubscribe <topic>`
  - `/publish <topic> "message"`
  - `/fetch <topic>`

## Intended environment
This project is intended to be built and installed inside a Buildroot-style build environment. The included Makefile assumes a Buildroot output directory layout (the Makefile defines `BUILDROOT_DIR := ../..` and derives `KDIR` and `COMPILER` relative to that). The Makefile will build the kernel module against the kernel build tree and copy the test binaries into Buildroot's `output/target/bin`.

## Requirements
- Buildroot checkout or an equivalent directory with a kernel build tree and cross-toolchain output (the Makefile expects `$(BUILDROOT_DIR)/output/build/linux-custom` and `$(BUILDROOT_DIR)/output/host/bin/<triplet>-gcc` by default).
- The kernel build used by Buildroot must match the target kernel for which the module is built (module kernel version and config must be compatible).
- Root privileges on the target (or a VM) to insert the module and create device nodes for testing.

## Build (Buildroot-aware)
The provided Makefile is designed to be used from within a Buildroot directory layout. You can either use the Makefile as-is or override variables on the command line.

From the repository root, using a Buildroot checkout at `/path/to/buildroot`:

```sh
# Build the kernel module and test binaries and install them into Buildroot's output
make BUILDROOT_DIR=/path/to/buildroot
```

What the Makefile does (summary):
- Uses `KDIR := $(BUILDROOT_DIR)/output/build/linux-custom` as the kernel build directory and runs `make -C $(KDIR) M=$$PWD` to build the module.
- Runs `modules_install INSTALL_MOD_PATH=../../target` to install built modules into the Buildroot target directory.
- Compiles `test_pubsub_driver` with the cross-compiler referenced by `COMPILER` and copies it to `$(BUILDROOT_DIR)/output/target/bin`.

If you prefer to build only on the host for quick development (native build of test programs and module against your running kernel headers):

```sh
# Build the module against the running kernel (development/testing only)
make -C /lib/modules/$(uname -r)/build M=$PWD modules

# Build the user-space test natively
gcc -o test_pubsub_driver test_pubsub_driver.c
```

To use a different toolchain or kernel build path when using the repository Makefile, override variables:

```sh
make BUILDROOT_DIR=/path/to/buildroot COMPILER=/path/to/host/bin/<triplet>-gcc KDIR=/path/to/kernel/build
```

## Install / Load module on the target
After Buildroot has installed the module into `output/target`, deploy the built target filesystem (or boot the generated image). On the running target, as root:

```sh
# If the module is already on the target at /lib/modules/<version>/...
insmod /lib/modules/<version>/kernel/drivers/<path>/pubsub_driver.ko
# or, if you have the .ko locally on the target filesystem root:
sudo insmod pubsub_driver.ko

# Check kernel logs
dmesg | tail -n 20

# Create device node if udev did not create it (replace MAJOR with the major number from dmesg)
sudo mknod /dev/pubsub c <MAJOR> 0
sudo chmod 666 /dev/pubsub
```

Important Buildroot notes:
- Cross-built kernel modules must be built against the same kernel sources and configuration used to build the target kernel in Buildroot. If the kernel version or config differ, the module may not load on the device.
- The Makefile's default `COMPILER` points to Buildroot's host compiler (`output/host/bin/i586-buildroot-linux-gnu-gcc`). Override `COMPILER` if you need a different cross-compiler or want to build tests natively.

## Usage (protocol)
Write plain strings to `/dev/pubsub`. Commands are ASCII text beginning with a slash:

- Subscribe to a topic:
  - `/subscribe mytopic`
- Unsubscribe from a topic:
  - `/unsubscribe mytopic`
- Publish a message to a topic (message must be enclosed in double quotes; the kernel strips the surrounding quotes and stores only the payload between them):
  - `/publish mytopic "Hello world"`
- Tell the driver which topic to fetch from:
  - `/fetch mytopic`
- Reading from `/dev/pubsub` returns the next message for the calling process for the topic the process selected with `/fetch`.

Example user flow (manual):
```sh
# subscribe to "news"
printf "/subscribe news" > /dev/pubsub

# publish to "news" (from another process)
printf "/publish news \"Breaking update\"" > /dev/pubsub

# tell device you want to fetch from "news"
printf "/fetch news" > /dev/pubsub

# read the message
head -c 255 < /dev/pubsub
```

Or use the provided test program (on the target device or in a chroot of the target rootfs):

```sh
# Run on the target or inside the target rootfs where the test binaries were installed
./test_pubsub_driver topic1 topic2
```

## Behavior notes & limitations
- Messages are stored per-subscriber in fixed-size circular buffers. `max_msgs` * `max_msg_size` determines per-subscriber allocation.
- The module uses kernel memory allocations (kmalloc) per subscription and per message buffer; unsubscribing frees this memory.
- Empty topics are removed when their last subscriber unsubscribes or is otherwise removed.
- Device writes are limited to 4096 bytes, topic names to 63 bytes, and malformed commands are rejected. A global mutex serializes access to topic, subscriber, and message-queue state. However, fetched messages are copied to userspace after the mutex is released, so concurrent operations are not fully protected against message-buffer changes.
- Message copying and string handling happen in kernel space; exercise caution and test thoroughly. This driver is intended for learning/demo purposes, not production use.
- There is basic logging via printk; check `dmesg` for kernel-side messages and diagnostics.

## Development pointers
- Run the host-side automated tests with `make test`. These exercise the topic and
  subscriber/message-buffer logic without loading the kernel module, so they run
  without a VM or root privileges. They do not replace integration testing of the
  device interface or kernel-specific behavior.
- Key symbols:
  - add_process_to_topic, rem_process_from_topic, publish_to_topic, fetch_from_process (defined in `list_driver.c`/`list_driver.h`).
  - Device operations (open/read/write/release) are in `main_driver.c`.
- To add features: consider safer parsing, length checks, protecting fetched-message access across the userspace copy, and clearer user-space protocol framing.
- Tests: `make test` runs host-side unit tests; `test_pubsub_driver.c` is a device-level scenario for validating the user-space interface.

## Security / Safety
- Running and testing kernel modules requires root. A faulty module can crash or hang the system — test in a VM or Buildroot-generated VM/image where possible.
- Avoid loading on production machines.

## Contact / Author
Repository author: paphartmann
