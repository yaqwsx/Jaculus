# Building runtime

In this document, we will guide you through the compilation of the Jaculus
runtime — the firmware you flash into ESP32 to turn it into a powerful Jaculus
device.

## Prerequisities

Jaculus runtime is developed directly in
[ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/).
Therefore, to build the runtime, you need to setup ESP-IDF development
environment on your computer.

Please follow the [getting started guide for
ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#setting-up-development-environment).
It will guide you through:

- toolchain installation,
- getting ESP-IDF, and
- setting up related tools.

Once you have it, open a terminal in the `runtime` directory. A side note: the
commands below assume ESP-IDF is installed in the default locations. If you
changed them, you have to change the commands accordingly.

First, you have to setup environmental variables into your terminal. This has to
be done for every terminal window you open:

- Linux/MacOS: `. $HOME/esp/esp-idf/export.sh`
- Windows CMD: `%userprofile%\esp\esp-idf\export.bat`
- Windows PowerShell: `.$HOME/esp/esp-idf/export.ps1`

Then you can build the firmware by invoking `idf.py build`. Note that the
initial build might take a while and you need internet connection — several
external dependencies are fetched during the initial build.

Then you can upload the firmware into the microcontroller cia `idf.py flash` and
open a serial terminal via `idf.py monitor`.
