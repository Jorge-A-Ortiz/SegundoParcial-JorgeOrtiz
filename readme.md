# Segundo Parcial — Monitor analógico con FRDM-MCXA156

**Estudiante:** Jorge Alberto Ortiz Nieves
**Matrícula:** 20250504

## Objetivo

Desarrollar por etapas un monitor de una señal analógica con la placa FRDM-MCXA156. Este hito prepara los pines del montaje y documenta el proyecto; la adquisición ADC, la temporización, la alarma y el protocolo UART se incorporarán en hitos posteriores.

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

En este hito, `pin_mux.c` configura P3_12 y P3_13 como salidas inicialmente bajas y SW3/P0_6 como entrada con pull-up. También se retiró el SysTick del ejemplo, que alternaba P3_12 y habría interferido con el LED externo de alarma. El programa aún no implementa el monitor: la temporización de aplicación se incorporará con CTIMER0 en el siguiente hito. Si se regeneran los archivos con MCUXpresso Config Tools, habrá que conservar estas asignaciones y mantener SysTick deshabilitado.
