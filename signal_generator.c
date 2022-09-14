#include <math.h>
#include <stdio.h>
#include <string.h>

#include "clock.pio.h"
#include "eightbit.pio.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#define SIZE 125000

uint clock(PIO pio, uint sm, uint pin, uint32_t high, uint32_t low) {

  uint32_t offset;

  offset = pio_add_program(pio, &clock_program);
  clock_program_init(pio, sm, offset, pin, 1, 0);

  // intrinsic delays - these are certainly 3 cycles
  high -= 3;
  low -= 3;

  // load low into OSR then copy to ISR
  pio->txf[sm] = low;
  pio_sm_exec(pio, sm, pio_encode_pull(false, false));
  pio_sm_exec(pio, sm, pio_encode_out(pio_isr, 32));

  // load high into OSR
  pio->txf[sm] = high;
  pio_sm_exec(pio, sm, pio_encode_pull(false, false));
}

int main() {

  // 0th dac pin -> LSB -> 2...9 inclusive used for DAC output
  static const uint dac0 = 2;

  // dac timer pin - will rise high at start of cycle
  static const uint dact = 10;

  PIO pio = pio0;

  setup_default_uart();

  uint sm_dac = pio_claim_unused_sm(pio, true);
  uint sm_clk = pio_claim_unused_sm(pio, true);

  uint off_dac = pio_add_program(pio, &eightbit_program);

  uint off_clk = clock(pio, sm_clk, dact, 62500, 62500);

  // set up data array - SIZE elements to allow time for DMA restart
  // 2 * pi / 1250. to give 100 kHz

  uint8_t data[SIZE];

  for (int j = 0; j < 125000; j++) {
    data[j] =
        127 + 128 * (sin(2 * M_PI * j / 1250.) * sin(2 * M_PI * j / 12500.) *
                     sin(2 * M_PI * j / 125000.));
  }

  printf("Init\n");
  eightbit_program_init(pio, sm_dac, off_dac, dac0);

  // set up DMA
  uint32_t dma_a, dma_b;
  dma_channel_config dmc_a, dmc_b;

  // grab DMA slots
  dma_a = dma_claim_unused_channel(true);
  dma_b = dma_claim_unused_channel(true);
  dmc_a = dma_channel_get_default_config(dma_a);
  dmc_b = dma_channel_get_default_config(dma_b);

  // configure A channel
  channel_config_set_transfer_data_size(&dmc_a, DMA_SIZE_32);
  channel_config_set_dreq(&dmc_a, pio_get_dreq(pio, sm_dac, true));
  channel_config_set_read_increment(&dmc_a, true);
  channel_config_set_write_increment(&dmc_a, false);
  channel_config_set_chain_to(&dmc_a, dma_b);

  // configure B channel
  channel_config_set_transfer_data_size(&dmc_b, DMA_SIZE_32);
  channel_config_set_dreq(&dmc_b, pio_get_dreq(pio, sm_dac, true));
  channel_config_set_read_increment(&dmc_b, true);
  channel_config_set_write_increment(&dmc_b, false);
  channel_config_set_chain_to(&dmc_b, dma_a);

  // SIZE / 4 as DMA is moving 4 byte words
  dma_channel_configure(dma_a, &dmc_a, (volatile void *)&(pio->txf[sm_dac]),
                        (const volatile void *)data, SIZE / 4, false);

  dma_channel_start(dma_a);
  printf("DMA started\n");

  pio_enable_sm_mask_in_sync(pio, 1 << sm_dac | 1 << sm_clk);
  printf("PIO started\n");

  while (true) {
    // (re-)configure B, wait for A
    dma_channel_configure(dma_b, &dmc_b, (volatile void *)&(pio->txf[sm_dac]),
                          (const volatile void *)data, SIZE / 4, false);
    dma_channel_wait_for_finish_blocking(dma_a);

    // (re-)configure A, wait for B
    dma_channel_configure(dma_a, &dmc_a, (volatile void *)&(pio->txf[sm_dac]),
                          (const volatile void *)data, SIZE / 4, false);
    dma_channel_wait_for_finish_blocking(dma_b);
  }
}