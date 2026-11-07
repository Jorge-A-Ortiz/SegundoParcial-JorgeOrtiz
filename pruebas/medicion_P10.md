# P10 con cronómetro en placa

La compilación P10 usa CTIMER1 como cronómetro de 1 µs nominal, con el mismo reloj FRO12M de CTIMER0. Conserva el montaje normal. No requiere puentes adicionales. Se puede quitar el cableado de captura de P02 con la placa apagada.

La ISR toma la primera marca al leer el LF de cada STATUS. La segunda se toma al atender la interrupción TC, que indica que salió el último bit de la respuesta. El diagnóstico retiene el siguiente byte TX hasta esa marca para asociarla a la respuesta correcta. La medida incluye la espera de la cola, el procesamiento y la transmisión de la respuesta. La referencia de tiempo y la atención de las ISR son internas a la placa.

## Ejecución

1. Compila y carga el ELF de diagnóstico.

   ```powershell
   cmake --preset debug -B debug/p10 -DP10_TIMING=ON
   cmake --build debug/p10
   ```

   ELF: `debug/p10/SegundoParcial.elf`.

2. Cierra Serial Monitor y reinicia la placa. Ejecuta el script desde la raíz, con el puerto MCU-Link correspondiente.

   ```powershell
   powershell -ExecutionPolicy Bypass -File pruebas/capturar_P10.ps1 -Puerto COM3
   ```

3. Durante la captura, mueve el potenciómetro a lo largo del rango. El script envía 150 STATUS a intervalos nominales de 200 ms y alterna RUN y PAUSE por UART con STOP y START. Después de los 30 s, el firmware emite las mediciones y los rangos de tiempo de las tareas.

Revisa en el registro el ritmo real de envío, las 150 latencias, los cambios de PCT y MODE, el comportamiento de ALARM y los intervalos de ADC, LED y telemetría. Los eventos periódicos se cronometran al disparar el ADC, cambiar el GPIO y encolar la telemetría. El límite para completar cada respuesta es 100 ms. Las marcas de recepción del PC incluyen MCU-Link y USB.
