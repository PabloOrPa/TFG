const sqlite3 = require("sqlite3").verbose();

const db = new sqlite3.Database("./data/locations.db");

db.serialize(() => {

    db.run(`
        CREATE TABLE IF NOT EXISTS pulsera (
            idPulsera INTEGER PRIMARY KEY AUTOINCREMENT,
            idDispositivo TEXT UNIQUE,
            bateria INTEGER,
            encendida BOOLEAN
        )
    `);

    db.run(`
        CREATE TABLE IF NOT EXISTS alquiler (
            idAlquiler INTEGER PRIMARY KEY AUTOINCREMENT,
            cliente TEXT,
            tipoAlquiler TEXT,
            activo INTEGER DEFAULT 1,
            fechaInicio DATETIME DEFAULT CURRENT_TIMESTAMP,
            fechaFin DATETIME
        )    
    `);

    db.run(`
        CREATE TABLE IF NOT EXISTS pulseraXAlquiler (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            idPulsera INTEGER,
            idAlquiler INTEGER,
            lat REAL,
            lng REAL,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    `);

    db.run(`
        CREATE TABLE IF NOT EXISTS msgPendientes (
            idMsg INTEGER PRIMARY KEY AUTOINCREMENT,
            idDispositivo TEXT,
            modoEmergencia INTEGER,
            confirmado INTEGER DEFAULT 0,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    `);
});



module.exports = db;