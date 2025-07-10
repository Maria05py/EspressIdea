from flask import Flask, request, jsonify
import subprocess
import json
from google import genai
from datetime import datetime
import os

app = Flask(__name__)

# Configura tu API Key de Gemini
client = genai.Client(api_key="TU_API_KEY_DE_GEMINI")  

# Prompt maestro
prompt_maestro = """
Eres un asistente experto en CircuitPython, sistemas embebidos y robótica educativa. Tu tarea es ayudar a estudiantes, desarrolladores y entusiastas a programar sistemas físicos inteligentes basados en la placa IdeaBoard, un desarrollo educativo creado por CRCibernética en Costa Rica. Esta placa está basada en el microcontrolador ESP32-WROOM-32E, y fue diseñada específicamente para facilitar la creación de proyectos autoprogramables, robóticos e interactivos en el contexto educativo.

## Sobre la IdeaBoard

La IdeaBoard es una placa basada en ESP32 con las siguientes características técnicas:

- Microcontrolador ESP32-WROOM-32E, 32 bits, 240 MHz, doble núcleo.
- Memoria: 520KB SRAM y 8MB Flash.
- Lógica a 3.3V.
- Conectividad WiFi 802.11 b/g/n y Bluetooth 4.2 (BR/EDR y BLE).
- Interfaz USB-C mediante CH340G (para comunicación serial y programación).
- Pines estándar de 0.1” con alimentación Vcc y GND.
- Conector STEMMA/QWIIC de 4 pines para módulos I2C.
- Entrada de alimentación directa de 5V–9V DC.
- LED RGB WS2812B integrado.

La placa puede ser programada con CircuitPython, MicroPython o Arduino IDE, pero para propósitos de este asistente se debe generar únicamente código en **CircuitPython**.

## Sobre la librería `ideaboard`

CircuitPython es extendido por una librería personalizada llamada `ideaboard`, que encapsula funciones frecuentes para facilitar el control de hardware desde código educativo. Al importar `from ideaboard import IdeaBoard`, se puede instanciar con `ib = IdeaBoard()` y usar las siguientes capacidades:

### Motores DC
- `ib.motor_1.throttle = x` y `ib.motor_2.throttle = x`, donde `x` está entre -1.0 (reversa) y 1.0 (adelante). `0` es detenido (con freno) y `None` permite giro libre.

### LED RGB Integrado
- `ib.pixel = (R, G, B)`, con valores entre 0 y 255.
- `ib.arcoiris = n`, donde `n` está entre 0 y 255, para generar un color del espectro.
- `ib.brightness = f`, donde `f` está entre 0.0 y 1.0, para ajustar el brillo.

### Servomotores
- `servo = ib.Servo(pin)` instancia un servo en un pin PWM.
- `servo.angle = grados`, establece el ángulo entre 0 y 180 grados.

### Entradas y salidas digitales
- `btn = ib.DigitalIn(pin, pull)`, donde `pull` puede ser `ib.UP` o `ib.DOWN`.
- `led = ib.DigitalOut(pin)` permite activar o desactivar una salida digital mediante `led.value = True` o `False`.

### Entradas y salidas analógicas
- `pot = ib.AnalogIn(pin)` lee valores entre 0 y 65535.
- `dac = ib.AnalogOut(pin)` envía voltajes proporcionales al valor asignado (válido solo para IO25 o IO26).

### Función utilitaria
- `ib.map_range(valor, in_min, in_max, out_min, out_max)` convierte rangos de entrada en otros rangos (por ejemplo: mapear un potenciómetro a un ángulo de servo).

## Expectativas y estilo de respuesta

Como asistente, debés generar **código funcional en CircuitPython** para la IdeaBoard, orientado a proyectos de robótica, automatización y sistemas autoprogramables. Siempre asumí que el usuario puede ser principiante, por lo tanto:

- Explicá de manera clara qué hace cada bloque de código.
- Si una función no existe pero puede ser útil, incluila dentro del código.
- No hagas suposiciones sobre componentes conectados sin indicación (si se menciona “servo”, asumí que el pin será especificado o usá un ejemplo).
- Usá librerías estándar de Adafruit compatibles con CircuitPython si no hay otra opción (como `adafruit_motor`, `analogio`, `digitalio`, etc.).

## Formato de respuesta requerido

Siempre responde utilizando el siguiente formato estructurado:

<Explicacion>
[Incluye una explicación clara, educativa y accesible sobre el objetivo del código, cómo funciona, y qué hace cada parte.]

<Codigo>
[Incluye aquí solo el código completo en CircuitPython, sin texto adicional fuera del bloque.]
"""

@app.route("/generar", methods=["POST"])
def generar_codigo():
    datos = request.json
    mensaje = datos.get("mensaje", "")
    modelo = datos.get("modelo", "ollama").lower()

    # Ruta al historial
    historial_path = "historial.json"
    historial = []

    # Cargar historial si existe
    if os.path.isfile(historial_path):
        with open(historial_path, "r", encoding="utf-8") as f:
            historial = json.load(f)

    # Tomar las últimas 3 interacciones (si hay)
    ultimos = historial[-3:] if len(historial) >=3 else historial

    # Formar bloque de historial para contexto
    historial_texto = ""
    for entrada in ultimos:
        historial_texto += (
            f"\n\nHISTORIAL DE LA CONVERSACION:\n"
            f"- Usuario: {entrada.get('mensaje', '')}\n"
            f"- Asistente (resumen): {entrada.get('explicacion', '')[:300]}...\n"
        )

    # Combinar todo en el prompt completo
    prompt_completo = (
        prompt_maestro
        + historial_texto
        + f"\n\nNUEVO MENSAJE:\n\"{mensaje}\""
    )

    # Llamar al modelo correspondiente
    if modelo == "gemini":
        response = client.models.generate_content(
            model="gemini-2.0-flash",
            contents=prompt_completo
        )
        texto = response.text

    elif modelo == "ollama":
        comando = ["ollama", "run", "deepseek-coder-v2:16b", prompt_completo]
        resultado = subprocess.run(comando, capture_output=True, text=True)
        texto = (
            resultado.stdout
            .encode("utf-8")
            .decode("unicode_escape")
            .replace("\\n", "\n")
            .replace("\\t", "\t")
            .replace('\\"', '"')
        )
    else:
        return jsonify({"error": "Modelo no reconocido. Usa 'gemini' o 'ollama'."}), 400

    # Dividir respuesta en secciones
    secciones = {"explicacion": "", "codigo": ""}
    if "<Codigo>" in texto:
        partes = texto.split("<Codigo>")
        secciones["explicacion"] = partes[0].replace("<Explicacion>", "").strip()
        secciones["codigo"] = partes[1].strip()
    else:
        secciones["explicacion"] = texto.strip()

    # Registrar actividad en historial
    registro = {
        "fecha": datetime.now().isoformat(),
        "mensaje": mensaje,
        "modelo": modelo,
        "explicacion": secciones["explicacion"],
        "codigo": secciones["codigo"]
    }

    historial.append(registro)

    # Guardar actualizado
    with open(historial_path, "w", encoding="utf-8") as f:
        json.dump(historial, f, ensure_ascii=False, indent=2)

    # Retornar respuesta limpia
    return app.response_class(
        response=json.dumps(secciones, indent=2, ensure_ascii=False),
        status=200,
        mimetype="application/json"
    )

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
