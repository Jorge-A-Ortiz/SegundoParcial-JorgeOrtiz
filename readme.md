# Segundo Parcial — Monitor analógico con FRDM-MCXA156

**Estudiante:** Jorge Alberto Ortiz Nieves
**Matrícula:** 20250504

## Objetivo

Monitor de una señal analógica con la FRDM-MCXA156. Lee el potenciómetro cada 100 ms, controla una alarma con histéresis y ofrece telemetría y comandos por UART. El modo se puede cambiar con SW3 o desde la terminal.

## Placa y entorno

- Placa: NXP FRDM-MCXA156 (MCXA156).
- IDE: MCUXpresso IDE 25.6.136.
- SDK: MCUXpresso SDK 26.9.0.
- Proyecto generado desde el ejemplo `led_blinky` de NXP y configurado con MCUXpresso Config Tools.

## Pines del montaje

| Señal | Pin de MCU | Conector | Uso previsto | Estado inicial/configuración |
|---|---|---|---|---|
| LED de alarma externo | P3_12 | J1, pin 15 | GPIO de salida, activo alto | Apagado |
| LED de actividad externo | P3_13 | J1, pin 13 | GPIO de salida, activo alto | Apagado |
| Pulsador de placa SW3 | P0_6 | Integrado en la FRDM-MCXA156 | Entrada GPIO, activo bajo | Pull-up habilitado |
| Cursor del potenciómetro | P1_10 (ADC1_A8) | J4, pin 2 | Entrada analógica de 12 bits | ADC1_A8 |

Conecta cada LED externo con su ánodo hacia la salida GPIO a través de una resistencia en serie (330 Ω–1 kΩ) y su cátodo a GND. No conectes un LED directamente entre GPIO y tierra sin resistencia.

Para el potenciómetro de 10 kΩ, conecta los extremos a 3.3 V/VDDA y GND, y la pata central (cursor) a P1_10.

## Estado de implementación

Hito 1 configuró P3_12 y P3_13 como salidas inicialmente bajas y SW3/P0_6 como entrada con pull-up. Se retiró SysTick porque alternaba P3_12, pin reservado para la alarma.

En el Hito 2, CTIMER0 recibe FRO12M con divisor 1. El prescaler `PR=11` divide los 12 MHz entre 12 y deja el contador a 1 MHz. El match `MR0=1000`, con reset e interrupción, genera un tick nominal de 1 ms; la ISR sólo incrementa el contador de milisegundos. El lazo principal alterna P3_13 cada 500 ms sin retardos bloqueantes, por lo que un ciclo completo del LED dura 1 s.

## Transmisión UART (Hito 3)

LPUART0 utiliza P0_3 para TX y P0_2 para RX a través de MCU-Link VCOM. Configura la terminal serial a **115200 baudios, 8 bits, sin paridad, 1 bit de parada y sin control de flujo**. El reloj funcional usa FRO12M con divisor 1.

La UART usa colas por interrupción. El lazo consume hasta 32 bytes por vuelta y acepta líneas de hasta 32 caracteres con LF o CRLF. Ignora líneas vacías. Las líneas largas, inválidas o con comandos desconocidos reciben ERR al terminar.


## Adquisición analógica (Hito 4)

ADC1 lee el canal 8 (ADC1_A8) en P1_10/J4-2 cada 100 ms. Usa resolución de 12 bits, referencia VDDA (Alt3), calibración de offset y automática al inicio, y tiempo de muestreo ADCK131. El resultado del SDK se desplaza 3 bits para obtener el rango 0–4095. El porcentaje se calcula con redondeo: `(ADC × 100 + 2047) / 4095`. El porcentaje alimenta la lógica de alarma y aparece en la telemetría.

### Señal de diagnóstico del ADC

P3_0 cambia de nivel justo antes de cada disparo del ADC. Conecta la punta del osciloscopio a P3_0 y la tierra de la sonda a GND de la placa. Entre flancos consecutivos se esperan 100 ms. Entre dos flancos del mismo tipo se esperan 200 ms (5 Hz). P3_0 comparte la señal con el LED azul integrado, que es activo en bajo.

