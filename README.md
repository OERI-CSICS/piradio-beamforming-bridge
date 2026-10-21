# xApp–Pi-Radio Bridge
TCP/JSON control bridge that lets an O-RAN xApp (OAI + FlexRIC) steer beams and nulls on a Pi-Radio phased-array SDR via its MATLAB driver.

**A lightweight TCP/JSON control bridge that lets an O-RAN xApp steer the beams and nulls of a Pi-Radio phased-array SDR through its MATLAB driver.**

![Language: C](https://img.shields.io/badge/client-C-blue)
![Language: MATLAB](https://img.shields.io/badge/server-MATLAB-orange)
![Protocol: TCP/JSON](https://img.shields.io/badge/protocol-TCP%20%2F%20JSON-lightgrey)
![Status: research prototype](https://img.shields.io/badge/status-research%20prototype-yellow)

---

## Overview

In an O-RAN deployment, an xApp running on the Near-RT RIC (e.g., FlexRIC) observes the RAN over the E2 interface and makes control decisions. External RF front-ends such as the **Pi-Radio** phased-array SDR, however, are driven by a MATLAB codebase and are not reachable over E2.

This repository closes that gap. It provides:

- **`start_xapp_piradio_bridge.m`** — a MATLAB TCP server that receives JSON commands and translates them into Pi-Radio beamforming calls (`calc_w`, `configure_realtime`).
- **`xapp_piradio_client.c`** — a dependency-free C client (POSIX sockets) that sends those commands. It runs standalone as a CLI and is written so its functions can be embedded directly in a C-based xApp.

The bridge is the hardware-actuation layer for research on **adaptive beamforming and null steering driven by RAN measurements**, built on OpenAirInterface (OAI) 5G SA and FlexRIC.

**Control flow:** E2 measurements → xApp decision → JSON command over TCP → MATLAB computes beamforming weights → weights pushed to the Pi-Radio array → JSON acknowledgement (with timing) returned to the xApp.

## Repository structure

```
.
├── start_xapp_piradio_bridge.m   # MATLAB TCP server (Pi-Radio side)
├── xapp_piradio_client.c         # C client / xApp-side library
└── README.md
```

## Command protocol

Messages are single-line JSON objects terminated by `\n` (LF). Every request receives exactly one JSON response.

| Command | Request | Pi-Radio action |
|---|---|---|
| `PING` | `{"cmd":"PING"}` | None — health check |
| `SET_BEAM` | `{"cmd":"SET_BEAM","beam_deg":20.00}` | `sdr1.calc_w(20); sdr1.configure_realtime(0);` |
| `SET_BEAM_NULL` | `{"cmd":"SET_BEAM_NULL","beam_deg":20.00,"null_deg":-35.00}` | `sdr1.calc_w(20,-35); sdr1.configure_realtime(0);` |

**Response fields**

| Field | Present in | Meaning |
|---|---|---|
| `status` | all | `"OK"` or `"ERROR"` |
| `message` | `PING`, errors | Human-readable status or error text |
| `beam_deg` | beam commands | Applied main-lobe angle (degrees) |
| `null_deg` | `SET_BEAM_NULL` | Applied null angle (degrees) |
| `piradio_status` | beam commands | Return value of `configure_realtime` |
| `elapsed_sec` | beam commands | Time to compute and apply the weights (seconds) |

Unknown commands and runtime exceptions are returned as `{"status":"ERROR","message":"..."}` rather than crashing the server.

## Requirements

**MATLAB side**
- MATLAB R2021a or later (uses `tcpserver` and `configureCallback`)
- The Pi-Radio MATLAB codebase, with an initialized SDR object (`sdr1`) exposing `calc_w` and `configure_realtime`
- A connected Pi-Radio phased-array SDR

**Client / xApp side**
- Linux (tested with Ubuntu 22.04)
- GCC or Clang — no third-party libraries required
- FlexRIC
- Network reachability to the MATLAB host on the chosen port (default `5000`)

> The Pi-Radio MATLAB driver is **not** included in this repository. Obtain it from Pi-Radio.

## Getting started

Bring the system up in this order.

### 1. Start the Pi-Radio MATLAB bridge

In MATLAB, run your usual Pi-Radio initialization, then start the server:

```matlab
% ... Pi-Radio initialization that creates sdr1 ...
sdr1.set_mode('gnb');

server = start_xapp_piradio_bridge(sdr1, 5000);
```

Keep the returned `server` variable in the workspace. Clearing it closes the socket.

### 2. Start the FlexRIC Near-RT RIC and the OAI gNB

Start the `nearRT-RIC` from your FlexRIC build. Then start the OAI gNB with its E2 agent pointed at the RIC. Executable names and config files depend on your build and deployment.

### 3. Build and test the bridge client

```bash
gcc -Wall -Wextra -O2 -o xapp_piradio_client xapp_piradio_client.c

# Health check
./xapp_piradio_client 192.168.1.100 5000 ping

# Steer the main beam to +20°
./xapp_piradio_client 192.168.1.100 5000 beam 20

# Steer the beam to +20° and place a null at −35°
./xapp_piradio_client 192.168.1.100 5000 beam-null 20 -35
```

The client accepts angles from **−90° to +90°**. Narrow these limits to match the scan range of your Pi-Radio array.

### 4. Run the FlexRIC xApp

Start the xApp against the running RIC. When it decides on a new beam or null angle, it calls the bridge.

## Design notes and limitations

- **One request per connection (CLI mode).** The CLI opens a connection, sends one command, and closes. For closed-loop control, keep a persistent socket open from the xApp and reuse it to avoid connection setup overhead.
- **Single client.** MATLAB's `tcpserver` accepts one client at a time. Only one xApp should drive a given Pi-Radio.
- **Blocking round trip.** Each command blocks until MATLAB returns; `elapsed_sec` reports the weight-computation and hardware-configuration time so actuation latency can be measured.
- **No authentication or encryption.** Run the bridge on an isolated lab network or tunnel it (e.g., SSH port forwarding).


## Acknowledgements

Built on [OpenAirInterface](https://openairinterface.org/) and [FlexRIC](https://gitlab.eurecom.fr/mosaic5g/flexric). Pi-Radio hardware control relies on the MATLAB codebase provided by the Pi-Radio team.

## License

Released under the MIT License.
