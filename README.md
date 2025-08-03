<p align="center">
  <img src="Assets/ESPRESSIDEA_004.svg" alt="Logo EspressIDEA" width="1000"/>
</p>

<p align="center">
  <a href="https://github.com/Maria05py/EspressIdea">
    <img alt="Version"
         src="https://img.shields.io/badge/version-1.0.0-343C4C?style=for-the-badge&logo=github&logoColor=white&labelColor=e8362d">
  </a>
  <a href="https://github.com/Maria05py/EspressIdea/blob/main/LICENSE">
    <img alt="License"
         src="https://img.shields.io/badge/license-CC--BY--NC-343C4C?style=for-the-badge&logo=creativecommons&logoColor=white&labelColor=e8362d">
  </a>
  <a href="#">
    <img alt="Status"
         src="https://img.shields.io/badge/status-Experimental-343C4C?style=for-the-badge&logo=python&logoColor=white&labelColor=e8362d">
  </a>
</p>

# EspressIDEA - Avance Preliminar del Proyecto

## 1. Información del Proyecto

**Nombre del Proyecto:** EspressIDEA  
**Equipo:**
- Emanuel Mena Araya (Backend)
- Angel Cabrera Mata (Frontend)
- Thais Hernández Quesada (LLM e investigación)
- María Jesús Rodríguez Molina (LLM e investigación)


---

## 2. Descripción y Justificación

**Problema que se aborda:**  
El aprendizaje de programación embebida con microcontroladores resulta complejo para principiantes debido a la necesidad de configurar múltiples herramientas, instalar software específico y conocer lenguajes como C o C++.

**Importancia y contexto:**  
Existe una creciente demanda de herramientas educativas accesibles que permitan enseñar programación y robótica desde edades tempranas. EspressIDEA facilita este proceso al centralizar el desarrollo sobre Python y ofrecer asistencia en tiempo real mediante modelos de lenguaje. EspressIDEA es una plataforma de desarrollo remoto y asistido para microcontroladores que ejecutan Python (MicroPython y CircuitPython). Aprovecha un microcontrolador ESP32 que actúa como un intermediario inteligente, permitiendo:
- Interfaz serial con dispositivos Python (por UART) como si fueran conectados por USB a un PC.
- Servidor web embebido que proporciona un editor de código accesible por navegador.
- Integración con modelos de lenguaje (LLM) para generar, explicar y documentar código en tiempo real.
- Compatibilidad con placas sin WiFi nativo, como Raspberry Pi Pico, Metro, Feather M4, entre otras.

**Usuarios/beneficiarios:**  
- Estudiantes de primaria, secundaria y universidad.  
- Docentes de tecnología, computación o robótica.  
- Aficionados a la electrónica y makers.  
- Instituciones educativas sin laboratorios sofisticados.

---

## 3. Objetivos del Proyecto

**Objetivo General:**  
•	Desarrollar una plataforma interactiva de propósito educativo que facilita la enseñanza y aprendizaje de programación electrónica, centrado en microcontroladores Esspresif y la integración de modelos de lenguaje avanzados, con el fin de capacitar estudiantes de diversas edades en la construcción de proyectos tecnológicos. 

**Objetivos Específicos:**  
•	Expandir el uso de microcontroladores en entornos educativos.
•	Fomentar el desarrollo de proyectos IOT, electrónica y robótica para todas las edades. 
•	Fructificar la integración de modelos de lenguaje en el desarrollo y aprendizaje de proyectos electrónicos.
•	Ampliar la base de conocimiento de principiantes en programación de manera interactiva y centralizada.

---

## 4. Requisitos Iniciales

Lista breve de lo que el sistema debe lograr:

- Requisito 1: Permitir la conexión UART entre el ESP32 y microcontroladores sin WiFi.  
- Requisito 2: Exponer un editor de código accesible vía navegador web local.  
- Requisito 3: Integrar un servidor Flask con capacidad de generar y explicar código usando Gemini u Ollama.

---

## 5. Diseño Preliminar del Sistema

**Arquitectura inicial (diagrama en proceso):**




**Componentes previstos:**

- **Microcontrolador:** ESP32 actuando como intermediario.  
- **Sensores/actuadores:** (Según el proyecto educativo)  
- **LLM/API:** Gemini (API externa) y Ollama (modelo local)  
- **Librerías y herramientas:** Flask, google.genai, PySerial, dotenv  
- **Bocetos o esquemas:** (Agregar imágenes o diagramas si es posible)

---

## 6. Plan de Trabajo

**Cronograma preliminar:**

| Hito                             |           Fecha estimada          |
|----------------------------------|-----------------------------------|
| Instalación y configuración LLM  |  Semana del 1 al 5 de Julio       |
| Desarrollo del editor web        |  Semana del 5 al 12 de Julio      | 
| Comunicación UART funcional      |  Semana del 12 al 19 de Julio     |   
| Generación de código en vivo     |                 |
| Pruebas con placas reales        |                 |
| Documentación inicial            |                 |
| Revisión final y ajustes         |                 |

**Riesgos identificados y mitigaciones:**

- **Riesgo 1:** Problemas de compatibilidad entre ESP32 y placas destino.  
  **Mitigación:** Usar firmwares estables (MicroPython/CircuitPython) y realizar pruebas específicas por placa.

- **Riesgo 2:** Latencia o fallos de respuesta del modelo de lenguaje.  
  **Mitigación:** Configurar fallback entre Gemini y Ollama, registrar logs detallados para debugging.

---

## 7. Prototipos conceptuales (si aplica)










