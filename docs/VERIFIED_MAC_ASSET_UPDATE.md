# Project Update

The current verified prototype includes BLE MAC forwarding from the scanner gateway to the nRF5340 controller and a master-side BLE asset table.

Current embedded path:

```text
C nRF52833 BLE Beacon
        |
        | BLE Advertisement
        v
B nRF52833 BLE Scanner / Mesh Relay
        |
        | Vendor BEACON_REPORT
        v
A nRF5340 Mesh Master / Provisioner
        |
        v
BLE Asset Table
```

Current BEACON_REPORT payload:

```text
Byte 0-5   : BLE MAC address
Byte 6-7   : Beacon ID
Byte 8-9   : Sequence
Byte 10    : RSSI
Total      : 11 bytes
```

The A-side asset table currently tracks Asset ID, BLE MAC, scanner Mesh address, RSSI, sequence, packets seen, last-seen time, and ACTIVE/TIMEOUT state.

Physical range has not yet been experimentally characterized. C-to-B BLE distance and B-to-A Mesh-hop distance remain test parameters. The current prototype has verified end-to-end communication and RSSI reporting, but no maximum distance value is claimed.

The latest verified runtime path is:

```text
BLE advertisement
    -> B scanner detects beacon and BLE MAC
    -> B creates 11-byte Mesh BEACON_REPORT
    -> A receives the report
    -> A updates the BLE asset table
```
