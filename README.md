# Raspberry Pi Pico 8-bit signal generator

Deterministic time signal generator - 8 bit output at up to 125 MHz into a
resistor DAC to give an analogue signal updated at 125 MHz. Notes:

- 125 MHz is derived from the PIO clock speed so can be slowed down easily.
- 125 MHz updates have to be near to one another as in my case the capacitance
  of the circuit was non-trivial.

Detailed docs and analysis [here](./doc/DOC.md).
