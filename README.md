


# Advanced MQTT Embedded Publisher

[![C](https://img.shields.io/badge/language-C-blue.svg)](https://www.gnu.org/software/gnu-c-manual/)  
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)  

An advanced MQTT client for embedded systems featuring:

- Offline message queueing
- JSON payload validation
- TLS support with certificates
- Telemetry reporting (CPU and memory usage)
- Robust retry and error handling

---

## Features

- Modular and maintainable C code
- Publishes telemetry and sensor data
- Handles broker disconnections with local queue replay
- Configurable via command-line options
- Senior-style code: clear variable names, logging, and formatting

---

## Requirements

- C compiler (GCC recommended)
- [Paho MQTT C Client](https://www.eclipse.org/paho/index.php?page=clients/c/index.php)
- [Jansson JSON Library](https://digip.org/jansson/)
- Linux or POSIX-compliant OS for `sysinfo()` telemetry

---

## Installation

1. Clone the repository:

```bash
git clone https://github.com/yourusername/mqtt-embedded-publisher.git
cd mqtt-embedded-publisher
````

2. Install dependencies:

```bash
sudo apt-get install libpaho-mqtt-dev libjansson-dev
```

3. Compile:

```bash
gcc -o mqtt_publisher mqtt_publisher.c -lpaho-mqtt3c -ljansson
```

---

## Usage

```bash
./mqtt_publisher [OPTIONS]
```

### Options

| Option                      | Description                                           |
| --------------------------- | ----------------------------------------------------- |
| `--address`                 | MQTT broker address (default: `tcp://localhost:1883`) |
| `--client-id`               | MQTT client ID (default: `embedded_publisher`)        |
| `--topic`                   | Sensor data topic (default: `sensors/data`)           |
| `--telemetry-topic`         | Telemetry topic (default: `sensors/telemetry`)        |
| `--qos`                     | MQTT QoS level (0, 1, or 2; default: 1)               |
| `--interval`                | Publish interval in seconds (default: 5)              |
| `--username` / `--password` | MQTT authentication credentials                       |
| `--ca-cert`                 | Path to CA certificate for TLS                        |
| `--client-cert`             | Path to client certificate for TLS                    |
| `--client-key`              | Path to client private key for TLS                    |
| `--key-password`            | Password for private key (if required)                |

---

## Example Output

```text
MQTT Connected
Replayed 3 queued messages
[1] Published: {"temperature":21.34,"humidity":42.11,"timestamp":1681700000,"count":1}
Telemetry published: {"uptime":12345,"loadavg":0.52,"totalram":104857600,"freeram":52428800}
[2] Published: {"temperature":20.87,"humidity":45.23,"timestamp":1681700005,"count":2}
Telemetry published: {"uptime":12350,"loadavg":0.48,"totalram":104857600,"freeram":52200000}
...
Shutdown complete. Total messages: 250
```

---

## How It Works

1. **Initialization:** Parses command-line arguments and sets up MQTT client.
2. **Offline Queue:** Messages that fail to publish are stored locally.
3. **MQTT Connection:** Attempts reconnect with retries if broker is unreachable.
4. **Telemetry:** Periodically publishes system info (CPU load, memory usage) to a dedicated topic.
5. **Main Loop:** Publishes simulated sensor data at configurable intervals.
6. **Graceful Shutdown:** Handles Ctrl+C and cleans up resources.

---

## File Structure

* `mqtt_publisher.c` — Main program file
* `README.md` — Project documentation
* `LICENSE` — MIT license

---


## Requirements

- C compiler (GCC recommended)
- [Paho MQTT C Client](https://www.eclipse.org/paho/index.php?page=clients/c/index.php)
- [Jansson JSON Library](https://digip.org/jansson/)
- Linux or POSIX-compliant OS for `sysinfo()` telemetry

---

## Installation

1. Clone the repository:

```bash
git clone https://github.com/yourusername/mqtt-embedded-publisher.git
cd mqtt-embedded-publisher
Install dependencies:
sudo apt-get install libpaho-mqtt-dev libjansson-dev
Compile:
gcc -o mqtt_publisher mqtt_publisher.c -lpaho-mqtt3c -ljansson
