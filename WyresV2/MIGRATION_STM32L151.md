# Migration status: STM32L151 bring-up

This project was migrated from STM32WLE5 scaffolding to an STM32L151CC bring-up profile for W_BASE V2 REVC.

What is now migrated:
- Target set to `STM32L151CCUx` in `.cproject`
- New startup and linker profile for STM32L151
- Minimal firmware bring-up in `Src/main.c`:
  - LED1 on `PA0`
  - LED2 on `PB3`
  - UART debug on `USART1` (`PA9/PA10`, 115200)
- WyresV2 app logic reintegrated:
  - state machine (`OFF/BOOTING/DISCOVERING/INSERTED/ALERT`)
  - heartbeat tick behavior
  - demo periodic text send path
  - files: `Src/wyresv2_node.c`, `Src/platform_port_stub.c`, `Src/main.c`

What is intentionally stubbed:
- Former STM32WL HAL/radio/subghz integration files remain disabled with temporary stubs.
- Radio backend for STM32L151 is still a platform stub (`platform_radio_send/init/process`).

Next steps:
- Replace radio stub with the real WyresV2 radio backend.
- Re-enable protocol routing (pred/succ) and ACK/retransmission path.
- Migrate the remaining modules from stubs to STM32L1-compatible implementations.
