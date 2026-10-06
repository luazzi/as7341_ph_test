# Módulo PIO — Controle WS2812B (Programmable I/O)

Este diretório contém a implementação em assembly PIO para geração do sinal de controle dos LEDs endereçáveis **WS2812B** (matriz 5×5 da placa BitDogLab v7), executado diretamente no periférico PIO do microcontrolador RP2350 (Raspberry Pi Pico 2 W).

---

## Arquivos

- [ws2812.pio](file:///c:/Users/luana/Documents/IC/a7341/a734x/a734x_test/firmware/pio/ws2812.pio) — Código assembly PIO e rotina C de inicialização da State Machine.

---

## Visão Geral do Protocolo WS2812B

Os LEDs WS2812B utilizam um protocolo serial de 1 fio (*single-wire*) baseado em pulsos com temporização precisa de alta frequência (800 kHz, período de 1,25 µs por bit):

- **Bit '0'**: Pulso alto curto (~0,35 µs) seguido de nível baixo longo (~0,90 µs).
- **Bit '1'**: Pulso alto longo (~0,70 µs) seguido de nível baixo curto (~0,60 µs).
- **Reset**: Linha em nível baixo por período superior a 280 µs (o firmware utiliza 500 µs).
- **Ordem de transmissão de cor**: 24 bits no formato **GRB** (Green [8 bits], Red [8 bits], Blue [8 bits]), MSB primeiro.

---

## Implementação no RP2350 PIO

O microcontrolador RP2350 possui blocos de hardware PIO (Programmable Input/Output) dedicados. O programa [ws2812.pio](file:///c:/Users/luana/Documents/IC/a7341/a734x/a734x_test/firmware/pio/ws2812.pio) implementa um gerador de temporização baseado em *side-set* de 1 pino com temporização balanceada de 10 ciclos por bit:

- **T1** = 3 ciclos (nível ALTO inicial comum para bit 0 e 1)
- **T2** = 3 ciclos (nível dependente do dado: ALTO para bit 1, BAIXO para bit 0)
- **T3** = 4 ciclos (nível BAIXO final comum)

Total = 10 ciclos por bit transmitido.

### Código Assembly PIO

```pasm
.program ws2812
.side_set 1

.define public T1 3
.define public T2 3
.define public T3 4

.wrap_target
bitloop:
    out x, 1       side 0 [T3 - 1] ; Garante nível baixo anterior e extrai bit do FIFO
    jmp !x do_zero side 1 [T1 - 1] ; Inicia pulso alto; desvia se bit == 0
do_one:
    jmp  bitloop   side 1 [T2 - 1] ; Mantém alto se bit == 1 e reinicia loop
do_zero:
    nop            side 0 [T2 - 1] ; Desce para baixo se bit == 0
.wrap
```

### Inicialização via C SDK

A função auxiliar `ws2812_program_init()` configura:
- Pino GPIO correspondente (GPIO 7 na BitDogLab v7).
- Shift register com autopull para palavras de 24 bits (formato GRB) ou 32 bits (RGBW).
- Divisor de clock fracionário para sincronizar 10 ciclos em 800 kHz:
  $$\text{div} = \frac{f_{\text{sys}}}{800\,000 \times 10}$$
- Habilitação da State Machine e junção de FIFOs de transmissão (`PIO_FIFO_JOIN_TX`).

---

## Geração Automática pelo CMake

Durante o processo de compilação, o utilitário `pioasm` do Raspberry Pi Pico SDK processa `ws2812.pio` e gera automaticamente o cabeçalho `ws2812.pio.h` na pasta de build (`build/ws2812.pio.h`), que é então incluído pelos aplicativos em `firmware/apps/`.
