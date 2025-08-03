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
- Emanuel Mena Araya (Backend)
- Angel Cabrera Mata (Frontend)
- María Jesús Rodríguez Molina (Trabajo con LLMs e investigación)
- Thais Hernández Quesada (Trabajo con LLMs e investigación)

---

## 2. Descripción y Justificación

**Problema que se aborda:**  
El aprendizaje de programación embebida con microcontroladores resulta complejo para principiantes debido a la necesidad de configurar múltiples herramientas, instalar software específico y conocer lenguajes como Python y Micropython, por lo que .

**Importancia y contexto:**  
Existe una creciente demanda de herramientas educativas accesibles que permitan enseñar programación y robótica desde edades tempranas. EspressIDEA facilita este proceso al centralizar el desarrollo sobre Python y ofrecer asistencia en tiempo real mediante modelos de lenguaje.EspressIDEA es una plataforma de desarrollo remoto y asistido para microcontroladores que ejecutan Python (MicroPython y CircuitPython). Aprovecha un microcontrolador ESP32 que actúa como un intermediario inteligente, permitiendo:
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
Desarrollar una plataforma interactiva de propósito educativo que facilite la enseñanza de programación electrónica, centrada en microcontroladores Espressif y en la integración de modelos de lenguaje avanzados.

**Objetivos Específicos:**  
- Expandir el uso de microcontroladores en entornos educativos.  
- Fomentar el desarrollo de proyectos IoT, electrónica y robótica para todas las edades.  
- Fructificar la integración de modelos de lenguaje en el desarrollo y aprendizaje de proyectos electrónicos.  
- Ampliar la base de conocimiento de principiantes en programación de manera interactiva y centralizada.

---

## 4. Requisitos Iniciales

Lista breve de lo que el sistema debe lograr:

- Requisito 1: Permitir la conexión UART entre el ESP32 y microcontroladores sin WiFi.  
- Requisito 2: Exponer un editor de código accesible vía navegador web local.  
- Requisito 3: Integrar un servidor Flask con capacidad de generar y explicar código usando Gemini u Ollama.

---

## 5. Diseño Preliminar del Sistema

**Arquitectura inicial (diagrama):**


----------------------------


EspressIDEA es una plataforma de desarrollo remoto y asistido para microcontroladores que ejecutan Python (MicroPython y CircuitPython). Aprovecha un microcontrolador ESP32 que actúa como un intermediario inteligente, permitiendo:

- Interfaz serial con dispositivos Python (por UART) como si fueran conectados por USB a un PC.

- Servidor web embebido que proporciona un editor de código accesible por navegador.

- Integración con modelos de lenguaje (LLM) para generar, explicar y documentar código en tiempo real.

- Compatibilidad con placas sin WiFi nativo, como Raspberry Pi Pico, Metro, Feather M4, entre otras.

