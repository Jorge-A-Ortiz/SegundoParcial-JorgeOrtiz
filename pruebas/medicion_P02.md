# P02 con captura de flancos en la placa

CTIMER1 registra ambos flancos de dos señales físicas durante 10 s. Su contador corre a 1 MHz nominal: FRO12M a 12 MHz, divisor 1 y PR=11. CTIMER0 conserva la programación normal. Ambos comparten FRO12M, así que la prueba comprueba la cadencia relativa, con la precisión del reloj de la placa. El registro UART del PC incluye el transporte USB y marca la recepción de la línea completa.

## Preparar la captura

1. Con la placa desconectada, coloca dos puentes. Mantén las conexiones del potenciómetro y los LED.

   | Salida | Entrada de captura |
   |---|---|
   | P3_0, J8 pin 7 (diagnóstico ADC) | P3_14, J1 pin 11 (CT_INP6) |
   | P3_13, J1 pin 13 (LED verde) | P3_15, J1 pin 9 (CT_INP7) |

2. Configura y compila desde la raíz. Selecciona el ELF de diagnóstico al cargarlo en MCU-Link.

   ```powershell
   cmake --preset debug -B debug/p02 -DP02_CAPTURE=ON
   cmake --build debug/p02
   ```

   Archivo: `debug/p02/SegundoParcial.elf`. El preset normal no habilita el diagnóstico.

3. Cierra Serial Monitor y ejecuta el siguiente comando. Cambia COM3 por el puerto MCU-Link de tu equipo. Pulsa RESET cuando lo pida el script y no envíes comandos.

   ```powershell
   powershell -ExecutionPolicy Bypass -File pruebas/capturar_P02.ps1 -Puerto COM3
   ```

## Interpretar el resultado

- **ADC:** alrededor de 100 flancos. El mínimo y máximo entre flancos deben quedar entre 95000 y 105000 µs.
- **LED:** alrededor de 20 flancos. El mínimo y máximo deben quedar entre 475000 y 525000 µs.
- **UART:** 10 líneas periódicas y 9 intervalos. Conserva tanto el campo interno `t` como los intervalos observados en el PC, con objetivo de 1000 ms ±5 %.
- **Control:** repite sin los puentes. Las entradas con pull-down no deberían detectar el tren de flancos. Un mínimo y máximo de cero con menos de dos flancos indica que faltaron datos para medir.

Adjunta el archivo generado por el script al informe. Las líneas P02 son un resumen de capturas de hardware, emitido después de la ventana de medición.
