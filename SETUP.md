
# EspressIDEA

## Guía de instalación y despliegue

Este documento describe los pasos necesarios para instalar, configurar y ejecutar **EspressIDEA**, tanto en el entorno de desarrollo (ESP32 + PlatformIO) como en la integración opcional con un servidor de IA.

---

## 1. Requisitos previos

- **Hardware**
  - Una placa ESP32 (ej. Wemos D1 Mini ESP32, ESP32-DevKitC).
  - Conexión WiFi en red de **2.4 GHz** (no compatible con 5 GHz).

- **Software**
  - [Visual Studio Code](https://code.visualstudio.com/)
  - [PlatformIO](https://platformio.org/install/ide?install=vscode)
  - Python 3.9 o superior
  - Dependencias de ESP-IDF instaladas (administradas por PlatformIO)

- **Opcional**
  - Acceso a [Gemini API](https://ai.google.dev/) (clave de API).
  - [Ollama](https://ollama.ai/) instalado para modelos locales.

---

## 2. Clonar el repositorio

> ⚠️ No clones en una ruta con espacios; ESP-IDF no soporta builds en esas condiciones.

```bash
git clone https://github.com/usuario/EspressIDEA.git
cd EspressIDEA
```

---

## 3. Configuración de la placa

Edita el archivo `platformio.ini` según tu modelo de ESP32. Ejemplo para Wemos D1 Mini ESP32:

```ini
[env:wemos_d1_mini32]
platform       = espressif32
board          = wemos_d1_mini32
framework      = espidf
monitor_speed  = 115200
```

---

## 4. Compilar y flashear el firmware

```bash
pio run --target upload
```

Esto compilará el firmware y lo cargará en la placa ESP32.

---

## 5. Configuración de credenciales WiFi e IA

Dentro de la carpeta `data/` se encuentra un archivo `CREDENTIALS.txt`. Debe tener la siguiente estructura:

```ini
SSID=WIFI_SSID
PASS=WIFI_PASS
HOST=MDNS_HOSTNAME
LLM_URL=http://servidor-ia:5000
```

- **SSID**: Nombre de tu red WiFi (2.4 GHz).
- **PASS**: Contraseña de tu red WiFi.
- **HOST**: Nombre de host para acceder desde el navegador (ej. `espressidea`).
- **LLM_URL**: Dirección del servidor IA (Flask con Gemini/Ollama).

Accederás a la interfaz web mediante `http://<HOST>.local`.

---

## 6. Subir los archivos del front (HTML/JS/CSS)

```bash
pio run --target buildfs
pio run --target uploadfs
```

Esto carga la interfaz web en el sistema de archivos SPIFFS del ESP32.

---

## 7. Acceder al servidor

1. Conéctate a la misma red WiFi que el ESP32.
2. Ingresa en tu navegador:

```plaintext
http://<HOST>.local
```

Ejemplo: si en `CREDENTIALS.txt` configuraste `HOST=espressidea`, abre:

```plaintext
http://espressidea.local
```

---

## 8. Uso de la interfaz web

- Editar y guardar código.
- Subir y descargar archivos.
- Ejecutar scripts en la placa.
- Usar la terminal REPL en vivo.
- (Opcional) Generar, explicar y documentar código con IA conectada.

---

## 9. Estado actual del proyecto

- ✅ Comunicación WebSocket estable.
- ✅ Editor web básico embebido.
- ✅ Explorador de archivos remoto (listar, leer, escribir, borrar).
- ✅ Ejecución de código en la placa con control del REPL.
- ✅ Integración con servidor LLM.
- 🔄 Mejoras de la UI (editor más avanzado, terminal tipo VSCode).
