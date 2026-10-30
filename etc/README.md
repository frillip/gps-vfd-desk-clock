# etc

Odds and ends from the V2 firmware. They describe the V2 firmware, which the current firmware has replaced, along with most of them:

| File | What | Now |
| --- | --- | --- |
| `serial_proto.txt` | V2's 0x83 PIC–ESP messages | Replaced by SML, [common/SML_PROTOCOL.md](../common/SML_PROTOCOL.md) |
| `serial_cmds.txt` | V2's console commands | Replaced, see [Console commands](../README.md#console-commands) |
| `menu_structure.txt` | V2's menu on the tubes | Reworked, see [Menu](../dspic_src/README.md#menu) |
| `gnss_config.txt` | A u-center dump of the NEO-M9N's configuration for V2 | Still loads with u-center, but the current firmware also needs NAV-TIMELS, NAV-EOE and TIM-TP on UART1, see [GNSS receiver](../dspic_src/README.md#gnss-receiver) |
| `i2cscanner/`, `pic_uart_usb/` | Arduino sketches for the ESP: an I2C bus scan, and a USB-UART bridge to the PIC | Bench tools, as they were |
