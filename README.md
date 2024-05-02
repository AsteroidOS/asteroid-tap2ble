asteroid-tap2ble
================

This daemon creates a TAP interface and exposes a BLE service with a RX and a
TX characteristic. These map to read/write operations on that TAP interface.

D-Bus forwarding
----------------

To expose D-Bus to the companion:

1.  Add a line containing `ListenStream=55556` right above the other
    `ListenStream=…` in `/usr/lib/systemd/user/dbus.socket`
2.  Enable anonymous authentication in `/usr/share/dbus-1/session.conf`
    by adding `<allow_anonymous/>` and `<auth>ANONYMOUS</auth>`
    to the `<busconfig>`
