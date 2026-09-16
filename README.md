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
````

---

# 1. System Architecture

The project is divided into three embedded components.

```text
                     EMBEDDED SYSTEM
                           │
          ┌────────────────┼────────────────┐
          │                │                │
          ▼                ▼                ▼
       Node A           Node B           Node C
      nRF5340          nRF52833         nRF52833
          │                │                │
          │                │                │
     Provisioner      Mesh Gateway      BLE Beacon
     Controller       BLE Scanner       Advertiser
          │                │                │
          └──── Bluetooth Mesh ────────────┘
```

The responsibilities are intentionally separated.

---

# 2. Node A – nRF5340 Mesh Provisioner / Controller

Hardware:

```text
nRF5340 DK
```

Node A is the Bluetooth Mesh Provisioner and embedded Mesh Controller.

Responsibilities:

- Initialize Bluetooth.
- Initialize Bluetooth Mesh.
- Act as Mesh Provisioner.
- Detect unprovisioned Mesh nodes.
- Provision Node B.
- Configure Node B.
- Add and bind the Mesh AppKey.
- Configure the vendor model.
- Receive BLE beacon reports from Node B.
- Process Mesh vendor messages.
- Provide the higher-level controller interface toward P1 ULTRON.

Node A Mesh address:

```text
Primary Address: 0x0001
NetIdx:          0x0000
AppIdx:          0x0001
```

Node A UUID:

```text
A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 AA AB AC AD AE AF
```

Node A uses a vendor model:

```text
Company ID : 0x1234
Model ID   : 0x0001
```

---

# 3. Node B – nRF52833 Mesh Gateway / BLE Scanner

Hardware:

```text
nRF52833 DK
```

Node B performs two major functions.

### BLE Scanner

Node B scans for BLE advertisements from Node C.

It extracts:

- Bluetooth address
- RSSI
- Company ID
- Protocol version
- Message type
- Beacon Node ID
- Sequence number
- Payload length

### Bluetooth Mesh Node

Node B is also a Bluetooth Mesh node.

It contains:

```text
Configuration Server
Vendor Model
Mesh Provisionee
Mesh Relay
```

Node B therefore acts as the bridge between the BLE beacon world and the Bluetooth Mesh network.

Conceptually:

```text
BLE Advertisement
       │
       ▼
BLE Scanner
       │
       ▼
Beacon Parser
       │
       ▼
Mesh Application Message
       │
       ▼
Bluetooth Mesh
```

Node B Mesh address:

```text
0x0002
```

Node B UUID:

```text
B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 BA BB BC BD BE BF
```

---

# 4. Node C – nRF52833 BLE Beacon

Hardware:

```text
nRF52833 DK
```

Node C operates as a BLE advertiser.

It does not need to establish a BLE connection with Node B.

Instead, it periodically broadcasts manufacturer-specific advertising data.

Advertising interval:

```text
100 ms
```

The beacon contains:

- Company ID
- Protocol version
- Message type
- Node ID
- Sequence number
- Payload length

Example advertising data:

```text
FF FF 01 01 00 01 00 00 00
```

The fields are:

```text
FF FF       Company ID
01          Protocol Version
01          Message Type
00 01       Node ID
00 00       Sequence Number
00          Payload Length
```

The beacon can therefore be detected by any compatible BLE scanner without requiring a BLE connection.

---

# 5. BLE Beacon Protocol

The current beacon advertising format is:

```text
+--------+------------------+
| Bytes  | Field            |
+--------+------------------+
| 0-1    | Company ID       |
| 2      | Protocol Version |
| 3      | Message Type     |
| 4-5    | Node ID          |
| 6-7    | Sequence Number  |
| 8      | Payload Length   |
+--------+------------------+
```

Current values:

```text
Company ID       = 0xFFFF
Protocol Version = 1
Message Type     = 1
Node ID          = 0x0001
Payload Length   = 0
```

The sequence number is intended to identify successive beacon transmissions.

RSSI is not transmitted by Node C.

Instead, Node B obtains RSSI locally when it receives the advertisement.

---

# 6. Bluetooth Mesh Architecture

The Mesh network is:

```text
                Bluetooth Mesh
                     │
                     ▼
              ┌─────────────┐
              │     A       │
              │ nRF5340     │
              │ Provisioner │
              │ Controller  │
              └──────┬──────┘
                     │
                     │ Mesh
                     │
              ┌──────▼──────┐
              │     B       │
              │ nRF52833    │
              │ Mesh Node   │
              │ + Scanner   │
              └─────────────┘
```

