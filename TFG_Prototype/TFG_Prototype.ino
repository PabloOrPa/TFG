////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//    Variables BC66
//
////////////////////////////////////////////////////////////////////////////////////////////////////////

String respuesta = "";
String deviceTAG = "sea001";

String pinSim = "4848";
boolean simOK = false;

String urlServidor = "pruebadeconceptopop.ddns.net";
enum EstadoBC66{
  BC_Registrando,
  BC_Registrado,
  BC_Asignado
};


EstadoBC66 estadoBC = BC_Registrando;
int tiempoAlquiler = 0;
unsigned long inicioAlquiler = 0;
unsigned long finAlquiler = 0;
const int LED = 22;

unsigned long ahora =0;
unsigned long restante = 0;

unsigned long envio = 0;
////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//    Variables GPS
//
////////////////////////////////////////////////////////////////////////////////////////////////////////

String linea = "";

bool tieneFix = false;

double latitud = 0.0;
double longitud = 0.0;

unsigned long ultimoPrint = 0;
/////////////////////////////////
// ESTADO GPS
unsigned long ultimoDatoGPS = 0;
enum EstadoGPS {
  GPS_DESCONECTADO,
  GPS_RECIBIENDO,
  GPS_FIX
};
EstadoGPS estadoGPS = GPS_DESCONECTADO;

