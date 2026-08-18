# RASPBERRY PI ZERO 2W

This code base controls the whole **ffs_module** and **actuators**

Create a systemd service to run after bootup 

Update and upgrade the repositories.

```bash
sudo apt update && sudo apt full-upgrade -y
```

Install relevant packages

```bash
sudo apt install python3-venv git 
```

Clone this repo into your raspberry pi

```bash
cd ~
git clone git@github.com:RIDDDLE-EN/FFS-TROUBLESHOOTING-SYSTEM.git
cd ~/FFS-TROUBLESHOOTING-SYSTEM/raspberry_pi
```

Create a python environment

```bash
python3 -m  venv ~/.venv
```

Source python3 environment

```bash
source ~/.venv/bin/activate
````

Install the requirements

```bash
pip install -r requirements.txt
```

Test if the app works

```bash
python3 main.py
```

If the machine works properly, create a systemd service

```bash
deactivate
sudo mv ./machine.service /etc/systemd/system/
sudo chmod 755 ~
sudo systemctl daemon-reload
sudo systemctl enable --now machine.service
sudo systemctl start machine.service NetworkManager
```
Confirm that the machine systemd service is running

```bash
sudo systemctl status machine.service
```

Allow port 5001 in ufw so that it may communicate with the web app

```bash
sudo apt update && install ufw
sudo ufw enable
sudo ufw allow 5001/tcp
sudo ufw status
```

Make sure that it's working properly

To follow up the logs

```bash
sudo journalctl -u machine.service -f
```
To see 10 lines of logs

```bash
sudo journalctl -u machine.service -n 10
```

## That's IT