Node A provisions Node B using PB-ADV.

After provisioning, Node B becomes a member of the Bluetooth Mesh network.

---

# 7. Mesh Provisioning Workflow

The provisioning sequence is:

```text
A starts
   │
   ▼
Bluetooth initialized
   │
   ▼
Bluetooth Mesh initialized
   │
   ▼
Provisioner initialized
   │
   ▼
A waits for unprovisioned nodes
   │
   ▼
B sends unprovisioned Mesh beacon
   │
   ▼
A detects B UUID
   │
   ▼
UUID matches expected B UUID
   │
   ▼
A starts PB-ADV provisioning
   │
   ▼
Provisioning completed
   │
   ▼
B receives Mesh address
   │
   ▼
B becomes provisioned Mesh node
```

Node B receives:

```text
Primary Address = 0x0002
```

---

# 8. Mesh Configuration Workflow

After provisioning, Node A performs Mesh configuration.

The configuration sequence is:

```text
Provision B
    │
    ▼
Add AppKey
    │
    ▼
Bind AppKey to B Vendor Model
    │
    ▼
Bind AppKey to A Vendor Model
    │
    ▼
Vendor Model communication enabled
    │
    ▼
Send Vendor PING
```

The application key used in the current prototype is:

```text
AppIdx = 0x0001
```

---

# 9. Vendor Model

The project uses a Bluetooth Mesh vendor model for application-specific communication.

Current identifiers:

```text
Company ID = 0x1234
Model ID   = 0x0001
```

The vendor model contains application-specific opcodes.

### Vendor PING

```text
Opcode = BT_MESH_MODEL_OP_3(0x01, 0x1234)
```

Purpose:

```text
A → B
```

It verifies that:

- provisioning works
- AppKey configuration works
- model binding works
- vendor message transmission works
- vendor message reception works

Expected result:

```text
VENDOR PING RECEIVED
Source : 0x0001
NetIdx : 0x0000
AppIdx : 0x0001
```

---

# 10. BLE Beacon Report Message

The second vendor opcode is used to transport BLE beacon observations through Mesh.

```text
Opcode = BT_MESH_MODEL_OP_3(0x02, 0x1234)
```

It is called:

```text
VENDOR_OP_BEACON_REPORT
```

Message direction:

```text
B → A
```

The payload is 5 bytes.

```text
+--------+----------------+
| Bytes  | Field          |
+--------+----------------+
| 0-1    | Beacon Node ID |
| 2-3    | Sequence       |
| 4      | RSSI           |
+--------+----------------+
```

Example:

```text
01 00 00 00 DA
```

Meaning:

```text
Beacon ID = 0x0001
Sequence  = 0
RSSI      = -38 dBm
```

---

# 11. Complete End-to-End Workflow

The complete system operates as follows.

```text
STEP 1
C Beacon starts
       │
       ▼
BLE advertising starts
       │
       │
       │ Manufacturer Data
       ▼
------------------------------------------------

STEP 2
B starts BLE Observer
       │
       ▼
B receives C advertisement
       │
       ▼
Parse manufacturer data
       │
       ├── Company ID
       ├── Protocol
       ├── Message Type
       ├── Beacon ID
       └── Sequence
       │
       ▼
Read RSSI
       │
       ▼
Create Beacon Report

------------------------------------------------

STEP 3
B creates Mesh Vendor Message
       │
       ▼
VENDOR_OP_BEACON_REPORT
       │
       ├── Beacon ID
       ├── Sequence
       └── RSSI
       │
       ▼
Bluetooth Mesh
       │
       ▼
A nRF5340

------------------------------------------------

STEP 4
A receives Vendor Message
       │
       ▼
Vendor model handler
       │
       ▼
Parse payload
       │
       ├── Source
       ├── NetIdx
       ├── AppIdx
       ├── Beacon ID
       ├── Sequence
       └── RSSI
       │
       ▼
Beacon observation available
```

---

# 12. Proven Data Path

The currently demonstrated path is:

```text
C nRF52833
BLE Beacon
     │
     │ BLE Advertisement
     │
     ▼
B nRF52833
BLE Scanner
     │
     │ Parse Advertisement
     │
     │ Beacon ID
     │ Sequence
     │ RSSI
     │
     ▼
Bluetooth Mesh
Vendor Model
     │
     │ BEACON_REPORT
     │
     ▼
A nRF5340
Mesh Controller
```

