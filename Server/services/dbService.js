function getPulsera(device) {
    return new Promise((resolve, reject) => {

        db.get(
            `SELECT * FROM pulsera WHERE idDispositivo = ?`,
            [device],
            (err, row) => {
                if (err) reject(err);
                else resolve(row);
            }
        );

    });
}

function getPulserasActivasLibres() {
    return new Promise((resolve, reject) => {

        db.all(
            `SELECT * FROM pulsera WHERE encendida = 1`,
            (err, rows) => {
                if (err) reject(err);
                else resolve(rows);
            }
        );
    });
}


function getAlquilerActivo(idPulsera) {
    return new Promise((resolve, reject) => {

        db.get(`
            SELECT a.*
            FROM alquiler a
            JOIN pulseraXAlquiler pxa
            ON a.idAlquiler = pxa.idAlquiler
            WHERE pxa.idPulsera = ?
            AND a.activo = 1
            ORDER BY a.fechaInicio DESC
            LIMIT 1
        `,
        [idPulsera],
        (err, row) => {
            if (err) reject(err);
            else resolve(row);
        });

    });
}

function guardarPosicion(idPulsera, idAlquiler, lat, lng) {
    return new Promise((resolve, reject) => {

        db.run(
            `INSERT INTO pulseraXAlquiler
            (idPulsera, idAlquiler, lat, lng)
            VALUES (?,?,?,?)`,
            [idPulsera, idAlquiler, lat, lng],
            err => {
                if (err) reject(err);
                else resolve();
            }
        );

    });
}

function getMensajePendiente(device) {
    return new Promise((resolve, reject) => {

        db.get(`
            SELECT *
            FROM msgPendientes
            WHERE idDispositivo = ?
            AND confirmado = 0
            ORDER BY timestamp ASC
            LIMIT 1
        `,
        [device],
        (err, row) => {
            if (err) reject(err);
            else resolve(row);
        });

    });
}

function confirmarMensaje(idMsg) {
    return new Promise((resolve, reject) => {

        db.run(
            `UPDATE msgPendientes
             SET confirmado = 1
             WHERE idMsg = ?`,
            [idMsg],
            err => {
                if (err) reject(err);
                else resolve();
            }
        );

    });
}

function registrarPulsera(device, bateria) {
    return new Promise((resolve, reject) => {

        db.get(
            `SELECT idPulsera FROM pulsera WHERE idDispositivo = ?`,
            [device],
            (err, row) => {

                if (err) return reject(err);

                // Si ya existe, se pone activa
                if (row) {

                    db.run(`
                        UPDATE pulsera
                        SET encendida = 1,
                        WHERE idDispositivo = ?
                    `, [device]);

                    return resolve(row.idPulsera);
                }

                // Si no existe, la inserta en la tabla de datos
                db.run(`
                    INSERT INTO pulsera
                    (idDispositivo, bateria, encendida)
                    VALUES (?, ?, 1)
                `,
                [device, bateria],
                function(err) {
                    if (err) reject(err);
                    else resolve(this.lastID);
                });

            }
        );
    });
}

module.exports = {
    getPulsera,
    getAlquilerActivo,
    guardarPosicion,
    getMensajePendiente,
    confirmarMensaje,
    registrarPulsera
};