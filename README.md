# Multi-Vendor Bluetooth Mesh–Assisted BLE Asset Tracking

A Bluetooth Low Energy (BLE) asset-tracking prototype that combines BLE beacon advertising, BLE scanning, Bluetooth Mesh communication, and a Linux-side controller architecture.

The system demonstrates how a BLE beacon can be detected by a Bluetooth Mesh node, converted into a Mesh application message, transported through Bluetooth Mesh, and received by an nRF5340 Mesh Provisioner/Controller.

The project is implemented using Nordic Semiconductor nRF52/nRF53 devices and Zephyr RTOS / nRF Connect SDK.

---

## Project Overview

The system contains three embedded nodes:

| Node | Hardware | Role |
|------|----------|------|
| A | nRF5340 DK | Bluetooth Mesh Provisioner / Controller |
| B | nRF52833 DK | Bluetooth Mesh Node + BLE Scanner / Gateway |
| C | nRF52833 DK | BLE Beacon |

The basic data flow is:

```text
                    BLE Advertisement
                ┌──────────────────────┐
                │                      │
                ▼                      │
        ┌─────────────────┐            │
        │ C - nRF52833    │            │
        │ BLE Beacon      │            │
        │                 │            │
        │ Asset ID        │            │
        │ Sequence        │            │
        └────────┬────────┘            │
                 │                     │
                 │ BLE ADV             │
                 ▼                     │
        ┌─────────────────┐            │
        │ B - nRF52833    │            │
        │ Mesh Gateway    │            │
        │                 │            │
        │ BLE Scanner     │            │
        │ Beacon Parser   │            │
        │ Mesh Node       │            │
        └────────┬────────┘            │
                 │                     │
                 │ Bluetooth Mesh      │
                 │ Vendor Message      │
                 ▼                     │
        ┌─────────────────┐
        │ A - nRF5340     │
        │ Mesh Controller │
        │                 │
        │ Provisioner     │
        │ Controller      │
        │                 │
        │ Beacon Report   │
        └────────┬────────┘
                 │
                 │ UART / USB
                 ▼
        ┌─────────────────┐
        │ P1 ULTRON       │
        │ Linux Controller│
        │                 │
        │ MQTT / Redis    │
        └────────┬────────┘
                 │
                 ▼
        ┌─────────────────┐
        │ P3 JARVIS       │
        │ Fleet / Backend │
        └─────────────────┘