A successful received report appears as:

```text
========================================
       BEACON REPORT RECEIVED
========================================
Source       : 0x0002
NetIdx       : 0x0000
AppIdx       : 0x0001
Beacon ID    : 0x0001
Sequence     : 0
RSSI         : -38 dBm
========================================
```

This confirms the following chain:

```text
BLE ADV
   ↓
BLE Scanner
   ↓
Beacon Parser
   ↓
Mesh Vendor Message
   ↓
Mesh Transport
   ↓
Vendor Model Handler
   ↓
Beacon Observation
```

---

# 13. Raw Mesh Message Example

A received Mesh message can appear in the log as:

```text
len 8: c2341201000000da
```

The bytes can be interpreted as:

```text
C2 34 12 01 00 00 00 DA
│  │     │        │     │
│  │     │        │     └── RSSI = -38 dBm
│  │     │        └──────── Sequence = 0
│  │     └───────────────── Beacon ID = 0x0001
│  └─────────────────────── Vendor Company ID
└────────────────────────── Vendor Opcode
```

The application receives:

```text
Opcode     : 0x00C21234
Source     : 0x0002
Destination: 0x0001
AppIdx     : 0x0001
```

---

# 14. Project Phases

## Phase 1 – Cross-Vendor Bluetooth Mesh

Target architecture:

```text
nRF5340 Provisioner
        │
        ▼
nRF52833 Mesh Node
        │
        ▼
QPG6105 Mesh Node
```

The intended architecture supports standard Bluetooth Mesh concepts and models for interoperability.

Standard SIG models can be used where appropriate.

Examples:

```text
Generic OnOff
Sensor Model
Configuration Models
Health Model
```

The vendor model is used where application-specific beacon data is required.

---

# 15. Phase 2 – Independent BLE Beacons

Two independent BLE beacons can be operated:

```text
nRF52833 Beacon
       │
       │ BLE ADV
       ▼
    Scanner


QPG6105 Beacon
       │
       │ BLE ADV
       ▼
    Scanner
```

Beacon validation parameters include:

```text
Asset ID
Advertising Interval
RSSI
Detection Range
Packet Loss
Power Consumption
```

---

# 16. Phase 3 – BLE Scanning + Bluetooth Mesh

The key integration concept is:

```text
BLE Beacon
     │
     │ Advertisement
     ▼
Mesh Node Scanner
     │
     │ Parse BLE packet
     │
     │ Convert BLE observation
     │ into Mesh application data
     ▼
Bluetooth Mesh
     │
     ▼
Mesh Controller
```

This allows BLE advertising devices to become trackable through a Mesh infrastructure.

---

# 17. Phase 4 – P1 ULTRON Integration

The larger architecture is:

```text
       BLE BEACONS
       ┌───────────────┐
       │               │
       ▼               ▼
 Qorvo Beacon     nRF Beacon
       │               │
       └───────┬───────┘
               ▼
       Mesh Scanner Nodes
               │
               ▼
       Bluetooth Mesh
               │
               ▼
       nRF5340 Controller
               │
           UART / USB
               │
               ▼
          P1 ULTRON
               │
          MQTT / Redis
               │
               ▼
          P3 JARVIS
```

The embedded nRF5340 provides the Mesh-side controller interface.

P1 ULTRON is intended to provide the higher-level Linux control/application layer.

---

# 18. Software Architecture

The project uses:

```text
Nordic nRF Connect SDK
        │
        ▼
Zephyr RTOS
        │
        ├── Bluetooth LE
        ├── Bluetooth Mesh
        ├── BLE Observer
        ├── Mesh Provisioning
        ├── Mesh Configuration
        └── Vendor Model
```

Current development environment:

```text
NCS Version    : v2.7.0
Zephyr         : v3.6.99-ncs2
West           : v1.2.0
Zephyr SDK     : 0.16.5
GCC            : 12.2.0
```

---

# 19. Repository Structure

Recommended repository layout:

