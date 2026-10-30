# Pi Zero W radio lab: run your own code on the radio's Bluetooth CPU

The Pi Zero W's radio chip, the BCM43430A1 (Cypress's CYW43438), has two Cortex-M3 CPUs: one runs Bluetooth and one
runs Wi-Fi. This lab starts from bare metal on the Pi, brings up the Bluetooth half, runs our own code on its CPU, and
ends with a C hello world on that CPU that prints back to the Pi. Wi-Fi will follow in the same form.

| Path | What it holds |
|---|---|
| `BLUETOOTH-CRASH-COURSE.md` | the tutorial: start here |
| `BLUETOOTH-HELLO-WORLD.md` | the next step: a hello world in C on the radio's Bluetooth CPU |
| `code-bluetooth/` | their programs: the routines (`pl011.h`, `bt.c`), the driver `1-bt-bringup.c`, the tests `0-`, `2-` and `3-`, `chip.S` for the radio, and the hello world: `start.S`, `hello.ld` and `hello.c` on the radio, `4-bt-hello.c` on the Pi |
| `docs/` | every document the tutorial cites; `docs/SOURCES.md` gives each copy's origin, date and SHA-256 |
| `HISTORY.md` | who did the original Bluetooth and Wi-Fi investigation in December 2025, and when |

The tutorial cites the radio lab's earlier programs, "lab card NN", and the program wifi-id. They are kept in the
lab's repository, not in this one; the tutorial does not need them.

## Build and run

You need an original Pi Zero W (not a Zero 2 W) with the class SD card, `arm-none-eabi-gcc`, `xxd`, the class
bootloader, and `CS340LX_2026_PATH` set to your class checkout: the Makefile builds against its libpi.

```sh
cd code-bluetooth
make                                        # every program, and the radio's images from chip.S and hello.c
my-install 1-bt-bringup.bin                 # bring up Bluetooth and run chip.S on the radio
my-install 4-bt-hello.bin                   # run hello.c on the radio's Bluetooth CPU; it prints back
make check                                  # run all five and compare with the .out files
```

A failure prints a PANIC line and reboots the Pi. The `.out` files hold the expected output, with the varying
bracketed values removed.
