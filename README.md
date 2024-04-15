# asteroid-tap2ble

This daemon creates a TAP interface and exposes a BLE service with a RX and a
TX characteristic. These map to read/write operations on that TAP interface.

The protocol takes into account fragmentation due to the BLE MTU so each BLE
message is prepended with a one-byte header that contains a sequence number (to
drop missed messages) and a bit to specify the end of a message.

This effectively exposes IP connectivity from the watch to companion apps where
this TAP traffic can be injected as RAW sockets or fed to daemons like passt.