```text
nrf-mesh-ble-asset-tracking/
│
├── README.md
├── .gitignore
│
├── A_nrf5340_provisioner/
│   ├── CMakeLists.txt
│   ├── prj.conf
│   └── src/
│       └── main.c
│
├── B_nrf52833_gateway/
│   ├── CMakeLists.txt
│   ├── prj.conf
│   └── src/
│       └── main.c
│
├── C_nrf52833_beacon/
│   ├── CMakeLists.txt
│   ├── prj.conf
│   └── src/
│       └── main.c
│
└── docs/
    └── architecture/
```

Build directories should not be committed.

---

# 20. Development Environment – Windows vs Linux

The firmware is platform-independent at the source-code level. The same Zephyr/nRF Connect SDK projects can be developed on either Windows or Linux.

The difference is the host development environment:

```text
                 SAME GITHUB SOURCE
                        │
             ┌──────────┴──────────┐
             │                     │
             ▼                     ▼
        WINDOWS PC              LINUX PC
             │                     │
       VS Code / NCS          VS Code / NCS
             │                     │
             └──────────┬──────────┘
                        ▼
                 Same Zephyr
                  Firmware
                        │
             ┌──────────┼──────────┐
             ▼          ▼          ▼
          Node A      Node B      Node C
         nRF5340    nRF52833    nRF52833
```

## Windows

Recommended host environment:

```text
Windows 10/11
VS Code
nRF Connect for VS Code
PowerShell / Command Prompt
nRF Connect SDK v2.7.0
Zephyr v3.6.99-ncs2
nrfutil
PuTTY
```

Example project location:

```text
C:\Users\harik\Desktop\nrf-mesh-ble-asset-tracking\
```

Start the NCS toolchain:

```powershell
nrfutil sdk-manager toolchain launch --ncs-version v2.7.0 --terminal
```

Windows uses drive-letter paths such as:

```text
C:\ncs\v2.7.0
C:\Users\harik\Desktop\...
```

Serial ports are normally exposed as:

```text
COM4
COM5
COM6
COM7
```

Current development console mapping:

```text
Node A - nRF5340 : COM6
Node B - nRF52833 : COM5
```

Open Node A:

```cmd
putty.exe -serial COM6 -sercfg 115200,8,n,1,N
```

Open Node B:

```cmd
putty.exe -serial COM5 -sercfg 115200,8,n,1,N
```

The COM number is not guaranteed to remain the same on another Windows installation. Check the actual device before opening the console.

## Linux

The same source tree can be used on Linux.

Recommended host environment:

```text
Ubuntu/Linux
VS Code
nRF Connect SDK v2.7.0
Zephyr v3.6.99-ncs2
nrfutil
West
J-Link / Nordic programming tools
screen or picocom
```

Example project location:

```text
~/nrf-mesh-ble-asset-tracking/
```

or:

```text
/home/<user>/nrf-mesh-ble-asset-tracking/
```

Linux uses POSIX paths instead of Windows drive-letter paths.

For example:

```text
Windows:
C:\Users\harik\Desktop\nrf-mesh-ble-asset-tracking

Linux:
~/nrf-mesh-ble-asset-tracking
```

Serial devices are normally exposed as:

```text
/dev/ttyACM0
/dev/ttyACM1
/dev/ttyACM2
```

Check connected devices:

```bash
nrfutil device list
```

For serial-port discovery:

```bash
ls /dev/ttyACM*
```

A console can be opened with:

```bash
screen /dev/ttyACM0 115200
```

or:

```bash
picocom -b 115200 /dev/ttyACM0
```

The actual `/dev/ttyACM*` number must be determined from the connected hardware.

## Windows vs Linux Command Differences

| Operation | Windows | Linux |
|---|---|---|
| Terminal | PowerShell / CMD | Bash |
| Path format | `C:\Users\...\project` | `~/project` |
| Serial port | `COM5` | `/dev/ttyACM0` |
| Console | PuTTY | `screen` / `picocom` |
| Build system | `west` | `west` |
| Flash tool | `nrfutil` | `nrfutil` |
| Source code | Same | Same |
| `prj.conf` | Same | Same |
| `CMakeLists.txt` | Same | Same |
| NCS version | v2.7.0 | v2.7.0 |
| Zephyr version | v3.6.99-ncs2 | v3.6.99-ncs2 |
| Target hardware | Same | Same |

## Important Rule

Do not create separate firmware implementations just because the host operating system is different.

The intended model is:

```text
Windows PC
    │
    │ Build / Flash
    ▼
Zephyr Firmware
    │
    ▼
nRF5340 / nRF52833
```

and:

