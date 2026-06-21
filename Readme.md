# Proyecto Redes - Entrenamiento Distribuido con UDP

Este proyecto implementa un esquema base de **entrenamiento distribuido** usando comunicación por **UDP** entre un programa **Master** y varios programas **Slave**.

El objetivo principal del código en C++ es encargarse de la comunicación, fragmentación, envío confiable con ACK/NACK, distribución del dataset, envío de pesos, recepción de resultados y promedio de matrices.
El entrenamiento real de la red neuronal será manejado externamente por Python.

---

# Estructura general del programa

El sistema está dividido en dos programas principales:

```txt
Master.cpp
Slave.cpp
```

## Master

El Master se encarga de:

```txt
1. Abrir el puerto principal 5000.
2. Registrar a los Slaves.
3. Dividir el dataset.
4. Enviar un bloque de datos a cada Slave.
5. Enviar matrices de pesos por batch y por layer.
6. Recibir las matrices actualizadas desde los Slaves.
7. Promediar las matrices recibidas.
8. Actualizar los pesos globales.
9. Enviar un mensaje de cierre a todos los Slaves.
```

## Slave

Cada Slave se encarga de:

```txt
1. Registrarse con el Master.
2. Recibir su bloque de dataset.
3. Esperar mensajes de entrenamiento.
4. Recibir matrices de pesos.
5. Procesar esas matrices.
6. Devolver los pesos actualizados al Master.
7. Terminar cuando recibe el mensaje END.
```

Actualmente, el procesamiento de pesos se realiza con una función de prueba que suma `0.01` a cada peso. Esa parte queda preparada para que luego se conecte con Python.

---

# Protocolo de comunicación UDP

Aunque se usa UDP, se implementó una lógica propia para hacerlo más confiable.
Cada mensaje se divide en paquetes fijos de 500 bytes.

```txt
Tamaño total del paquete: 500 bytes
Header: 20 bytes
Payload: 480 bytes
```

## Estructura del paquete

Cada paquete tiene la siguiente estructura:

```txt
[ HASH ][ CONTROL ][ SEQ_FRAG ][ SEQ_MSG ][ PAYLOAD ]
```

Donde:

```txt
HASH      -> 10 bytes
CONTROL   -> 2 bytes
SEQ_FRAG  -> 4 bytes
SEQ_MSG   -> 4 bytes
PAYLOAD   -> 480 bytes
```

---

# Header del paquete

## HASH

El campo `HASH` contiene un CRC32 calculado sobre el payload completo.

```txt
Tamaño: 10 bytes
Formato: número decimal en ASCII
```

Sirve para verificar si el paquete llegó correctamente.

Si el hash recibido coincide con el hash calculado, el paquete se acepta.
Si no coincide, se rechaza y se responde con NACK.

---

## CONTROL

El campo `CONTROL` indica qué tipo de fragmento es el paquete.

```txt
"01" -> Primer fragmento
"00" -> Fragmento intermedio
"11" -> Último fragmento o mensaje de un solo fragmento
```

Ejemplo:

```txt
Mensaje pequeño:
CONTROL = "11"

Mensaje grande:
Fragmento 0 -> "01"
Fragmento 1 -> "00"
Fragmento 2 -> "00"
Fragmento 3 -> "11"
```

---

## SEQ_FRAG

El campo `SEQ_FRAG` indica el número de fragmento dentro de un mensaje.

```txt
Tamaño: 4 bytes
Ejemplo: 0000, 0001, 0002
```

Sirve para reconstruir los fragmentos en orden.

---

## SEQ_MSG

El campo `SEQ_MSG` identifica el mensaje lógico completo.

```txt
Tamaño: 4 bytes
Ejemplo: 0001, 0002, 0003
```

Sirve para evitar mezclar fragmentos de mensajes distintos.

---

# Payload del paquete

El payload mide 480 bytes.

En el primer fragmento, o en mensajes de un solo paquete, el payload tiene esta estructura:

```txt
[ TYPE ][ DATA_SIZE ][ DATA ]
```

Donde:

```txt
TYPE      -> 1 byte
DATA_SIZE -> 10 bytes
DATA      -> hasta 469 bytes
```

En fragmentos intermedios y finales, el payload contiene directamente más partes del contenido.

---

# Tipos de mensajes usados

