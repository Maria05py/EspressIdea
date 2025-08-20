# Pruebas de EspressIDEA

## Objetivo
Verificar que el firmware arranca correctamente y que la interfaz es accesible.

## Test 1.1 – Conexión WiFi
- Flashear firmware en ESP32.
- Revisar logs de arranque → debe mostrar “WiFi conectado, IP asignada”.
- Verificar que el ESP32 responde a ping `espressidea.local`.

## Test 1.2 – Servidor web disponible
- Acceder desde el navegador a `http://espressidea.local`.
- La página debe cargar el editor web.
- Verificar carga de CSS y JS en consola del navegador (sin errores 404).

## 2. Explorador de archivos
### Objetivo
Confirmar que el sistema de archivos remoto funciona.

### Test 2.1 – Crear archivo
- Crear `foo.py` desde el editor web.
- Refrescar explorador → debe aparecer en la lista.

### Test 2.2 – Editar y guardar archivo
- Abrir `foo.py`, escribir `print("hola mundo")`, guardar.
- Descargar archivo y confirmar contenido.

### Test 2.3 – Eliminar archivo
- Borrar `foo.py`.
- Verificar que ya no aparece en la lista.

## 3. REPL y ejecución de código
### Objetivo
Validar que el control del REPL es estable.

### Test 3.1 – Acceso REPL en navegador
- Abrir la terminal web.
- Escribir `print(2+2)` → debe devolver `4`.

### Test 3.2 – Ejecutar script
- Subir `blink.py` con un parpadeo en LED integrado.
- Ejecutar desde interfaz.
- LED debe parpadear.

### Test 3.3 – Interrupción de script
- Ejecutar un `while True` en script.
- Presionar botón de detener.
- REPL debe quedar nuevamente disponible.

## 4. Comunicación en tiempo real
### Objetivo
Validar que WebSocket es confiable.

### Test 4.1 – Comandos consecutivos
- Enviar desde terminal:
  ```
  for i in range(3):
      print(i)
  ```
- La salida debe mostrar `0, 1, 2` en tiempo real.

### Test 4.2 – Desconexión y reconexión
- Desconectar la WiFi del PC.
- Reconectar y volver a `espressidea.local`.
- Confirmar que WebSocket se restablece.

## 5. Integración con IA (si servidor LLM está activo)
### Objetivo
Validar que el puente con IA funciona.

### Test 5.1 – Generar código
- Enviar prompt: “Escribe un programa para mover un servo en pin 5”.
- Debe devolver un script en CircuitPython con `servo = ib.Servo(5)`.

### Test 5.2 – Explicación de código
- Subir un archivo con funciones.
- Usar botón “Explicar código”.
- El asistente debe devolver explicación en `<Explicacion>` y `<Codigo>`.

## 6. Robustez del sistema
### Objetivo
Medir la estabilidad ante errores.

### Test 6.1 – Archivo grande
- Subir un archivo de >200 KB al sistema de archivos.
- Confirmar que se guarda sin corrupción.

### Test 6.2 – Código con error
- Ejecutar:
  ```
  print(x)
  ```
- El terminal debe mostrar `NameError: name 'x' is not defined`.
- Sistema no debe colgarse.

## 📂 Recomendación de organización en `/tests/`
- `/tests/connectivity.md` → pruebas de conexión.
- `/tests/filesystem.md` → pruebas de archivos.
- `/tests/repl.md` → pruebas de REPL.
- `/tests/ia.md` → pruebas de integración con LLM.
- `/tests/stress.md` → pruebas de robustez.
