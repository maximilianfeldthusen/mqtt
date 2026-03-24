
## MQTT Sensor Client v2 - Complete Documentation

A production-ready C MQTT client for IoT sensor data publishing with enhanced security, reliability, and offline capabilities.

## Overview

This MQTT client publishes simulated temperature and humidity sensor data to an MQTT broker. It includes four major production enhancements:

| Feature | Purpose |
| :--- | :--- |
| **mTLS Authentication** | Mutual TLS with client certificates for strong broker authentication |
| **JSON Schema Validation** | Validates payload structure before publishing using jansson |
| **Offline Message Queuing** | Persists messages to disk when broker is unavailable |
| **Systemd Integration** | Auto-start, crash recovery, and logging via systemd |

## Code Structure

```text
sensor_client_v2.c
├── Configuration & Constants
├── Signal Handlers (SIGINT)
├── Argument Parsing (--help, -a, -c, -t, etc.)
├── JSON Validation (validate_payload)
├── Queue Management (init_queue, save_to_queue, replay_queue)
├── Connection Logic (connect_with_retry)
└── Main Loop (publish, validate, queue on failure)Key Components Explained1. TLS Client Certificate Authentication (mTLS)ssl_opts.enableServerCertAuth = 1;
if (config.ca_cert) ssl_opts.trustStore = config.ca_cert;
if (config.client_cert) ssl_opts.keyStore = config.client_cert;
if (config.client_key) ssl_opts.privateKey = config.client_key;
if (config.key_password) ssl_opts.privateKeyPassword = config.key_password;What it does:

trustStore: CA certificate to verify the broker's identity
keyStore: Client certificate to prove your identity to the broker
privateKey: Client private key for signing operations
privateKeyPassword: Optional password for encrypted keys

Security benefit: Both client and server authenticate each other, preventing unauthorized access even if credentials are compromised.
2. JSON Schema Validation
int validate_payload(const char *payload) {
    json_error_t error;
    json_t *root = json_loads(payload, 0, &error);
    if (!root) return -1;

    json_t *temp = json_object_get(root, "temperature");
    json_t *hum = json_object_get(root, "humidity");
    json_t *ts = json_object_get(root, "timestamp");

    if (!temp || !hum || !ts ||
        !json_is_number(temp) || !json_is_number(hum) || !json_is_integer(ts)) {
        return -1;
    }
    return 0;
}
What it does:

Parses the JSON string using jansson
Checks that temperature, humidity, and timestamp fields exist
Verifies numeric types match expectations
Returns -1 if validation fails

Why it matters: Prevents malformed data from reaching the broker or downstream consumers. Invalid payloads are logged and skipped.
3. Offline Message Queuing
int save_to_queue(const char *payload) {
    char filename[512];
    snprintf(filename, sizeof(filename), "%s/msg_%ld_%d.json", QUEUE_DIR, time(NULL), rand());
   
    FILE *f = fopen(filename, "w");
    if (!f) return -1;
    fputs(payload, f);
    fclose(f);
    return 0;
}

int replay_queue(MQTTClient client, ...) {
    DIR *dir = opendir(QUEUE_DIR);
    while ((entry = readdir(dir)) != NULL) {
        // Read, validate, republish, delete on success
    }
    closedir(dir);
    return count;
}
Workflow:

On publish failure: Save payload to /var/spool/mqtt-queue/msg_<timestamp>.json
On reconnect: Scan queue directory, validate each message, republish
On successful delivery: Delete the file from disk

Data durability: Messages survive broker outages, network interruptions, and client restarts.
4. Systemd Service Integration
[Unit]
Description=MQTT Sensor Client with Queue
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/sensor_client_v2 -a ssl://broker.example.com:8883
Restart=on-failure
RestartSec=10
User=root
ExecStartPre=-mkdir -p /var/spool/mqtt-queue

[Install]
WantedBy=multi-user.target
Features:

Restart=on-failure: Automatically restarts if the client crashes
RestartSec=10: Waits 10 seconds before restarting
ExecStartPre: Ensures queue directory exists before starting
StandardOutput=journal: Logs to journalctl for debugging

Build Instructions
Dependencies
Ubuntu/Debian
sudo apt-get install gcc libpaho-mqtt-dev libssl-dev libjansson-dev
macOS
brew install libpaho-mqtt-c openssl jansson
CentOS/RHEL
sudo yum install gcc paho-mqtt-devel openssl-devel jansson-devel
Compile
Standard
gcc -o sensor_client_v2 sensor_client_v2.c \
    -lpaho-mqtt3c -lssl -lcrypto -ljansson
With Optimization
gcc -O2 -o sensor_client_v2 sensor_client_v2.c \
    -lpaho-mqtt3c -lssl -lcrypto -ljansson
Usage Examples
Basic Usage
./sensor_client_v2 -a ssl://broker.hivemq.com:8883
With mTLS
./sensor_client_v2 \
  -a ssl://broker.example.com:8883 \
  -k /etc/ssl/certs/ca.crt \
  --cert /etc/ssl/certs/client.crt \
  --key /etc/ssl/private/client.key \
  --key-pass "your_password"
With Authentication
./sensor_client_v2 \
  -a ssl://broker.example.com:8883 \
  -u myuser -p mypass \
  -t sensors/environment -i 10
Full Command-Line Options
FlagDescriptionDefault-a, --addressMQTT broker URLssl://your-mqtt-broker.com:8883-c, --client-idClient identifierAdvancedCClient-t, --topicPublish topicsensors/environment-q, --qosQoS level (0,1,2)1-i, --intervalSeconds between messages5-u, --usernameUsername for auth—-p, --passwordPassword for auth—-k, --ca-certCA certificate path—--certClient certificate (mTLS)—--keyClient private key (mTLS)—--key-passKey password (optional)—-h, --helpShow help—
Deployment Checklist

 Generate CA, client certificate, and private key
 Store certificates securely (chmod 600 for private keys)
 Create queue directory: mkdir -p /var/spool/mqtt-queue
 Set correct ownership: chown sensoruser:sensorgroup /var/spool/mqtt-queue
 Copy binary to /usr/local/bin/sensor_client_v2
 Install systemd service: cp mqtt-sensor.service /etc/systemd/system/
 Enable and start: systemctl enable mqtt-sensor.service && systemctl start mqtt-sensor.service
 Monitor logs: journalctl -u mqtt-sensor.service -f

Troubleshooting
IssueSolutionConnection failedVerify broker address, firewall rules, and certificate validityPermission deniedCheck file permissions on private key (chmod 600)Queue directory errorEnsure /var/spool/mqtt-queue exists and is writableJSON validation failedCheck sensor simulation logic for valid number rangesMessages not replayedVerify queue files exist and are valid JSON
Security Considerations

Private Key Protection: Store keys in secure locations with restricted permissions.
Certificate Rotation: Plan for certificate expiration and renewal.
Queue Directory Security: Restrict access to the queue folder to prevent tampering.
Network Security: Use SSL/TLS ports (8883) and avoid plaintext MQTT (1883) in production.
Credential Management: Avoid hardcoding passwords; use environment variables or secrets managers.

Future Enhancements

SQLite-based persistent queue for large message volumes
Prometheus metrics endpoint for monitoring
Dynamic configuration reload without restart
WebSocket transport support
Message compression for bandwidth-constrained networks