El campo `TYPE` indica qué tipo de mensaje se está enviando.

```txt
L -> Login / registro de Slave
B -> Bloque de dataset
P -> Pesos enviados por el Master
R -> Resultado enviado por el Slave
E -> End / finalizar Slave
A -> ACK
N -> NACK
```

---

# Mensaje L - Registro de Slave

El mensaje `L` es enviado por cada Slave al Master para registrarse.

```txt
TYPE = L
DATA = vacío
```

Flujo:

```txt
Slave -> Master: L
Master -> Slave: ACK
```

Cuando el Master recibe el registro, guarda la IP y el puerto temporal del Slave.

Ejemplo:

```txt
[OK]: Slave 1 registered from 127.0.0.1:51856
```

---

# Mensaje B - Bloque de dataset

El mensaje `B` es enviado por el Master a cada Slave.

Sirve para darle a cada Slave una parte del dataset.

Estructura lógica del contenido:

```txt
[ ROWS ][ COLUMNS ][ CSV_BLOCK ]
```

Donde:

```txt
ROWS      -> 6 bytes
COLUMNS   -> 4 bytes
CSV_BLOCK -> datos CSV asignados al Slave
```

Ejemplo:

```txt
Rows: 3
Columns: 4
CSV block size: 41
```

Cada Slave recibe su propio bloque y lo guarda localmente para ser usado después por Python.

---

# Mensaje P - Pesos enviados por el Master

El mensaje `P` contiene una matriz de pesos enviada desde el Master hacia los Slaves.

Estructura lógica:

```txt
[ BATCH_ID ][ LAYER_ID ][ ROWS ][ COLUMNS ][ WEIGHTS_DATA ]
```

Donde:

```txt
BATCH_ID     -> 5 bytes
LAYER_ID     -> 3 bytes
ROWS         -> 6 bytes
COLUMNS      -> 4 bytes
WEIGHTS_DATA -> matriz serializada como texto
```

La matriz se representa como texto usando este formato:

```txt
fila1_col1,fila1_col2;fila2_col1,fila2_col2
```

Ejemplo:

```txt
0.1,0.2;0.3,0.4
```

Representa la matriz:

```txt
0.1  0.2
0.3  0.4
```

---

# Mensaje R - Resultado del Slave

El mensaje `R` es enviado por el Slave al Master después de procesar los pesos.

Tiene la misma estructura lógica que `P`:

```txt
[ BATCH_ID ][ LAYER_ID ][ ROWS ][ COLUMNS ][ UPDATED_WEIGHTS_DATA ]
```

El Master usa `BATCH_ID` y `LAYER_ID` para verificar que el resultado corresponde al batch y layer esperados.

Después, convierte la matriz recibida desde texto a matriz numérica y la usa para el promedio global.

---

# Mensaje E - Finalización

El mensaje `E` sirve para indicar que el entrenamiento terminó.

Contenido lógico:

```txt
END
```

Flujo:

```txt
Master -> Slave: E / "END"
Slave -> Master: ACK
Slave cierra su socket y termina
```

Esto evita que los Slaves se queden esperando mensajes infinitamente después de terminar el entrenamiento.

---

# ACK y NACK

Para hacer más confiable la comunicación sobre UDP, cada paquete recibido se valida con CRC32.

Si el paquete llegó correctamente:

```txt
Se responde ACK
```

Si el paquete llegó corrupto:

```txt
Se responde NACK
```

Tipos usados:

```txt
A -> ACK
N -> NACK
```

El emisor espera una respuesta por cada fragmento enviado.

Si recibe ACK:

```txt
El fragmento se considera enviado correctamente.
```

Si recibe NACK o ocurre timeout:

```txt
El fragmento se retransmite.
```

Cada fragmento tiene un límite de reintentos:

```txt
MAX_RETRIES = 5
```

---

# Fragmentación y reconstrucción

Si un mensaje completo no entra en un solo paquete, se divide en varios fragmentos.

El primer fragmento contiene:

```txt
TYPE + DATA_SIZE + primera parte de DATA
```

Los siguientes fragmentos contienen solo partes de DATA.

La reconstrucción usa:

```txt
SEQ_FRAG para ordenar fragmentos
SEQ_MSG para validar que pertenecen al mismo mensaje
CONTROL para saber cuándo llega el último fragmento
DATA_SIZE para recortar el padding final
```

