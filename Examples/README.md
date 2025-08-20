# Ejemplos para IdeaBoard con ESP32

Este repositorio contiene ejemplos de código y proyectos para aprender a programar con **IdeaBoard** y **ESP32** utilizando **CircuitPython/MicroPython**. Los ejemplos están organizados por niveles de dificultad: principiante, intermedio, avanzado y con IA (si hay un servidor LLM conectado).

## 🔹 Ejemplos Básicos (Nivel Principiante)

### Hola Mundo en la Placa
- **Descripción**: Script que imprime “Hola, EspressIDEA” en la terminal REPL.
- **Propósito**: Probar la conectividad entre el navegador, el ESP32 y la placa.
- **Archivo**: `examples/basic/hello_world.py`

### Parpadeo de LED Integrado
- **Descripción**: Script en CircuitPython/MicroPython que hace parpadear el LED interno.
- **Propósito**: Validación rápida de la ejecución de código en la placa.
- **Archivo**: `examples/basic/blink_led.py`

### Lectura de un Potenciómetro
- **Descripción**: Conexión de un potenciómetro a un pin ADC y muestra del valor en la terminal del navegador.
- **Propósito**: Introducción a la lectura de sensores analógicos.
- **Archivo**: `examples/basic/read_potentiometer.py`

## 🔹 Ejemplos Intermedios (Nivel Educativo)

### Servo Controlado por Deslizador en el Front
- **Descripción**: Código en la placa para mover un servo en un pin específico, controlado por un deslizador en el frontend que envía valores en tiempo real por WebSocket.
- **Propósito**: Enseñar comunicación bidireccional entre placa y navegador.
- **Archivo**: `examples/intermediate/servo_slider.py`

### Semáforo Simple
- **Descripción**: Controla tres LEDs (rojo, amarillo, verde) en secuencia.
- **Propósito**: Enseñanza de condicionales y bucles en un contexto práctico.
- **Archivo**: `examples/intermediate/traffic_light.py`

### Sensor de Temperatura con DHT11/DHT22
- **Descripción**: Lectura periódica de un sensor de temperatura/humedad y muestra de datos en la consola del navegador.
- **Propósito**: Introducción a sensores ambientales.
- **Archivo**: `examples/intermediate/dht_sensor.py`

### Control de Motor DC
- **Descripción**: Usa la librería `ideaboard.motor` (si aplica) para controlar velocidad y dirección de un motor DC desde botones en el frontend.
- **Propósito**: Enseñar control de actuadores.
- **Archivo**: `examples/intermediate/motor_control.py`

## 🔹 Ejemplos Avanzados (Nivel Maker)

### Explorador de Archivos Remoto
- **Descripción**: Permite subir, editar y ejecutar un archivo `main.py` en la placa, con validación de persistencia en SPIFFS.
- **Propósito**: Gestión remota de archivos en el ESP32.
- **Archivo**: `examples/advanced/file_explorer_demo.md`

### Juego Educativo Simple
- **Descripción**: Script en Python ejecutado desde la placa (ej. adivinar un número) que envía respuestas a la terminal. El frontend actúa como interfaz interactiva.
- **Propósito**: Introducción a aplicaciones interactivas.
- **Archivo**: `examples/advanced/number_guessing_game.py`

### Dashboard IoT Básico
- **Descripción**: Lee un sensor (ej. luminosidad) y envía datos en tiempo real al navegador mediante WebSocket.
- **Propósito**: Crear un sistema IoT básico.
- **Archivo**: `examples/advanced/iot_dashboard.py`

## 🔹 Ejemplos con IA (Si Servidor LLM Está Conectado)

### Generación Automática de un Programa
- **Descripción**: Prompt: “Genera código para encender un LED en el pin 2 cuando un botón está presionado”. La IA devuelve el código con una explicación.
- **Propósito**: Automatización de generación de código.
- **Archivo**: `examples/with_ai/generate_led_code.md`

### Explicación de un Script Existente
- **Descripción**: Sube un archivo Python y la IA proporciona una explicación línea por línea.
- **Propósito**: Facilitar la comprensión de código existente.
- **Archivo**: `examples/with_ai/explain_script.md`

### Corrección de Errores en Código
- **Descripción**: Introduce un código con un error de sintaxis y la IA lo corrige, devolviendo una versión funcional.
- **Propósito**: Depuración asistida por IA.
- **Archivo**: `examples/with_ai/fix_code_error.md`

## 📂 Estructura del Repositorio

```plaintext
/examples/
├── basic/
│   ├── hello_world.py
│   ├── blink_led.py
│   └── read_potentiometer.py
├── intermediate/
│   ├── servo_slider.py
│   ├── traffic_light.py
│   ├── dht_sensor.py
│   └── motor_control.py
├── advanced/
│   ├── file_explorer_demo.md
│   ├── number_guessing_game.py
│   └── iot_dashboard.py
└── with_ai/
    ├── generate_led_code.md
    ├── explain_script.md
    └── fix_code_error.md
```

## Notas
- Los ejemplos están diseñados para ser compatibles con **CircuitPython** o **MicroPython** en un ESP32 con **IdeaBoard**.
- Asegúrate de tener las dependencias necesarias (como librerías para sensores o WebSocket) instaladas en tu placa.
- Los ejemplos con IA requieren un servidor LLM configurado y conectado.
