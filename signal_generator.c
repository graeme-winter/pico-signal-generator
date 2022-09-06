#include <math.h>
#include <stdio.h>
#include <string.h>

#include "eightbit.pio.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#define SIZE 100000

int main() {

  static const uint led_pin = 2;

  PIO pio = pio0;

  setup_default_uart();

  uint sm = pio_claim_unused_sm(pio, true);

  uint offset = pio_add_program(pio, &eightbit_program);

  // set up data array - SIZE elements to allow time for DMA restart
  // 2 * pi / 1250. to give 100 kHz

  uint8_t data[SIZE];

  for (int j = 0; j < 1250; j++) {
    data[j] = 127 + 128 * sin(2 * M_PI * j / 1250.);
  }

  for (int j = 1; j < (SIZE / 1250); j++) {
    memcpy(data + 1250 * j, data, 1250);
  }

  printf("Init\n");
  eightbit_program_init(pio, sm, offset, led_pin);

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
  channel_config_set_dreq(&dmc_a, pio_get_dreq(pio, sm, true));
  channel_config_set_read_increment(&dmc_a, true);
  channel_config_set_write_increment(&dmc_a, false);
  channel_config_set_chain_to(&dmc_a, dma_b);

  // configure B channel
  channel_config_set_transfer_data_size(&dmc_b, DMA_SIZE_32);
  channel_config_set_dreq(&dmc_b, pio_get_dreq(pio, sm, true));
  channel_config_set_read_increment(&dmc_b, true);
  channel_config_set_write_increment(&dmc_b, false);
  channel_config_set_chain_to(&dmc_b, dma_a);

  // SIZE / 4 as DMA is moving 4 byte words
  dma_channel_configure(dma_a, &dmc_a, (volatile void *)&(pio->txf[sm]),
                        (const volatile void *)data, SIZE / 4, false);

  pio_sm_set_enabled(pio, sm, true);
  printf("PIO started\n");

  dma_channel_start(dma_a);
  printf("DMA started\n");

  while (true) {
    // (re-)configure B, wait for A
    dma_channel_configure(dma_b, &dmc_b, (volatile void *)&(pio->txf[sm]),
                          (const volatile void *)data, SIZE / 4, false);
    dma_channel_wait_for_finish_blocking(dma_a);

    // (re-) configure A, wait for B
    dma_channel_configure(dma_a, &dmc_a, (volatile void *)&(pio->txf[sm]),
                          (const volatile void *)data, SIZE / 4, false);
    dma_channel_wait_for_finish_blocking(dma_b);
  }
}
