"use strict";

/*
=====================================    IMPORTS    =====================================
*/

const express = require("express");     // Sirve web y API
const dgram = require("dgram");         // UDP nativo de NodeJS
const WebSocket = require("ws");        // Permite actualizar el mapa sin refrescar
const morgan = require("morgan");       // Para HTTP logger
const db = require("./database");       // ruta de la BBDD
const dbService = require("./services/dbService") // Consultas a la BBDD
/*
=====================================    CONFIG    =====================================
*/

const HTTP_PORT = 8000;
const UDP_PORT = 5000;

const app = express();      // Crea la aplicación web

/*
=====================================    MIDDLEWARE    =====================================
*/

app.use(morgan("dev"));             // Para los logs
app.use(express.static("public"));  // Directorio de los archivos servidos por la web
app.use(express.json());            // Para recibir JSON por http (hace falta realmente?????)

/*
=====================================    HTTP SERVER    =====================================
*/

const server = app.listen(HTTP_PORT, "0.0.0.0", () => {
    console.log(`🌍 HTTP → http://localhost:${HTTP_PORT}`);
});
/*
=====================================    HTTP API    =====================================
*/
app.post("/api/alquiler/iniciar", async (req, res) => {
    try {
        const { servicio } = req.body;
        await iniciaAlquiler(servicio);
        res.json({
            ok: true
        });

    } catch (err) {
        console.error(err);
        res.status(500).json({
            ok: false,
            error: "Error iniciando alquiler"
        });
    }
});

app.get("/api/devices/available", async (req, res) => {
    try {
        const devices = await dbService.getAvailableDevices();
        res.json(devices);

    } catch (err) {
        console.error(err);
        res.status(500).json({
            error: "Error obteniendo dispositivos"
        });
    }
});
/*
=====================================    WEBSOCKET PARA MAPA    =====================================
*/

const wss = new WebSocket.Server({ server });
const clients = new Set();

// Guarda cada conexión de cliente en un Set
wss.on("connection", ws => {

    console.log("🟢 Web client connected");
    clients.add(ws);

    ws.on("close", () => {
        clients.delete(ws);
        console.log("🔴 Web client disconnected");
    });
});

// Esta función es la que se encarga de actualizar el mapa en tiempo real
// a todos los clientes conectados y registrados en dicho Set
function broadcast(data) {

    const payload = JSON.stringify(data);

    for (const client of clients) {
        if (client.readyState === WebSocket.OPEN) {
            client.send(payload);
        }
    }
}

/*
=====================================    UDP SERVER    =====================================
*/

const udp = dgram.createSocket("udp4");
udp.on("message", handleUDPMessage);

udp.bind(UDP_PORT, "0.0.0.0", () => {
    console.log(`📡 UDP escuchando ${UDP_PORT}`);
});

// Handler de entrada
async function handleUDPMessage(msg, rinfo){
    const paquete = parseaPaquete(msg);

    if(!paquete){
        console.log("Paquete UDP no válido");
        return;
    }

    if(paquete.tipo === "1"){
        await handleRegistro(paquete, rinfo);
    }else if(paquete.tipo === "2"){
        await handlePosicion(paquete);
    }else{
        console.log("Tipo de paquete desconocido: ", paquete.tipo);
    }
}

// Si el msg es de registro, el servidor pone como activa la pulsera
async function handleRegistro(paquete, rinfo){
    try{
        const device = paquete.device;
        const bateria = paquete.payload[0];
        // La forma de un registro es: TIPO, DEVICE, bateria
        await dbService.registrarPulsera(device, bateria);

    } catch (err){
        console.error("ERROR de REGISTRO: ", err)
    }    
    
}

// Si el msg es de posicion, el servidor registra latitud y longitud
async function handlePosicion(paquete){
    const idPulsera = await dbService.getPulsera(paquete.device);
    const idAlquiler = await dbService.getAlquilerActivo(idPulsera);
    const lat = paquete[0];
    const lng = paquete[1];
    await dbService.guardarPosicion(idPulsera, idAlquiler, lat, lng);
}




function parseaPaquete(msg){
    try{
        const text = msg.toString().trim();
        const parts = text.split(",");
        return{
            device: parts[0],
            tipo: parts[1],
            payload: parts.slice(2)
        }
    } catch {
        return null;
    }
}

// Funciones para envío de mensaje UDP
function sendUDP(rinfo, msg){
    const buffer = Buffer.from(msg);

    udp.send(buffer, 
        rinfo.port, 
        rinfo.address, 
        err => {if(err) console.error("UDP send error:",err)});
}

function sendToDevice(rinfo, device, message) {

    const info = rinfo; 

    if (!info) {
        console.log("Device no conectado");
        return;
    }

    sendUDP(info, message);
}
/*
=====================================    Logica de negocio    =====================================
*/
async function iniciaAlquiler(servicio){

    console.log("Iniciando alquiler:", servicio);

    // lógica real
}
/*
=====================================    CIERRE SERVIDOR    =====================================
*/
// Cierra todos los servicios correctamente al pulsar Ctrl+C
process.on("SIGINT", () => {
    console.log("\n🛑 Cerrando servidor...");
    udp.close();
    server.close();
    db.close();
    process.exit(0);
});


