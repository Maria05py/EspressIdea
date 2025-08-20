# Arquitectura del Proyecto

## Visión General
EspressIDEA se organiza en una arquitectura modular que separa documentación, software, modelos y pruebas.

### Estructura de Carpetas
- `/docs/`: documentación extendida, notas técnicas y diagramas.  
- `/hardware/`: esquemas eléctricos, PCB y diagramas de conexión.  
- `/software/`: código fuente (backend Flask, scripts de interacción, microcontrolador).  
- `/models/`: prompts, configuraciones y modelos de LLM.  
- `/tests/`: pruebas unitarias y de integración.  
- `/examples/`: ejemplos de uso de la plataforma.  

### Componentes Clave
- **Backend**:  
- **Módulos LLM**:  expone API REST (`/generar`) para la interacción con modelos y su integración con Gemini (nube) y Ollama (local).  


### Flujo General

