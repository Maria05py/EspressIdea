# Generador de Código en CircuitPython con LLMs

Este proyecto implementa un **servidor web con Flask** que expone un **endpoint REST** (`/generar`) para la creación de programas en **CircuitPython** y **MicroPython** orientados a diferentes placas de desarrollo Espressif.  El sistema utiliza **modelos de lenguaje (LLMs)**, tanto locales (Ollama utilizando Deepsek en este caso) como en la nube (Gemini), para generar código acompañado de explicaciones educativas.  
El objetivo principal es **asistir a estudiantes, educadores y desarrolladores** en la elaboración de programas embebidos, facilitando el aprendizaje y el desarrollo de proyectos de robótica y automatización.

---

## Estructura General

### Dependencias

- **Flask**: framework web para exponer la API REST.
- **subprocess**: ejecución de comandos externos (para Ollama).
- **json**: lectura y escritura de historiales persistentes.
- **google.genai**: cliente oficial de Gemini.
- **datetime**: gestión de marcas temporales.
- **os**: verificación de existencia de archivos de historial.

---

## Configuración de Modelos y Prompts

El sistema soporta múltiples placas, cada una con un **prompt maestro** que define:

- Información técnica de la placa.  
- Librerías disponibles.  
- Expectativas sobre el estilo y formato de respuesta.  
- Reglas para generar código educativo y comentado.  

Actualmente, se incluyen los siguientes perfiles:

- **IdeaBoard** (ESP32-WROOM-32E, CRCibernética, Costa Rica).  
- **DOIT ESP32 DevKit V1**.  
- **Adafruit ItsyBitsy M4 Express**.  

Cada prompt establece lineamientos sobre librerías, mapeo de pines y formato esperado de salida.

---

## Endpoint `/generar`

El endpoint recibe solicitudes **POST** con un cuerpo JSON de la siguiente forma:

```json
{
  "mensaje": "Texto con la petición o consulta",
  "placa": "ideaboard",
  "modelo": "gemini" // o "ollama"
}
```

## Flujo de procesamiento

### Validación de placa
- Verifica que la placa indicada tenga un prompt configurado.

### Carga de historial
- Se utiliza un archivo `historial_<placa>.json` para cada tipo de placa.  
- Si existe, se cargan las interacciones previas.  
- Se toman hasta las tres últimas interacciones como contexto adicional.  

### Construcción del prompt
- Combina el prompt maestro de la placa con el historial y la nueva consulta del usuario.  

### Invocación del modelo
- **Gemini**: llamada a `client.models.generate_content`.  
- **Ollama**: ejecución mediante CLI con `subprocess`.  

### Procesamiento de la respuesta
- Se busca dividir la salida en dos secciones: `<Explicacion>` y `<Codigo>`.  
- Si no se encuentra la etiqueta `<Codigo>`, todo el texto se almacena como explicación.  

### Registro de interacción
Cada interacción se almacena en `historial_<placa>.json` con:
- Fecha y hora.  
- Mensaje original.  
- Modelo utilizado.  
- Explicación y código generados.  

### Respuesta HTTP
Se devuelve un objeto JSON con las claves:
- `explicacion`  
- `codigo`  

---

## Ejemplo de Uso en PowerShell

### Con Gemini
```powershell
$body = @{
    mensaje = "Encender un LED con IdeaBoard"
    placa   = "ideaboard"
    modelo  = "gemini"
} | ConvertTo-Json

$response = Invoke-RestMethod `
  -Uri "http://127.0.0.1:5000/generar" `
  -Method POST `
  -Headers @{ "Content-Type" = "application/json" } `
  -Body $body

$response.explicacion
$response.codigo
```

### Con Ollama
```powershell
$body = @{
    mensaje = "Mover un servo conectado al pin 5"
    placa   = "ideaboard"
    modelo  = "ollama"
} | ConvertTo-Json

$response = Invoke-RestMethod `
  -Uri "http://127.0.0.1:5000/generar" `
  -Method POST `
  -Headers @{ "Content-Type" = "application/json" } `
  -Body $body

$response.explicacion
$response.codigo
```
## Persistencia del Historial

Cada interacción se guarda en un archivo historial_<placa>.json, lo que permite:

- Mantener contexto entre consultas consecutivas.
- Auditar conversaciones pasadas.
- Facilitar el seguimiento y mejora del aprendizaje.

