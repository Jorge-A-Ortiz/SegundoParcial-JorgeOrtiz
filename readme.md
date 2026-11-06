# Segundo Parcial — Monitor analógico con FRDM-MCXA156

**Estudiante:** Jorge Alberto Ortiz Nieves
**Matrícula:** 20250504

## Objetivo

Desarrollar por etapas un monitor de una señal analógica con la placa FRDM-MCXA156. El proyecto incorpora temporización, actividad y transmisión UART. La adquisición ADC está activa; la alarma, el estado de aplicación y los comandos se incorporarán en hitos posteriores.

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
| Cursor del potenciómetro | P1_10 (ADC1_A8) | J4, pin 2 | Entrada analógica prevista | Se configura en el hito ADC |

Conecta cada LED externo con su ánodo hacia la salida GPIO a través de una resistencia en serie (330 Ω–1 kΩ) y su cátodo a GND. No conectes un LED directamente entre GPIO y tierra sin resistencia.

Para el potenciómetro de 10 kΩ, conecta los extremos a 3.3 V/VDDA y GND, y la pata central (cursor) a P1_10. En este hito el canal analógico aún no se inicializa.

## Estado de implementación

Hito 1 configuró P3_12 y P3_13 como salidas inicialmente bajas y SW3/P0_6 como entrada con pull-up. Se retiró SysTick porque alternaba P3_12, pin reservado para la alarma.

En el Hito 2, CTIMER0 recibe FRO12M con divisor 1. El prescaler `PR=11` divide los 12 MHz entre 12 y deja el contador a 1 MHz. El match `MR0=1000`, con reset e interrupción, genera un tick nominal de 1 ms; la ISR sólo incrementa el contador de milisegundos. El lazo principal alterna P3_13 cada 500 ms sin retardos bloqueantes, por lo que un ciclo completo del LED dura 1 s. El firmware compila; la comprobación formal en la placa queda para el hito de pruebas.

## Transmisión UART (Hito 3)

LPUART0 utiliza P0_3 para TX y P0_2 para RX a través de MCU-Link VCOM. Configura la terminal serial a **115200 baudios, 8 bits, sin paridad, 1 bit de parada y sin control de flujo**. El reloj funcional usa FRO12M con divisor 1.

Al iniciar, se encola la línea provisional `UART lista` con terminación CRLF. La ISR transmite desde una cola circular de 256 bytes; el programa principal continúa atendiendo el LED de actividad. Si la terminal se abre después del arranque, pulsa RESET para ver la línea. La recepción UART está habilitada: la ISR almacena los bytes en una cola circular de 256 posiciones (255 útiles). En esta etapa todavía no se consumen ni se interpretan; si la cola se llena o aparece un error de recepción, se registra una falla y se descartan los bytes posteriores hasta reiniciar. El armado de líneas acepta LF y CRLF; ignora líneas vacías y CR aislado. Se aceptan hasta 32 caracteres sin contar el terminador. Si se excede el límite, se descarta la línea completa hasta LF y se encola una sola respuesta `ERR` al terminarla. Los bytes RX se consumen con un máximo de 32 por vuelta del lazo, fuera de la ISR. En este hito todas las líneas completas reciben `ERR`; los comandos de aplicación se añaden más adelante.


## Adquisición analógica (Hito 4)

ADC1 lee el canal 8 (ADC1_A8) en P1_10/J4-2 cada 100 ms. Usa resolución de 12 bits, referencia VDDA (Alt3), calibración de offset y automática al inicio, y tiempo de muestreo ADCK131. El resultado del SDK se desplaza 3 bits para obtener el rango 0–4095. El porcentaje se calcula con redondeo: `(ADC × 100 + 2047) / 4095`. El valor medido no controla todavía ningún LED ni se transmite por UART.

### Señal de diagnóstico del ADC

P3_0 cambia de nivel justo antes de cada disparo del ADC. Conecta la punta del osciloscopio a P3_0 y la tierra de la sonda a GND de la placa. Entre flancos consecutivos se esperan 100 ms. Entre dos flancos del mismo tipo se esperan 200 ms (5 Hz). P3_0 comparte la señal con el LED azul integrado, que es activo en bajo.

### Alarma con histéresis

El monitor arranca en RUN con THR=70 % y la alarma externa apagada. Cada muestra válida actualiza la alarma en P3_12. Se enciende con PCT mayor o igual a THR y se apaga con PCT menor que THR menos 5 puntos. En la banda intermedia conserva su estado. Con 60 %, 72 %, 67 % y 62 %, la alarma queda apagada, encendida, encendida y apagada, respectivamente. El LED externo es activo en alto. El rojo integrado comparte la salida y es activo en bajo.

### Modos RUN y PAUSE con SW3

SW3 (P0_6, activo en bajo) alterna RUN y PAUSE una vez por pulsación. Tanto la presión como la liberación deben permanecer estables durante 30 ms. Mantener presionado el botón no repite el cambio.

PAUSE apaga la alarma y mantiene el muestreo, la señal de diagnóstico, el LED de actividad y la atención UART. Al regresar a RUN, la alarma se decide de inmediato con la última muestra válida y el umbral vigente. Las muestras siguientes vuelven a aplicar la histéresis.
