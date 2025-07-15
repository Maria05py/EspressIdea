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
