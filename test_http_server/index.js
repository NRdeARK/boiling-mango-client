const express = require("express");
const app = express();
const port = 3000;
app.use(express.json());

app.get("/", (req, res) => {
  res.send("Hello World!");
});

app.post("/registerId", (req, res) => {
    response = { 
        message : "register success!",
        macAddress : req.body.macAddress
    };
    console.log(response)
    res.end(JSON.stringify(response));  
});

app.post("/requestId", (req, res) => {
    response = {
        message : "request success!",
        mqtt_hostname : "192.168.2.252",
        mqtt_port : 1883,
        username : "test1",
        password : "test"
    };
    console.log(response)
    res.end(JSON.stringify(response));  
});

app.listen(port, () => {
  console.log(`Example app listening at http://localhost:${port}`);
});
