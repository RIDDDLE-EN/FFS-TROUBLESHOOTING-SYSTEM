# Arduino Framework
This ESP32 code is written in arduino framework using platform io

To run:

1. Create and enter python environment

```bash
sudo apt update && sudo apt install python3-venv
python3 -m venv <path/to/your/custom/environment>
source <path/to/your/custom/environment>
```
2. install platformio
```bash
wget -O get-platformio.py https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py
python3 get-platformio.py
```
3. compile and upload to your board

```bash
pio run -e esp32dev -t upload -p <path/to/the/connected/port>
pio device monitor -p <path/to/the/connected/port>
```
