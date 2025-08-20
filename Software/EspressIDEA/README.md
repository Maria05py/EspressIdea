# EspressIDEA

EspressIDEA es un editor web ligero diseñado para programar placas que
ejecutan **MicroPython** o **CircuitPython**.\
El proyecto utiliza un **ESP32** como intermediario entre el navegador y
el dispositivo Python, permitiendo:

-   Acceso directo al REPL.
-   Exploración, creación, modificación y eliminación de archivos.
-   Ejecución y detención de código Python.
-   Interacción en vivo con el terminal, directamente desde una interfaz
    web.
-   Integración opcional con un servidor de IA para generación de
    código.

------------------------------------------------------------------------

## Características principales

-   Comunicación bidireccional en tiempo real (navegador ↔ ESP32 ↔ placa
    Python).
-   Editor web embebido servido desde el ESP32 (SPIFFS).
-   Sistema de archivos remoto: lectura, escritura, subida, descarga y
    borrado.
-   WebSocket de terminal para acceder al REPL como si fuera un puerto
    serie.
-   Ejecución de código con control sobre interrupciones y reinicios.
-   Modularización del backend en C++ para mantenimiento y
    extensibilidad.
-   Conector opcional hacia un servidor LLM externo (IA).

------------------------------------------------------------------------

## Tecnologías utilizadas

-   **ESP-IDF + PlatformIO** --- Desarrollo del firmware para ESP32.
-   **C/C++** --- Backend modular: control de UART, REPL, WebSockets, HTTP
    y FS.
-   **HTML, CSS, JavaScript** --- Frontend del editor web.
-   **SPIFFS** --- Sistema de archivos embebido para servir la interfaz.
-   **FreeRTOS** --- Tareas concurrentes en el ESP32.

------------------------------------------------------------------------

## Estructura del proyecto

    EspressIDEA/
    ├── .pio/                   ← Carpeta interna de PlatformIO (builds temporales)
    ├── .vscode/                ← Configuración de VSCode
    ├── build/                  ← Archivos de compilación de ESP-IDF
    ├── data/                   ← Archivos estáticos del sitio web (html, css, js)
    ├── include/                ← Headers compartidos globalmente
    ├── lib/                    ← Librerías principales del backend
    │   ├── EspressIDEA/        ← Núcleo del IDE (ReplControl, servicios, etc.)
    │   ├── ServerManager/      ← Manejo de WiFi, mDNS, HTTP y WebSockets
    │   ├── PyBoardUART/        ← Implementación del REPL en C++ vía UART
    ├── managed_components/     ← Dependencias gestionadas por ESP-IDF/PIO
    ├── src/                    ← Código principal (ej. `main.cpp`)
    ├── test/                   ← Pruebas unitarias o de integración
    ├── platformio.ini          ← Configuración de entornos PlatformIO
    ├── sdkconfig*              ← Configuraciones generadas por menuconfig
    ├── partitions.csv          ← Particiones de memoria para el ESP32
    └── README.md               ← Este archivo

------------------------------------------------------------------------

## Compilación y carga del firmware

### 1. Requisitos previos

-   [VSCode](https://code.visualstudio.com/)
-   [PlatformIO](https://platformio.org/)
-   Una placa ESP32 (ej. Wemos D1 Mini ESP32, ESP32-DevKitC)

### 2. Clonar el repositorio

> Importante: **no clones en una ruta con espacios**; ESP-IDF no soporta
> builds en esas condiciones.

``` bash
git clone https://github.com/Maria05py/EspressIdea.git
cd EspressIDEA
```

### 3. Configurar la placa en `platformio.ini`

Ejemplo para Wemos D1 Mini:

``` ini
[env:wemos_d1_mini32]
platform = espressif32
board = wemos_d1_mini32
framework = espidf
monitor_speed = 115200
```

### 4. Compilar y flashear el firmware

``` bash
pio run --target upload
```

### 5. Cambiar los PlaceHolders en CREDENTIALS.txt

Dentro de la carpeta `data` se encuentra un archivo CREDENTIALS.txt, que se debe ver así

```
SSID=WIFI_SSID
PASS=WIFI_PASS
HOST=MDNS_HOSTNAME
LLM_URL=AI_SERVER
```

aquí cambias SSID por el nombre de la red de WiFi, PASS, por la contraseña, HOST por el hostname por el que se va a acceder al servidor, y LLM_URL por la URL del servidor que probee el LLM, para mas información de este puedes ver [aqui](https://github.com/Maria05py/EspressIdea/tree/main/Models/Servidor_LLM).

*IMPORTANTE* 
`LA RED WIFI NO PUEDE SER MAYOR A 2.4Ghz`

### 6. Subir los archivos web (HTML/JS/CSS)

``` bash
pio run --target buildfs
pio run --target uploadfs
```

### 7.Acceder al servidor
Una vez subido el Firmware y los Spiffs, solo tienes que estar conectado a la misma red WiFi que el ESP32, y luego acceder desde el navegador!
simplemente accede al al nombre que pusiste como HOST en CREDENTIALS.txt y le añades un `.local`

*Por ejemplo:*

Si Pusiste

```
HOST=espressidea
```

tendrás que buscar espressidea.local en tu navegador.

Si hiciste esto correctamente verás la interfaz web de EspressIDEA.

------------------------------------------------------------------------

## Uso de la interfaz web

-   Editar código.
-   Guardar y descargar archivos.
-   Ejecutar scripts en la placa.
-   Usar la terminal en vivo.
-   Generar, Arreglar, Documentar y Explicar Código por medio de un asistente IA

------------------------------------------------------------------------

## Estado actual del proyecto

-   [x] Comunicación WebSocket estable.
-   [x] Editor web básico embebido.
-   [x] Explorador de archivos remoto (listado, lectura, escritura,
    borrado).
-   [x] Ejecución de código con control de REPL.
-   [X] Integración con servidor LLM (en progreso).
-   [X] Mejoras de la UI (editor más avanzado, terminal tipo VSCode).

------------------------------------------------------------------------

## Licencia

Este proyecto se distribuye bajo la licencia **CC0 1.0 Universal**.

_Desarrollado por Emanuel Mena Araya, 2025, para las Olimpiadas Informaticas de EXPOCENFO_
_© EpressIDEA 2025_
_No olvides Apoyar a proyectos Open Source!_