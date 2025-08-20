#  Hardware 

Este readme contiene la documentación extendida del **hardware** utilizado en el proyecto, el cual combina una **placa ESP32** con diferentes módulos electrónicos y mecánicos para crear un sistema embebido completo y funcional.

---

##  Contenido de la carpeta

- 📋 **Listado de componentes**: módulos electrónicos, sensores, actuadores y accesorios.  
- 📝 **Notas técnicas** sobre integración y alimentación de energía.  

---

##  Componentes principales

| Componente | Descripción | Enlace |
|------------|-------------|--------|
| [IdeaBoard](https://www.crcibernetica.com/crcibernetica-ideaboard) | Placa principal corriendo **CircuitPython** para control y programación. | Ver componente |
| [WEMOS D1 Mini ESP32](https://www.crcibernetica.com/wemos-d1-mini-esp32) | Placa secundaria corriendo **EspressIDEA**, utilizada para lógica y conectividad. | Ver componente |
| [MT3608 2A 2V-24V DC-DC Booster Power Module](https://www.crcibernetica.com/mt3608-2a-2v-24v-dc-dc-booster-power-module/?searchid=2437815&search_query=Boost) | Eleva el voltaje de entrada para garantizar una alimentación estable. | Ver componente |
| [TP4056 Lithium Battery Charger Module (USB-C, doble protección)](https://www.crcibernetica.com/tp4056-lithium-battery-charger-module-with-dual-protection-usb-c/?searchid=2437823&search_query=USB+c) | Permite cargar baterías de litio con protección contra sobrecarga y sobredescarga. | Ver componente |
| [Batería de polímero de litio 3.7V 600mAh](https://www.crcibernetica.com/lithium-ion-polymer-battery-3-7v-600mah/?searchid=2437835&search_query=Lithium) | Fuente de energía ligera y recargable. | Ver componente |
| [Micro JST 1.25mm (cables macho/hembra, 2 pines)](https://www.crcibernetica.com/micro-jst-1-25mm-2-pin-male-and-female-cables/?searchid=2437827&search_query=Jst) | Conectores compactos para facilitar conexión y desconexión de la batería. | Ver componente |
| [Rodamientos 608ZZ (x2)](https://www.crcibernetica.com/608zz-roller-bearing/?searchid=2437838&search_query=Roller) | Reducen fricción en el sistema mecánico y soporte de ruedas. | Ver componente |

---

##  Componentes mecánicos

| Componente | Descripción |
|------------|-------------|
| Acrílico 3mm (corte láser) | Material para fabricar el chasis del robot. |
| Impresión 3D (ruedas y soporte de motores) | Se imprimieron piezas personalizadas en 3D para movilidad y montaje de motores. |
| Tornillos y tuercas M3 | Fijación estructural de los componentes al chasis. |
| Bandas elásticas (x2) | Elemento de sujeción adicional para asegurar componentes. |

---

##  Componentes alternativos (otra configuración)

| Componente | Descripción | Enlace |
|------------|-------------|--------|
| [Ruedas de goma 42mm para microgearmotors (x2)](https://www.crcibernetica.com/42mm-rubber-wheels-for-micro-gearmotors/?searchid=2437873&search_query=Wheels) | Alternativa a ruedas impresas en 3D. | Ver componente |
| [Ball Caster 20mm](https://www.crcibernetica.com/ball-caster-20mm/?searchid=2437878&search_query=Wheel) | Soporte adicional para estabilidad del chasis. | Ver componente |
| [Porta baterías 4xAA con tapa y switch](https://www.crcibernetica.com/battery-holder-4xaa-with-cover-and-switch/?searchid=2437884&s) | Alternativa para alimentación mediante pilas AA. | Ver componente |


---