unsigned long bateria = 89; // Será un valor fijo para probar funcionamiento

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//    Funcion Setup
//
////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {

  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  Serial.begin(9600);   // Monitor serie PC
  Serial1.begin(9600);  // GPS
  Serial2.begin(9600);  // BC66

  Serial.println("\r\n=== BUSES INICIADOS ===\r\n");

  // Arrancamos el BC66 y comunicamos nuestra disponibilidad al Servidor
    // 1º - Comprobamos comunicación
  while(respuesta.indexOf("OK") < 0){
    respuesta = checkAT();
  }
  Serial.println("Comunicacion establecida con BC66\r\n");
    // 2º - Preparamos SIM
  simOK = false;
  while (!simOK) {
    respuesta = checkSIM();
    preparaSIM(respuesta);
  }
    // 3º - Comprueba conexión operador
  respuesta = checkAttach();
  while (respuesta.indexOf("CGATT: 1") < 1) {
    respuesta = checkAttach();
    Serial.println(respuesta);
  }
  Serial.println("Conexión establecida con Proveedor\r\n");

    // 4º - Iniciamos registro en el servidor
  abreSocketUDP();

  while (estadoBC == BC_Registrando) {
    enviaUDP(false); // Envía REG
    String ack = esperaRespuestaUDP();
    if (ack.startsWith("ACK,")) {
        int nuevoTiempo = ack.substring(4).toInt();
        estadoBC = BC_Asignado;
        if (nuevoTiempo != tiempoAlquiler) {
            Serial.print("Tiempo actualizado: ");
            Serial.println(nuevoTiempo);
            tiempoAlquiler = nuevoTiempo;
            finAlquiler = inicioAlquiler + (unsigned long)tiempoAlquiler * 60UL * 1000UL;
        }
    } else {
        Serial.println("Sin ACK, reintentando...");
        delay(5000);
    }
  }

  cierraSocketUDP();


}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//    Bucle Principal
//
////////////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {

  actualizaLED(); // CONTROL LED TIEMPO RESTANTE

  leerGPS();      // LECTURA GPS CONTINUA


  ///////////////////////////////////////////
  // MUESTRA POSICION CADA 5 SEGUNDOS
  if (millis() - ultimoPrint >= 5000) {

    ultimoPrint = millis();

    Serial.println("====== ESTADO GPS ======");

    switch (estadoGPS) {

      case GPS_DESCONECTADO:
        Serial.println("GPS DESCONECTADO");
        break;

      case GPS_RECIBIENDO:
        Serial.println("GPS RECIBIENDO TRAMAS");
        Serial.println("BUSCANDO SATELITES...");
        break;

      case GPS_FIX:
        Serial.println("GPS FIX OK");

        Serial.print("Latitud: ");
        Serial.println(latitud, 6);

        Serial.print("Longitud: ");
        Serial.println(longitud, 6);
        break;
    }

    Serial.println("========================");
    Serial.println();
  }

  ////////////////////////////////////////////
  // SOLO ENVIA SI HAY FIX

  if (!tieneFix) {
    return;
  }
  
  while(millis() > 30000 + envio){
    ////////////////////////////////////////////
    // 1º - Comprueba comunicación módulo

    Serial.println("Check AT:");
    respuesta = "";

    while (respuesta.indexOf("OK") < 0) {
      respuesta = checkAT();
    }
    Serial.println("AT OK\r");

    ////////////////////////////////////////////
    // 2º - Comprueba SIM

    Serial.println("Check PIN:");
    simOK = false;

    while (!simOK) {
      respuesta = checkSIM();
      preparaSIM(respuesta);
    }

    ////////////////////////////////////////////
    // 3º - Comprueba conexión operador

    respuesta = checkAttach();

    while (respuesta.indexOf("CGATT: 1") < 1) {
      respuesta = checkAttach();
      Serial.println(respuesta);
    }

    Serial.println("IP Obtenida");

    ////////////////////////////////////////////
    // ENVIO UDP CON GPS
    abreSocketUDP();
    enviaUDP(true);
    String ack = esperaRespuestaUDP();
      if (ack.startsWith("ACK,")) {
          int nuevoTiempo = ack.substring(4).toInt();
          estadoBC = BC_Asignado;
          if (nuevoTiempo != tiempoAlquiler) {
              Serial.print("Tiempo actualizado: ");
              Serial.println(nuevoTiempo);
              tiempoAlquiler = nuevoTiempo;
              finAlquiler = inicioAlquiler + (unsigned long)tiempoAlquiler * 60UL * 1000UL;
          }
      }
    cierraSocketUDP();
    envio = millis();
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//    Funciones GPS
//
////////////////////////////////////////////////////////////////////////////////////////////////////////

void leerGPS() {

  // Mientras el GPS esté mandando datos, los leemos
  while (Serial1.available()) {
    
    ultimoDatoGPS = millis();
    char c = Serial1.read();

    if (c == '\n') {
      procesarNMEA(linea);
      linea = "";
    }
    else if (c != '\r') {
      linea += c;
    }
  }

  // 2º - Actualizamos el estado del GPS en función de la respuesta
  if (millis() - ultimoDatoGPS > 5000) {
    // Si hace más de 5 segundos que no llegan datos damos el GPS por desconectado
    estadoGPS = GPS_DESCONECTADO;
  }
  else {
  
    if (tieneFix) {
      estadoGPS = GPS_FIX;  // Si da señal y tiene FIX, todo Ok
    }
    else {
      estadoGPS = GPS_RECIBIENDO; // Si da señal y NO tiene fix, quedamos a la espera.
    }
  }
}

void procesarNMEA(String trama) {

  // Solo nos interesa la linea $GPRMC
  if (!trama.startsWith("$GPRMC")) {
    return;
  }

  // Separamos los campos del mensaje
  String campos[20];
  int indice = 0;
  int inicio = 0;

  for (int i = 0; i < trama.length(); i++) {

    if (trama[i] == ',') {
      campos[indice++] = trama.substring(inicio, i);
      inicio = i + 1;
    }
  }
  campos[indice] = trama.substring(inicio); // Guarda el último campo (al no terminar por , )

  // Campo 2 = estado GPS
  // Si es A -> tiene fix     Si es V -> NO tiene fix
  if (campos[2] != "A") {
    tieneFix = false;
    return;
  }
  // Si tiene fix, tiene una posición válida. La convertimos a grados y minutos a decimal.
  tieneFix = true;

  latitud = convertirCoordenada(campos[3], campos[4]);
  longitud = convertirCoordenada(campos[5], campos[6]);
}

double convertirCoordenada(String valor, String direccion) {

  double coordenada = valor.toFloat();
  
  int grados = (int)(coordenada / 100);
  double minutos = coordenada - (grados * 100);
  
  double decimal = grados + (minutos / 60.0);

  if (direccion == "S" || direccion == "W") {
    decimal *= -1.0;
  }

  return decimal;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//    Funciones BC66
//
////////////////////////////////////////////////////////////////////////////////////////////////////////

String checkAT() {
  Serial2.println("AT");
  String res = esperaComando(300);
  //Serial.println(res);
  return res;
}

String checkSIM() {

  Serial2.println("AT+CPIN?");
  String res = esperaComando(5000);

  return res;
}

void preparaSIM(String estado) {

  if (estado.indexOf("OK") > 0) {

    if (estado.indexOf("READY") > 0) {
      Serial.println("SIM Lista");
      simOK = true;
    }else if (estado.indexOf("SIM PIN") > 0) {
      Serial.println("SIM NO lista, necesita PIN");
      introducePinSim();
    }else if (estado.indexOf("SIM PUK") > 0) {
      Serial.println("SIM requiere PUK");
    }else if (estado.indexOf("NOT READY") > 0) {
      Serial.println("SIM no preparada");
    }else {
      Serial.println("Estado SIM desconocido");
    }
  }else {
    imprimeError("SIM", estado);
  }
}

void introducePinSim() {

  Serial2.print("AT+CPIN=\"");
  Serial2.print(pinSim);
  Serial2.println("\"");
  String res = esperaComando(5000);

  if (res.indexOf("OK") < 0) {
    Serial.println("ERROR introduciendo PIN");
    Serial.print("AT+CPIN=\"");
    Serial.print(pinSim);
    Serial.println("\"");
    Serial.println(res);
  }
}

String checkAttach() {

  Serial2.println("AT+CGATT?");
  String res = esperaComando(85000);

  return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//    ENVIOS UDP
//
////////////////////////////////////////////////////////////////////////////////////////////////////////

void abreSocketUDP(){
  Serial.println("Abriendo socket UDP");
  boolean oki = false;
  while (!oki) {
    Serial2.println("AT+QIOPEN=1,0,\"UDP\",\"" + urlServidor + "\",5000,4000,0");
    String res = "";
    unsigned long inicio = millis();

    while (millis() - inicio < 5000) {
      actualizaLED();
      while (Serial2.available()) {
        char c = Serial2.read();
        res += c;
      }
      // salir antes si ya llegó respuesta útil
      if (res.indexOf("OK") >= 0 ||
          res.indexOf("ERROR") >= 0 ||
          res.indexOf("+QIOPEN:") >= 0) {
        break;
      }
    }

    Serial.println(res);
    if (res.indexOf("OK") >= 0) {
      oki = true;
    }else if(res.indexOf("ERROR") >= 0){
      delay(5000);
    }
  }

  Serial.println("Socket abierto con exito");
}

void cierraSocketUDP(){
  Serial.println("Cerrando socket UDP");
  boolean oki = false;
  while (!oki) {
    Serial2.println("AT+QICLOSE=0");
    String res2 = esperaComando(300);
    if (res2.indexOf("OK") > 0) {
      oki = true;
    }
  }
  Serial.println("Socket cerrado con exito");
}

void enviaUDP(boolean esPosicion) {
  
  // 1º - Construimos el mensaje correspondiente
  String payload = construyeMensaje(esPosicion);
  
  // 2º - Lo enviamos
  Serial.println("Enviando mensaje UDP");
  boolean oki = false;

  while (!oki) {
    Serial.println("Entra al qisend");
    Serial2.print("AT+QISEND=0,");
    Serial2.print(payload.length());
    Serial2.print(",\"");
    Serial2.print(payload);
    Serial2.println("\"");
    String res2 = esperaComando(300);
    Serial.println(res2);
    if (res2.indexOf("SEND OK") >= 0) {
      oki = true;
    }
  }
  Serial.println("Mensaje enviado con exito");

}

String construyeMensaje(boolean posicion){
  // Parte común a todos los mensajes
  String payload = "";
  payload += deviceTAG;
  payload += ",";
  
  if(posicion){
    char latBuffer[16];
    char lonBuffer[16];

    dtostrf(latitud, 10, 6, latBuffer);
    dtostrf(longitud, 11, 6, lonBuffer);
    payload += "POS,";
    payload += latBuffer;
    payload += ",";
    payload += lonBuffer;
    Serial.println("Payload:");
    Serial.println(payload);
  }else{
    payload += "REG,";
    payload += bateria;
  }
  return payload;
}

String esperaRespuestaUDP() {
    String res = "";
    unsigned long inicio = millis();

    // Espera hasta 15s a que llegue el +QIURC por indicaciones del Datasheet
    while (millis() - inicio < 15000) {
      actualizaLED();
      while (Serial2.available()) {
          res += (char)Serial2.read();
      }
      
      if (res.indexOf("+QIURC:") >= 0) {
          Serial.println("QIURC detectado, leyendo datos...");
          
          // Pedimos los datos
          Serial2.println("AT+QIRD=0,100");
          String datos = esperaComando(300);

          Serial.println("=== QIRD ===");
          Serial.println(datos);
          Serial.println("============");

          // Extraemos el payload: buscamos la línea que empieza por "ACK"
          int idx = datos.indexOf("ACK,");
          if (idx >= 0) {
              int fin = datos.indexOf('\n', idx);
              String payload = datos.substring(idx, fin);
              payload.trim();
              return payload;
          }
      }
    }

    Serial.println("Timeout esperando ACK");
    return "";
}
////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//    FUNCIONES AUXILIARES
//
////////////////////////////////////////////////////////////////////////////////////////////////////////

String esperaComando(int tout) {

  String aux = "";
  unsigned long inicio = millis();

  while (millis() - inicio < tout) {
    actualizaLED();
    while (Serial2.available()) {
      char c = Serial2.read();
      aux += c;
    }
  }

  return aux;
}

void actualizaLED() {
    //if (finAlquiler == 0) return;

    ahora = millis();
    restante = (finAlquiler > ahora) ? (finAlquiler - ahora) / 60000UL : 0;
    if (restante >= 10) {
        // Apagado — solo escribe si hace falta
        if (digitalRead(LED) == HIGH) {
            digitalWrite(LED, LOW);
        }

    } else if (restante > 0) {
        // Encendido fijo — solo escribe si hace falta
        if (digitalRead(LED) == LOW) {
            digitalWrite(LED, HIGH);
        }

    } else {
        // Tiempo agotado — parpadeo no bloqueante
        static unsigned long ultimoParpadeo = 0;
        static bool ledState = false;

        if (ahora - ultimoParpadeo >= 500) {
            ultimoParpadeo = ahora;
            ledState = !ledState;
            digitalWrite(LED, ledState ? HIGH : LOW);
        }
    }
}

void imprimeError(String tipo, String estado) {

  Serial.println("####################################");
  Serial.print("Hubo un error de [ ");
  Serial.print(tipo);
  Serial.println(" ]");
  Serial.println(estado);
  Serial.println("####################################");
}