# 🧩 Descripción del Código

Este script implementa un **servidor web Flask** que expone una **API REST** para generar código en **CircuitPython** orientado a la placa **IdeaBoard** mediante un modelo de lenguaje grande (**LLM**).  
Su propósito principal es **asistir a estudiantes, educadores y desarrolladores** en la creación de programas embebidos, proporcionando explicaciones y ejemplos claros, que se almacenan de manera ordenada en un historial persistente.


# 🛠️ Estructura Principal del Código

A continuación se describen los componentes y bloques más relevantes:


## 1️⃣ Dependencias

- **Flask**: Framework web para exponer el endpoint `/generar`.
- **subprocess**: Permite ejecutar comandos de terminal (en este caso, para Ollama).
- **json**: Serializa y deserializa historial de conversaciones.
- **google.genai**: Cliente oficial para la API Gemini.
- **datetime**: Marca temporal de cada interacción.
- **os**: Verificación de existencia de historial previo.


## 2️⃣ Variables Globales

- `client`: Cliente de Gemini inicializado con tu API Key.
- `prompt_maestro`: Instrucciones base que definen la personalidad del asistente y el formato de respuesta esperado.



## 3️⃣ Endpoint `/generar`

Este endpoint acepta peticiones **POST** con un cuerpo JSON que debe incluir:

```json
{
  "mensaje": "Texto de la consulta o petición",
  "modelo": "gemini" // o "ollama"
}
```

📄 Para probar la invocación y llamadas al LLM desde PowerShell, se debe utilizar la siguiente estructura:
```
$body = @{
    mensaje = "PROMPT DESEADO"
    modelo = "gemini"
} | ConvertTo-Json

$response = Invoke-RestMethod `
  -Uri "http://127.0.0.1:5000/generar" `
  -Method POST `
  -Headers @{ "Content-Type" = "application/json" } `
  -Body $body

$response.codigo
$response.explicacion
```

🦙 Usando Ollama:
```
$body = @{
    mensaje = "PROMPT DESEADO"
    modelo = "ollama"
} | ConvertTo-Json

$response = Invoke-RestMethod `
  -Uri "http://127.0.0.1:5000/generar" `
  -Method POST `
  -Headers @{ "Content-Type" = "application/json" } `
  -Body $body

$response.codigo
$response.explicacion
```


### 🔹 Flujo detallado

**Carga del historial:**

- Si existe `historial.json`, se leen las entradas previas.
- Si no existe, se inicializa vacío.

**Contextualización:**

- Se toman las últimas tres interacciones (si las hay).
- Se incorporan al prompt como “historial conversacional”.

**Ejecución del modelo:**

- Si se selecciona `gemini`, se hace una llamada directa con `client.models.generate_content`.
- Si se selecciona `ollama`, se invoca el comando CLI de Ollama.

**Procesamiento de la respuesta:**

- El texto se divide en `<Explicacion>` y `<Codigo>`.
- Ambos se almacenan por separado.

**Registro:**

Cada interacción se guarda en `historial.json` con:

- Fecha y hora.
- Mensaje original.
- Modelo usado.
- Respuesta estructurada.

**Respuesta HTTP:**

Devuelve un objeto JSON con las secciones:

- `explicacion`
- `codigo`

---

# 📂 Archivo `historial.json`

Este archivo actúa como una **bitácora persistente** de todas las consultas realizadas.  
Cada registro incluye:

- `fecha`: marca temporal ISO8601.
- `mensaje`: consulta original.
- `modelo`: “gemini” u “ollama”.
- `explicacion`: explicación generada por el asistente.
- `codigo`: bloque de código generado.

Esto permite:

✅ Mantener contexto en futuras consultas.  
✅ Auditar interacciones pasadas.  
✅ Mejorar trazabilidad del uso de la plataforma.

---

# 🔄 Ejemplo de Respuesta JSON

Una respuesta típica del endpoint `/generar` tiene el siguiente formato:

```json
{
  "explicacion": "Este ejemplo muestra cómo controlar un servo usando la librería ideaboard...",
  "codigo": "from ideaboard import IdeaBoard\nib = IdeaBoard()\nservo = ib.Servo(5)\nservo.angle = 90"
}

```
