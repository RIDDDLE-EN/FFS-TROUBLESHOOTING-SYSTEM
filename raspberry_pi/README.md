# RASPBERRY PI ZERO 2W

This code base controls the whole **ffs_module** and **actuators**

Create a systemd service to run after bootup 

update and upgrade the repositories.

```bash

sudo apt update && sudo apt full-upgrade -y

```

install relevant packages

```bash

sudo apt install python3 virtualenv git 
```

clone this repo

```bash

cd ~
git clone https://github.com/RIDDDLE-EN/FFS-TROUBLESHOOTING-SYSTEM/tree/main/raspberry_pi
cd ~/FFS-TROUBLESHOOTING-SYSTEM/raspberry_pi
```

create a python environment

```bash

python3 -m  venv ~/.venv
```

source python3 environment

```bash

source ~/.venv/bin/activate
````

install the requirements

```bash

pip install -r requirements.txt
```

test if the app works

```bash

python3 main.py
```

if the machine works properly, create a systemd service

```bash

sudo nano /etc/systemd/system/machine.service
```

```service

[Unit]
Description=systemd instance for the machine service
After=network.target

[Service]
Users=<username>
Group=www-data
WorkingDirectory=/home/<username>/FFS-TROUBLESHOOTING-SYSTEM/raspberry_pi
Environment="PATH=/home/<username>/.venv/bin"
ExecStart=python3 main.py

[Install]
WantedBy=multi-user.target
```

start and enable machine.service

```bash

sudo systemctl start machine.service
```

> if and errror occurs try this first then start the service

```bash
sudo systemctl daemon-reload
```

```bash
sudo systemctl enable machine.service
sudo systemctl status machine.service
```

make sure that it's working properly

## That's IT
