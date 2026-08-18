# WEB APP
This contains the web app that will run on a server. The web is containerized and uses nginx to proxy requests

1. Install and setup docker [Docker install](https://docs.docker.com/engine/install/), [Docker setup](dev.to/s3cloudhub/push-docker-images-to-docker-hub-directly-using-cli-a-complete-guide-2ep0)
2. Configure own domain name
- if web will be running on an independent server on the same network:

> move the update hosts script to the dispatcher service and add your pi IP address in the *~/FFS-TROUBLESHOOTINGS-SYSTEM/web/app/.env*

> you can get your pi IP by running

>> ip -4 -o addr show <your interface e.g wlo1> | awk '{print $4}' | cut -d / -f1

```bash
sudo cp 99-update-hosts /etc/NetworkManager/dispatcher.d/
sudo chmod +x /etc/NetworkManager/dispatcher.d/99-update-hosts
echo "PI=<your pi IP>" >> ./app/.env
```

- If the web will be hosted in the same raspberry pi, use this 99-update-hosts:

```bash
sudo cp 99-update-hosts-2 /etc/NetworkManager/dispatcher.d/
sudo chmod +x /etc/NetworkManager/dispatcher.d/99-update-hosts-2
```

``` bash
sudo systemctl enable --now NetworkManager-dispatcher.service
sudo nmcli device reapply wlo1 
nmcli radio wifi off && sleep 3 nmcli radio wifi on
cat /etc/hosts
```
you should see an output at the bottom like this

> ***`your ip address` ffstroubleshootingsystem.com www.ffstroubleshootingsystem.com***

```bash
cat ./app/.env
```

you should see an outpur at the bottom like this

> PI=`<your pi IP address>`

3. Configure your firewall

```bash
sudo ufw allow 'Nginx Full'
```
4. Write and .env file to store these environment parameters: port, docker username, container name and container tag

**e.g**
```bash
nano ./app/.env
```

```env
PORT=8000
DOCKER_USERNAME=<your docker username>
CONTAINER_NAME=<custom container name>
TAG=<custom container tag e.g latest>
````

5. Build docker container

```bash
docker compose --env-file ./app/.env up -d --build
```

This builds and runs the containers i.e for the web app and the nginx container

You can check containers that are running using 

```bash
docker ps
```

To navigate to the web app, in your browser, search yourdomainname.com e.g [ffstroubleshootingsystem.com](http://ffstroubleshootingsystem.com/)

To stop the containers,

```bash
docker compose down
````