```text
Linux PC
    │
    │ Build / Flash
    ▼
Zephyr Firmware
    │
    ▼
nRF5340 / nRF52833
```

The embedded firmware remains the same.

Only the host-side commands, paths, serial device names, and development tools can differ.

## Moving the Project from Windows to Linux

When cloning the repository on Linux:

```bash
git clone <REPOSITORY_URL>
cd nrf-mesh-ble-asset-tracking
```

Verify the development environment:

```bash
west --version
arm-zephyr-eabi-gcc --version
nrfutil --version
```

Then build the required node.

The same procedure applies when moving from Linux back to Windows: clone the repository, enter the corresponding NCS environment, and build using the Windows paths.

## Linux and P1 ULTRON

Linux has an additional role in the larger architecture.

The embedded A/B/C firmware runs on the Nordic devices:

```text
C nRF52833
    │
    │ BLE
    ▼
B nRF52833
    │
    │ Bluetooth Mesh
    ▼
A nRF5340
```

The higher-level controller can run on Linux:

```text
A nRF5340
    │
    │ UART / USB
    ▼
P1 ULTRON
    │
    ├── MQTT
    └── Redis
         │
         ▼
     P3 JARVIS
```

Therefore, Linux can be both:

1. A development/build/flash host for the Zephyr firmware.
2. The runtime host for the higher-level P1 ULTRON controller.

These are separate roles.

The nRF5340 continues to execute the embedded Bluetooth Mesh Provisioner/Controller firmware; Linux does not replace the nRF5340 firmware.

---

# 20. Building Node A

Enter the NCS environment:

```powershell
nrfutil sdk-manager toolchain launch --ncs-version v2.7.0 --terminal
```

Build:

```cmd
cd /d C:\ncs\v2.7.0

west build -b nrf5340dk/nrf5340/cpuapp C:\Users\harik\Desktop\nrf-mesh-ble-asset-tracking\A_nrf5340_provisioner --pristine always
```

---

# 21. Flashing Node A

Application core:

```cmd
C:\Users\harik\nrfutil\nrfutil.exe device program --firmware "C:\ncs\v2.7.0\build\zephyr\zephyr.hex" --serial-number 1050003647 --family nrf53 --core application --options reset=RESET_NONE,verify=VERIFY_READ
```

Network core:

```cmd
C:\Users\harik\nrfutil\nrfutil.exe device program --firmware "C:\Users\harik\Desktop\nrf_ble_gateway\build_A_v2\hci_ipc\zephyr\zephyr.hex" --serial-number 1050003647 --family nrf53 --core network --options reset=RESET_NONE,verify=VERIFY_READ
```

Reset:

```cmd
C:\Users\harik\nrfutil\nrfutil.exe device reset --serial-number 1050003647 --family nrf53
```

Console:

```cmd
putty.exe -serial COM6 -sercfg 115200,8,n,1,N
```

---

# 22. Building Node B

Build Node B using the nRF52833 DK target.

The B firmware contains:

```text
Bluetooth Observer
Bluetooth Broadcaster
Bluetooth Mesh
Mesh Provisionee
Mesh Relay
Configuration Server
Vendor Model
BLE Beacon Parser
Beacon Report Sender
```

The current successful build was approximately:

```text
FLASH: 194832 B / 512 KB = 37.16%
RAM  : 39356 B / 128 KB = 30.03%
```

---

# 23. Flashing Node B

Current Node B device:

```text
Serial Number : 685688495
Console       : COM5
```

Flash:

```cmd
C:\Users\harik\nrfutil\nrfutil.exe device program --firmware "C:\ncs\v2.7.0\build\zephyr\zephyr.hex" --serial-number 685688495 --family nrf52 --options reset=RESET_NONE,verify=VERIFY_READ
```

Reset:

```cmd
C:\Users\harik\nrfutil\nrfutil.exe device reset --serial-number 685688495 --family nrf52
```

Console:

```cmd
putty.exe -serial COM5 -sercfg 115200,8,n,1,N
```

---

# 24. Building Node C

Node C is the BLE beacon.

Build using the nRF52833 DK target and the C beacon project.

The beacon advertises every:

```text
100 ms
```

Console example:

```text
C BEACON STARTING
Device Name: C_BEACON
Node ID: 0x0001
Protocol: 1
Message Type: 1
Adv Interval: 100 ms
Bluetooth initialized successfully
Beacon advertising started successfully
```

---

