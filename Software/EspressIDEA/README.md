
# EspressIDEA

EspressIDEA es un editor web ligero para programar placas microcontroladoras que ejecutan MicroPython o CircuitPython. El proyecto utiliza un ESP32 como intermediario entre el navegador y el dispositivo Python, lo que permite editar, ejecutar y gestionar archivos directamente desde una interfaz web — sin necesidad de instalar nada en el dispositivo host.

---

## 🚀 Características

- Comunicación bidireccional en tiempo real entre navegador y placa vía WebSocket
- Editor web para escribir y ejecutar código Python
- Sistema de archivos remoto (leer, escribir, subir, descargar)
- Soporte para múltiples rutas WebSocket (modular)
- Modularización del backend en C++ para mejor mantenimiento
- Preparado para integrarse con un LLM (modelo de lenguaje) para generación de código inteligente *(próximamente)*

---

## 🛠️ Tecnologías Usadas

- **ESP-IDF + PlatformIO**: para desarrollar el firmware del ESP32
- **C++ (ESP32)**: backend modular para manejar WebSockets, UART y FS
- **JavaScript + HTML/CSS**: frontend del editor
- **WebSockets**: comunicación en tiempo real con el navegador
- **ArduinoJson**: parser JSON para el backend
- **SPIFFS**: sistema de archivos embebido para alojar la página web

---

## 🧩 Estructura del Proyecto

```
EspressIDEA/
├── .pio/                   ← Carpeta interna de PlatformIO (builds temporales)
├── .vscode/                ← Configuración del entorno para VSCode
├── build/                  ← Archivos de compilación generados por ESP-IDF
├── data/                   ← Archivos estáticos para el sitio web (html, css, js, imágenes)
├── include/                ← Headers compartidos si se requieren de forma global
├── lib/                    ← Librerías del proyecto (EspressIDEA, ServerManager, Pyboard, etc.)
│   ├── EspressIDEA/        ← Lógica del IDE, comunicación con el dispositivo Python
│   ├── ServerManager/      ← Manejo del servidor HTTP/WebSocket + SPIFFS
│   └── Pyboard_cpp/        ← Manejo de REPL/CircuitPython vía UART (Pyboard en C++)
├── managed_components/     ← Componentes auto-gestionados por PlatformIO/ESP-IDF
├── src/                    ← Código principal del firmware (main.cpp)
├── test/                   ← Pruebas (si se usan)
├── platformio.ini          ← Configuración de PlatformIO (plataforma, entornos, flags)
├── sdkconfig*              ← Configuraciones generadas por menuconfig para distintas placas
├── partitions.csv          ← Particionamiento de memoria del ESP32
└── README.md               ← Este archivo
```

---

## 🧑‍💻 Cómo compilar y cargar el firmware

### 1. 🧰 Requisitos

- [VSCode](https://code.visualstudio.com/)
- [PlatformIO](https://platformio.org/)
- ESP32 D1 Mini o compatible (soportado en el proyecto)

### 2. 🧪 Clona el repositorio

**NO CLONES EL REPOSITORIO EN UNA DIRECCIÓN QUE TENGA ESPACIOS EN BLANCO, ESP-IDF NO PERMITE HACER BUILDS SI ESTE ES EL CASO**

```bash
git clone https://github.com/Maria05py/EspressIdea.git
cd EspressIDEA
```

### 3. ⚙️ Elige tu placa en `platformio.ini`

El proyecto soporta múltiples placas. Ejemplo:

```ini
[env:wemos_d1_mini32]
platform = espressif32
board = wemos_d1_mini32
framework = espidf
monitor_speed = 115200
```

> Puedes cambiar a otra, como `esp32dev`, si tu placa es diferente.

### 4. 📂 Sube los archivos web (html/js/css)

```bash
pio run --target buildfs
pio run --target uploadfs
```

### 5. 🔥 Compila y flashea el firmware

```bash
pio run --target upload
```

### 6. 🖥️ Abre el monitor serie

```bash
pio device monitor
```

---

## 🌐 Cómo usar la interfaz web

1. Conecta tu ESP32 al WiFi y accede a la IP que muestra por el puerto serie.
2. La interfaz web se servirá automáticamente desde SPIFFS.
3. Escribe código, súbelo, ejecútalo y observa la salida en vivo.

---

## 🧭 Estado actual del proyecto

✅ Comunicación WebSocket  
✅ Editor web básico  
✅ Explorador de archivos remoto  
✅ Comunicación con REPL vía UART  
🔜 Integración con LLM (en desarrollo)  
🔜 Editor visual más completo  
🔜 Terminal interactiva estilo VSCode


---

## 📜 Licencia

Este proyecto se distribuye bajo la licencia CC0 1.0 Universal.
