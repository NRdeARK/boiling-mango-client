# boiling-mango-client
client mqtt on esp32 


To run mqtt server for testing 
```
cd test_server
docker compose -p mqtt5 up -d
```
add new user
```
docker ps
docker exec -it [container-id] sh
mosquitto_passwd -c /mosquitto/config/pwfile [user]
```

Run mqtt web client
```
sudo docker run -d --name mqttx-web -p 80:80 emqx/mqttx-web
```
Thanks sukesh-ak/setup-mosquitto-with-docker