# 25. Testing Procedure

The recommended test sequence is:

```text
1. Flash C
2. Verify BLE advertising
3. Flash B
4. Verify B detects C
5. Flash A
6. Verify Mesh provisioning
7. Verify AppKey configuration
8. Verify Vendor Model binding
9. Verify Vendor PING
10. Verify Beacon Report
```

---

# 26. Test 1 – Verify BLE Beacon

Start C.

Expected:

```text
Beacon advertising started successfully
```

Verify:

```text
Company ID
Protocol
Message Type
Node ID
Sequence
Advertising interval
```

---

# 27. Test 2 – Verify B Detects C

B should print:

```text
C BEACON DETECTED
Address  : XX:XX:XX:XX:XX:XX
RSSI     : -XX dBm
Company  : 0xFFFF
Protocol : 1
Type     : 1
Node ID  : 0x0001
Sequence : X
Payload  : 0 bytes
```

This confirms:

```text
C → BLE ADV → B
```

---

# 28. Test 3 – Verify Mesh Provisioning

A should detect B's unprovisioned Mesh beacon.

Expected workflow:

```text
UNPROVISIONED MESH BEACON
        │
        ▼
UUID MATCHED
        │
        ▼
PB-ADV provisioning
        │
        ▼
B PROVISIONING COMPLETE
```

B should eventually report:

```text
B IS NOW A PROVISIONED MESH NODE
Addr : 0x0002
```

---

# 29. Test 4 – Verify Vendor Model

A configures the Vendor Model.

Expected:

```text
SUCCESS: B Vendor Model bound
```

and:

```text
SUCCESS: A Vendor Model bound
```

Then A sends:

```text
VENDOR PING
```

B should receive:

```text
VENDOR PING RECEIVED
Source : 0x0001
NetIdx : 0x0000
AppIdx : 0x0001
```

---

# 30. Test 5 – Verify Beacon Report

B detects C.

B converts:

```text
BLE Advertisement
```

into:

```text
BEACON_REPORT
```

The message is sent:

```text
B → Bluetooth Mesh → A
```

A should display:

```text
========================================
       BEACON REPORT RECEIVED
========================================
Source       : 0x0002
NetIdx       : 0x0000
AppIdx       : 0x0001
Beacon ID    : 0x0001
Sequence     : X
RSSI         : -XX dBm
========================================
```

This is the primary end-to-end validation of the prototype.

---

# 31. Memory Usage

Node A current application/network memory usage has been measured during development.

Application/network utilization should be checked after every significant feature addition.

Example current measurements:

```text
Application FLASH : 171196 B / 1 MB
Application RAM   : 43484 B / 448 KB

Network FLASH     : 166356 B / 256 KB
Network RAM       : 54328 B / 64 KB
```

Node B:

```text
FLASH : 194832 B / 512 KB
RAM   : 39356 B / 128 KB
```

Memory usage is dependent on the exact SDK configuration and enabled features.

---

# 32. RTOS and Stack Considerations

Bluetooth Mesh provisioning and access-layer processing can generate significant workqueue activity.

During development, Node A experienced a stack overflow/Usage Fault associated with workqueue usage.

The configuration workqueue stack was increased to:

```text
2048 bytes
```

Bluetooth RX stack:

```text
4096 bytes
```

This configuration resolved the observed stack issue during provisioning testing.

For production optimization, stack usage should be measured using the final feature set rather than simply reducing stack sizes.

---

# 33. Design Characteristics

The architecture intentionally separates:

```text
BLE Advertising
       │
       ▼
BLE Scanning
       │
       ▼
Application Parsing
       │
       ▼
Mesh Transport
       │
       ▼
Controller
       │
       ▼
Linux Backend
```

This separation makes it possible to replace individual components without redesigning the complete system.

For example:

```text
nRF52833 Beacon
```

can potentially be replaced by:

```text
QPG6105 Beacon
```

while maintaining the same higher-level tracking concept.

---

# 34. Cross-Vendor Interoperability

Bluetooth Mesh interoperability depends on both devices implementing compatible Bluetooth Mesh specifications, provisioning procedures, bearer support, configuration procedures, and models.

Standard SIG models are preferred when interoperability with multiple vendors is required.

Vendor models are used for application-specific functionality.

The project therefore separates:

```text
Standard Mesh functionality
```

from:

```text
Application-specific Beacon Report functionality
```

---

