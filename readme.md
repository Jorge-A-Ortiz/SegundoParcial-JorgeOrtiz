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

## Pines y conexiones del montaje

Todas las conexiones del protoboard comparten GND. Desconecta la placa antes de cambiar el cableado.

| Desde la placa | Hacia | Función |
|---|---|---|
| J2 pin 16, VDDA_MCU | Un extremo del potenciómetro de 10 kΩ | Referencia analógica nominal de 3.3 V. |
| J3 pin 12, GND | Riel de tierra del protoboard | Retorno del otro extremo del potenciómetro y de ambos cátodos. |
| J4 pin 2, P1_10 (ADC1_A8) | Pata central del potenciómetro | Entrada analógica de 12 bits. El rango usado es de GND a VDDA. |
| J1 pin 13, P3_13 | Resistencia en serie de 330 Ω a 1 kΩ y ánodo del LED verde | Actividad. Cátodo a GND. Activo en alto. |
| J1 pin 15, P3_12 | Resistencia en serie de 330 Ω a 1 kΩ y ánodo del LED rojo | Alarma. Cátodo a GND. Activo en alto. |
| SW3 integrado, P0_6 | Pulsador de la placa | Activo en bajo con pull-up. |
| USB-C MCU-Link, J15 | PC | Alimentación, programación y puerto virtual LPUART0 por P0_3 (TX) y P0_2 (RX). |

```text
J2-16 VDDA ── extremo A [potenciómetro 10 kΩ] extremo B ── GND
                           cursor ── J4-2 (P1_10)
J1-13 (P3_13) ── R ── ánodo LED verde  cátodo ── GND
J1-15 (P3_12) ── R ── ánodo LED rojo   cátodo ── GND
J3-12 GND ── riel de tierra común del protoboard
```

Para la captura de P02, añade los puentes **P3_0 (J8 pin 7) → P3_14 (J1 pin 11)** y **P3_13 (J1 pin 13) → P3_15 (J1 pin 9)**. Consulta el [procedimiento de P02](pruebas/medicion_P02.md) y el [manual de NXP, figura 16 y tabla 16](https://www.nxp.com/docs/en/user-manual/UM12121.pdf).

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

## SDK y configuración de placa

- El proyecto usa los controladores GPIO, PORT, CTIMER, LPADC, LPUART e INPUTMUX del SDK oficial de NXP. El SDK se obtiene y se asocia como se indica abajo.
- [Configuración de FRDM-MCXA156](frdmmcxa156/): inicialización de hardware, pines, reloj, archivos de placa y proyecto de Config Tools. La configuración que compila el firmware está en los archivos C de esta carpeta.
- [Presets de CMake](CMakePresets.json) y [configuración de VS Code](.vscode/): compilación y depuración del MCXA156. El SDK completo aporta la infraestructura de CMake y Kconfig.

## Importación, compilación y carga

1. Clona este repositorio. Instala **MCUXpresso for VS Code** y usa **MCUXpresso Installer** para disponer de ARM GCC, CMake, Ninja, Python y LinkServer. Importa el **SDK 26.09.00 para FRDM-MCXA156** desde **Import Repository**. El [SDK oficial de NXP](https://mcuxpresso.nxp.com/mcuxsdk/26.09.00/) también está disponible como archivo.
2. En el panel **Projects** de MCUXpresso, selecciona **Import MCUXpresso for VS Code Project**, elige la carpeta raíz clonada y asocia el SDK y la cadena de herramientas instalados. Es el proyecto existente, con su configuración para **FRDM-MCXA156 / MCXA156 / Cortex-M33**. Consulta la [guía de importación de NXP](https://mcuxpresso.nxp.com/mcux-vscode/26.09/Project-Management.html#import-existing-project).
3. Si compilas desde la terminal, verifica en `mcux_include.json` que `SdkRootDirPath` apunte a la carpeta que contiene `mcuxsdk`, `ARMGCC_DIR` a la carpeta de herramientas ARM y `MCUX_VENV_PATH` al directorio `Scripts` del entorno Python de MCUXpresso en Windows. Usa las rutas de tu equipo. Desde la raíz ejecuta:

   ```powershell
   cmake --preset debug
   cmake --build --preset debug
   ```

4. Conecta el USB de **MCU-Link** y carga `debug/SegundoParcial.elf` con **Debug** en VS Code. Selecciona la sonda integrada. Si el depurador se detiene en `main`, pulsa **Continuar**. También puedes programar desde una terminal con LinkServer en PATH:

   ```powershell
   LinkServer flash MCXA156:FRDM-MCXA156 load debug/SegundoParcial.elf
   ```

5. Abre **MCU-Link VCOM** a **115200 baudios, 8N1, sin control de flujo**. Envía los comandos con LF o CRLF. El firmware normal arranca en RUN con THR=70 y emite telemetría cada segundo.

Las compilaciones de diagnóstico son opcionales. Sus instrucciones están en [P02](pruebas/medicion_P02.md) y [P10](pruebas/medicion_P10.md).

## Verificación y demostración

- [Video de la demostración](https://youtu.be/UeBQF-Kku2U)
- [Resultados de las pruebas P01–P10](pruebas/resultados_P01_P10.md)
- [Traza UART de P02](pruebas/traza_P02_UART.txt)
- [Traza del banco en PC para P03, P05 y P10](pruebas/traza_simulacion_P03_P05_P10.txt)