Cuando llegan todos los fragmentos, el programa reconstruye el mensaje original.

---

# Padding

Todos los paquetes deben medir exactamente 500 bytes.

Por eso, si el payload no llena los 480 bytes, se rellena con padding.

Se usan dos caracteres de padding:

```txt
# -> padding normal
@ -> padding para mensaje de un solo fragmento
```

El tamaño real del mensaje se guarda en `DATA_SIZE`, por eso al reconstruir se eliminan los bytes extra.

---

# Flujo completo del entrenamiento distribuido

El flujo general actual es:

```txt
1. Master abre puerto 5000.
2. Slaves se registran con mensaje L.
3. Master guarda IP y puerto de cada Slave.
4. Master divide dataset.csv.
5. Master envía un bloque B a cada Slave.
6. Cada Slave guarda su bloque local.
7. Master inicializa matrices globales por layer.
8. Master recorre batches.
9. Master recorre layers.
10. Master envía P a todos los Slaves.
11. Cada Slave procesa P.
12. Cada Slave devuelve R.
13. Master recibe todos los R.
14. Master promedia las matrices recibidas.
15. Master actualiza los pesos globales de esa layer.
16. Al terminar todos los batches y layers, Master envía E.
17. Slaves reciben END y terminan.
```

---

# Ciclo por batches y layers

El Master trabaja con esta estructura lógica:

```txt
Para cada batch:
    Para cada layer:
        Enviar pesos actuales a todos los Slaves
        Recibir pesos actualizados
        Promediar resultados
        Actualizar pesos globales
```

Esto permite simular el flujo base de entrenamiento distribuido.

---

# Promedio de pesos

Después de recibir los resultados `R`, el Master promedia las matrices devueltas por los Slaves.

Ejemplo:

```txt
Slave 1 -> 0.11,0.21;0.31,0.41
Slave 2 -> 0.13,0.23;0.33,0.43
Slave 3 -> 0.12,0.22;0.32,0.42
```

El Master calcula:

```txt
Promedio = suma posición por posición / cantidad de resultados válidos
```

Resultado:

```txt
0.12,0.22;0.32,0.42
```

Ese promedio se convierte en los nuevos pesos globales de la layer correspondiente.

---

# Conexión pendiente con Python

El entrenamiento real no se realiza dentro de C++.

Actualmente el Slave usa una función de prueba:

```cpp
Update_Weights_For_Test(Weights)
```

Esta función solo suma `0.01` a cada peso para probar que la comunicación funciona.

La conexión con Python debe reemplazar esa parte.

Ubicación en `Slave.cpp`:

```cpp
//Cambiar por pesos entrenados aqui.

std::vector<std::vector<double>> Updated_Weights = Update_Weights_For_Test(Weights);
```

La idea es reemplazarla por una llamada a Python, por ejemplo:

```cpp
std::vector<std::vector<double>> Updated_Weights = Call_Python_Trainer(
    Weights,
    CSV_Block,
    Message.Batch_ID,
    Message.Layer_ID
);
```

Python se encargará de:

```txt
1. Recibir la matriz de pesos.
2. Usar el bloque de dataset del Slave.
3. Entrenar localmente.
4. Devolver la matriz actualizada.
```

C++ solo se encarga de enviar, recibir, validar y promediar.

---

# Ejecución

Compilar:

```bash
g++ Master.cpp -o Master.exe -pthread
g++ Slave.cpp -o Slave.exe -pthread
```

Ejecutar primero el Master:

```bash
./Master.exe
```

Luego ejecutar la cantidad de Slaves configurada:

```bash
./Slave.exe
./Slave.exe
./Slave.exe
```

La cantidad esperada está definida por:

```cpp
#define NUM_SLAVES 3
```

Si `NUM_SLAVES` es 3, el Master esperará hasta que se registren 3 Slaves.

---

# Estado actual

El sistema ya cuenta con:

```txt
Registro de Slaves
Envío confiable sobre UDP con ACK/NACK
CRC32 para validación de paquetes
Fragmentación y reconstrucción de mensajes
Distribución del dataset
Envío de pesos por batch y layer
Recepción de pesos actualizados
Promedio de pesos en el Master
Finalización ordenada de Slaves con END
Punto preparado para conectar Python
```

Este código funciona como base para integrar entrenamiento real desde Python.