# 35. Why Bluetooth Mesh?

BLE advertising alone is limited by the scanner's physical detection range.

Bluetooth Mesh provides an infrastructure for forwarding information between distributed nodes.

The architecture therefore combines:

```text
BLE Beacon
      +
BLE Scanner
      +
Bluetooth Mesh
      +
Controller
```

This allows the system to scale conceptually from a direct BLE scanner architecture toward a distributed asset-observation network.

---

# 36. Future Development

Potential next stages include:

### Multiple Beacons

```text
C1 ─┐
C2 ─┤
C3 ─┤
C4 ─┤
    ▼
    B
```

B maintains observations for multiple beacon IDs.

### Multiple Scanner Nodes

```text
        C1
        │
   ┌────┼────┐
   ▼    ▼    ▼
  B1    B2   B3
   │    │    │
   └────┼────┘
        ▼
        A
```

### Multiple Mesh Hops

```text
B1 → B2 → B3 → A
```

### Beacon Tracking Database

P1 ULTRON can maintain:

```text
Asset ID
Scanner ID
RSSI
Timestamp
Sequence
Mesh Source
```

### MQTT Integration

```text
nRF5340
   │
 UART/USB
   ▼
P1 ULTRON
   │
 MQTT
   ▼
Backend
```

### Redis Integration

Redis can be used for:

```text
Current asset state
Last-seen timestamp
Scanner information
RSSI history
```

---

# 37. Current Milestone

The following functionality has been demonstrated:

```text
[✓] nRF52833 BLE Beacon
[✓] BLE Manufacturer Data
[✓] BLE Advertising
[✓] nRF52833 BLE Observer
[✓] Beacon Advertisement Parsing
[✓] RSSI Extraction
[✓] nRF5340 Mesh Provisioner
[✓] nRF52833 Mesh Provisionee
[✓] PB-ADV Provisioning
[✓] Mesh AppKey Configuration
[✓] Vendor Model
[✓] Vendor Model Binding
[✓] Vendor PING
[✓] BLE → Mesh Gateway Conversion
[✓] Beacon Report Vendor Message
[✓] nRF5340 Beacon Report Reception
```

End-to-end demonstrated path:

```text
C Beacon
   │
   │ BLE Advertisement
   ▼
B BLE Scanner
   │
   │ Beacon Report
   ▼
B Mesh Node
   │
   │ Bluetooth Mesh
   ▼
A nRF5340
   │
   ▼
Beacon Report Handler
```

---

# 38. Final Architecture

```text
                         BLE ASSET TRACKING
                              SYSTEM
                                │
              ┌─────────────────┴─────────────────┐
              │                                   │
              ▼                                   ▼
       BLE Beacon Layer                    Mesh Infrastructure
              │                                   │
       ┌──────┴──────┐                     ┌──────┴──────┐
       │             │                     │             │
       ▼             ▼                     ▼             ▼
   nRF52833       QPG6105              nRF52833       nRF5340
    Beacon         Beacon               Gateway       Controller
       │             │                     │             │
       └──────┬──────┘                     │             │
              │                            │             │
              ▼                            ▼             │
        BLE Advertising              BLE Scanner         │
                                           │             │
                                           ▼             │
                                    Beacon Parser        │
                                           │             │
                                           ▼             │
                                    Vendor Message       │
                                           │             │
                                           └──────┬──────┘
                                                  │
                                           Bluetooth Mesh
                                                  │
                                                  ▼
                                           nRF5340 Controller
                                                  │
                                               UART/USB
                                                  │
                                                  ▼
                                            P1 ULTRON
                                                  │
                                            MQTT / Redis
                                                  │
                                                  ▼
                                            P3 JARVIS
```

---

# 39. Project Summary

This project demonstrates a multi-layer BLE asset-tracking architecture in which BLE advertising is used for low-overhead asset identification and Bluetooth Mesh is used as the distributed transport infrastructure.

The fundamental data path is:

```text
BLE Beacon
    ↓
BLE Advertisement
    ↓
Mesh Node Scanner
    ↓
Beacon Parsing
    ↓
Mesh Vendor Message
    ↓
Bluetooth Mesh
    ↓
nRF5340 Mesh Controller
    ↓
P1 ULTRON
    ↓
MQTT / Redis
    ↓
Backend / Fleet Management
```

The embedded prototype establishes the foundation for a distributed, multi-vendor BLE asset observation system.

```