### Alarma con histéresis

El monitor arranca en RUN con THR=70 % y la alarma externa apagada. Cada muestra válida actualiza la alarma en P3_12. Se enciende con PCT mayor o igual a THR y se apaga con PCT menor que THR menos 5 puntos. En la banda intermedia conserva su estado. Con 60 %, 72 %, 67 % y 62 %, la alarma queda apagada, encendida, encendida y apagada, respectivamente. El LED externo es activo en alto. El rojo integrado comparte la salida y es activo en bajo.

### Modos RUN y PAUSE con SW3

SW3 (P0_6, activo en bajo) alterna RUN y PAUSE una vez por pulsación. Tanto la presión como la liberación deben permanecer estables durante 30 ms. Mantener presionado el botón no repite el cambio.

PAUSE apaga la alarma y mantiene el muestreo, la señal de diagnóstico, el LED de actividad y la atención UART. Al regresar a RUN, la alarma se decide de inmediato con la última muestra válida y el umbral vigente. Las muestras siguientes vuelven a aplicar la histéresis.

### STATUS y telemetría

El monitor transmite una línea de estado cada 1000 ms y también al recibir STATUS. Ambas salidas terminan en CRLF y usan el formato:

    t=1000;ADC=2048;PCT=50;THR=70;MODE=RUN;ALARM=0

t indica los milisegundos desde el inicio del temporizador, ADC es la última lectura de 12 bits, PCT es su porcentaje redondeado, THR es el umbral, MODE es RUN o PAUSE y ALARM vale 1 cuando la alarma está activa.

### Umbral por UART

THR n cambia el umbral a un entero decimal entre 10 y 95, inclusive. Por ejemplo, THR 75 responde OK THR=75. Una entrada inválida responde ERR y conserva el estado. En RUN se reevalúa la alarma con la última muestra válida. En PAUSE permanece apagada.

### Cambio de modo por UART

START activa RUN y responde OK RUN. STOP activa PAUSE y responde OK PAUSE. Ambos usan la misma transición que SW3. Repetir START en RUN conserva el estado de la alarma dentro de la banda de histéresis. En PAUSE continúan el muestreo, la actividad y la telemetría.

Los comandos distinguen mayúsculas y minúsculas. Si se detecta un error de recepción o se llena la cola RX, se descartan los datos pendientes y se espera el siguiente LF para recuperar el inicio de línea.

## Compilación y carga

1. Abre la carpeta raíz del proyecto. Necesitas el SDK indicado arriba, las herramientas ARM de MCUXpresso, CMake y Ninja. En `mcux_include.json`, ajusta `SdkRootDirPath` a la carpeta que contiene `mcuxsdk`, `ARMGCC_DIR` a la carpeta de herramientas ARM y `MCUX_VENV_PATH` a la carpeta `Scripts` del entorno Python de MCUXpresso.
2. Desde la raíz, configura y compila con el preset `debug`.

   ```sh
   cmake --preset debug
   cmake --build --preset debug
   ```

3. Conecta la placa por USB a **MCU-Link**. En VS Code con MCUXpresso, selecciona la sonda MCU-Link y carga `debug/SegundoParcial.elf` con la configuración **Debug**. Si se detiene en `main`, continúa la ejecución.
4. Abre el puerto **MCU-Link VCOM** a **115200 baudios, 8N1, sin control de flujo**. Envía los comandos con LF o CRLF.

## Verificación y demostración

- [Video de la demostración](https://youtu.be/UeBQF-Kku2U)
- [Resultados de las pruebas P01–P10](pruebas/resultados_P01_P10.md)
- [Traza UART de P02](pruebas/traza_P02_UART.txt)
- [Traza del banco en PC para P03, P05 y P10](pruebas/traza_simulacion_P03_P05_P10.txt)
