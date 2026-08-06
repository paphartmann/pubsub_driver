# pubsub_driver

A small Linux kernel character device driver that implements an in-kernel publish/subscribe mechanism for processes on the same system. User processes interact with the driver via `/dev/pubsub` and use simple textual commands to subscribe, unsubscribe, publish messages to topics, and fetch messages addressed to them.

Key files
- `main_driver.c` — kernel module (device registration, file operations, command parsing).
- `list_driver.c` / `list_driver.h` — topic and per-process message list management.
- `params.h` — external module parameters (`max_msgs`, `max_msg_size`).
- `test_pubsub_driver.c` — simple test program demonstrating subscribe/publish/fetch.
- `test_pubsub_driver_it.c` — interactive test program.
- `Makefile` — build helpers (expects a kernel build directory via `KDIR` in a Buildroot style environment).

License: GPL (module declared with MODULE_LICENSE("GPL")).

## Features
- Character device accessible at `/dev/pubsub`.
- Topics are maintained in-kernel; each subscribed process gets a circular buffer of messages.
- Simple text command protocol written to the device:
  - `/subscribe <topic>`
  - `/unsubscribe <topic>`
  - `/publish <topic> "message"`
  - `/fetch <topic>`

## Requirements
- Linux system with kernel headers and build environment for the target kernel.
- Building the module typically requires the kernel build directory (KERNEL_SRC or `/lib/modules/$(uname -r)/build`).
- Building test programs requires a user-space C compiler (or the cross-compiler configured in the Makefile).

## Build (recommended)
The repository includes a Makefile that expects a Buildroot-like layout. If you have a standard kernel build directory you can build the module and test programs manually.

Quick local build steps (preferred for development/testing):
```sh
# Build the kernel module against the running kernel headers:
make -C /lib/modules/$(uname -r)/build M=$PWD modules

# Compile the user-space test programs (if Makefile doesn't do it for you):
gcc -o test_pubsub_driver test_pubsub_driver.c
gcc -o test_pubsub_driver_it test_pubsub_driver_it.c
```

If you want to use the included Makefile as-is, make sure the variables `KDIR` and `COMPILER` point to valid kernel build and compiler locations (the Makefile uses Buildroot-style relative paths).

## Install / Load module
Be careful: loading kernel modules requires root and can crash your system if the module is buggy.

```sh
# Build as shown above, then as root:
sudo insmod pubsub_driver.ko  # or use modprobe if installed to the correct module path

# Check dmesg for driver initialization messages:
dmesg | tail -n 20

# Ensure device node exists:
# If udev created /dev/pubsub automatically you can use it directly.
# Otherwise create it (replace MAJOR with the major number printed in dmesg)
sudo mknod /dev/pubsub c <MAJOR> 0
sudo chmod 666 /dev/pubsub
```

You can pass module parameters at load time (defaults shown in code):
- max_msgs (default 5) — number of messages per-process buffer (circular).
- max_msg_size (default 255) — bytes per message.

Example:
```sh
sudo insmod pubsub_driver.ko max_msgs=10 max_msg_size=512
```

Unload:
```sh
sudo rmmod pubsub_driver
```

## Usage (protocol)
Write plain strings to `/dev/pubsub`. Commands are ASCII text beginning with a slash:

- Subscribe to a topic:
  - `/subscribe mytopic`
- Unsubscribe from a topic:
  - `/unsubscribe mytopic`
- Publish a message to a topic (message must include quotes in the test programs; the kernel code locates the first `"`):
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

Or use the provided test program:
```sh
# Build test program then run:
./test_pubsub_driver topic1 topic2
# The program subscribes to topics, forks to publish, waits and fetches messages, then unsubscribes.
```

Interactive test:
```sh
./test_pubsub_driver_it
# Type commands such as:
# /subscribe topic
# /publish topic "Hello"
# /fetch topic
```

## Behavior notes & limitations
- Messages are stored per-subscriber in fixed-size circular buffers. `max_msgs` * `max_msg_size` determines per-subscriber allocation.
- The module uses kernel memory allocations (kmalloc) per subscription and per message buffer; unsubscribing frees this memory.
- The module does not perform extensive input validation — malformed input may cause unexpected behavior.
- Message copying and string handling happen in kernel space; exercise caution and test thoroughly. This driver is intended for learning/demo purposes, not production use.
- There is basic logging via printk; check `dmesg` for kernel-side messages and diagnostics.

## Development pointers
- Key symbols:
  - add_process_to_topic, rem_process_from_topic, publish_to_topic, fetch_from_process (defined in `list_driver.c`/`list_driver.h`).
  - Device operations (open/read/write/release) are in `main_driver.c`.
- To add features: consider safer parsing, length checks, per-topic locking (spinlocks) for concurrency, and clearer user-space protocol framing.
- Tests: the repository contains `test_pubsub_driver.c` (automated simple scenario) and `test_pubsub_driver_it.c` (interactive).

## Contributing
- Open issues or PRs for bug fixes, clearer input parsing, concurrency hardening, or feature requests.
- If adding tests or CI, keep kernel build steps isolated — building kernel modules in CI requires special setup or cross-building.

## Security / Safety
- Running and testing kernel modules requires root. A faulty module can crash or hang the system — test in a VM or containerized environment where possible.
- Avoid loading on production machines.

## Contact / Author
Repository author: paphartmann

---
