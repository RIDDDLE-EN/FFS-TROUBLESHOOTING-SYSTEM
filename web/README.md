# WEB APP
This contains the web app that will run on a server. The web is containerized and uses nginx to proxy requests

1. Install and setup docker [Docker install](https://docs.docker.com/engine/install/), [Docker setup](dev.to/s3cloudhub/push-docker-images-to-docker-hub-directly-using-cli-a-complete-guide-2ep0)
2. Configure own domain name

```bash
sudo cp 99-update-hosts /etc/NetworkManager/dispatcher.d/
sudo chmod +x /etc/NetworkManager/dispatcher.d/99-update-hosts
sudo systemctl enable --now NetworkManager-dispatcher.service
nmcli radio wifi off && sleep 3 nmcli radio wifi on
cat /etc/hosts
```
you should see and output at the bottom like

***`your ip address` ffstroubleshootingsystem.com www.ffstroubleshootingsystem.com***

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

To navigate to the web app, in your browser, search yourdomainname.com e.g [ffstroubleshootingsystem.com](http://localhost/)

To stop the containers,

```bash
docker compose down
````
