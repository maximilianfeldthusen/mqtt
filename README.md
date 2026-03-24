
## MQTT Sensor Client v2 - Documentation Explanation

## What This Documentation Covers

This is a **production-ready C MQTT client** designed for IoT sensor applications. It's built to handle real-world deployment challenges like network failures, security requirements, and system integration.

---

## Documentation Structure Breakdown

### 1. Overview Section
**Purpose:** High-level feature summary
**What it tells you:**
- Four core production enhancements (mTLS, JSON validation, offline queuing, systemd)
- Target use case: Temperature/humidity sensor data publishing
- Technology stack: C with Paho MQTT library

### 2. Code Structure
**Purpose:** Visual map of the source file organization
**Why it matters:**
```text
sensor_client_v2.c
├── Configuration & Constants      # Hardcoded settings, paths, limits
├── Signal Handlers (SIGINT)       # Graceful shutdown on Ctrl+C
├── Argument Parsing               # CLI flag handling (-a, -c, -t, etc.)
├── JSON Validation                # Payload integrity checks
├── Queue Management               # Offline message persistence
├── Connection Logic               # Retry logic, connection establishment
└── Main Loop                      # Core publish-validate-queue workflow3. Key Components ExplainedmTLS Authentication
What: Mutual TLS using client certificates
Security value: Both broker AND client authenticate each other
Implementation: Uses OpenSSL trustStore/keyStore/privateKey configuration
JSON Schema Validation
What: Validates payload before publishing using jansson library
Why: Prevents malformed data from corrupting downstream systems
Fields checked: temperature, humidity, timestamp (all must be numeric)
Offline Message Queuing
What: Persists failed messages to disk for later replay
Location: /var/spool/mqtt-queue/msg_<timestamp>.json
Workflow:

Publish fails → save to disk
Reconnect → scan queue directory
Republish → delete on success

Systemd Integration
What: Linux service management configuration
Benefits: Auto-restart on crash, logging via journalctl, boot-time startup
4. Build Instructions
Purpose: How to compile the client
Dependencies:

libpaho-mqtt-dev - MQTT protocol library
libssl-dev - TLS/SSL support
libjansson-dev - JSON parsing

Compilation flags:
# Standard build
gcc -o sensor_client_v2 sensor_client_v2.c -lpaho-mqtt3c -lssl -lcrypto -ljansson

# Optimized build
gcc -O2 -o sensor_client_v2 sensor_client_v2.c -lpaho-mqtt3c -lssl -lcrypto -ljansson
5. Usage Examples
Purpose: Practical command-line invocation patterns
ScenarioExampleBasic./sensor_client_v2 -a ssl://broker.hivemq.com:8883mTLSAdd -k, --cert, --key flagsAuthAdd -u username and -p password
6. Command-Line Options Table
Purpose: Complete reference for all flags
Key flags:

-a / --address - Broker URL (required)
-t / --topic - MQTT topic to publish to
-q / --qos - Quality of Service level (0, 1, or 2)
-i / --interval - Seconds between messages

7. Deployment Checklist
Purpose: Step-by-step production deployment guide
Critical items:

Certificate generation and secure storage (chmod 600 for private keys)
Queue directory creation with proper ownership
Systemd service installation and enabling

8. Troubleshooting Table
Purpose: Common issues and solutions
Most frequent problems:

Connection failures (check firewall, certificates)
Permission denied (check key file permissions)
Queue errors (verify directory exists and is writable)

9. Security Considerations
Purpose: Production security best practices
Key points:

Private keys must have restricted permissions
Use SSL/TLS ports (8883), avoid plaintext (1883)
Plan certificate rotation before expiration
Don't hardcode credentials

10. Future Enhancements
Purpose: Planned improvements roadmap
Notable items:

SQLite for larger message volumes
Prometheus metrics for monitoring
WebSocket transport option


Who Should Use This Documentation?
AudienceFocus AreaDevelopersCode structure, build instructions, API usageDevOps EngineersSystemd integration, deployment checklistSecurity TeamsmTLS setup, certificate management, security considerationsOperationsTroubleshooting, log monitoring, queue management

Quick Reference Summary
ComponentTechnologyPurposeMQTT LibraryPaho MQTT CProtocol implementationTLSOpenSSLSecure communicationJSONJanssonPayload parsing/validationService ManagerSystemdProcess lifecycle managementQueue StorageFile systemOffline message persistence

Next Steps After Reading

Review security section before deploying to production
Test locally with a public broker (HiveMQ, Mosquitto)
Generate certificates for mTLS if required
Configure systemd for automatic startup
Monitor logs via journalctl -u mqtt-sensor.service -f




