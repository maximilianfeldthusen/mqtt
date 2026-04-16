
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include "MQTTClient.h"
#include <jansson.h>

#define DEFAULT_ADDRESS     "ssl://your-mqtt-broker.com:8883"
#define DEFAULT_CLIENTID    "AdvancedCClient"
#define DEFAULT_TOPIC       "sensors/environment"
#define DEFAULT_TELEMETRY_TOPIC "sensors/telemetry"
#define DEFAULT_QOS         1
#define DEFAULT_TIMEOUT_MS  10000L
#define DEFAULT_INTERVAL_S  5
#define QUEUE_DIR           "/var/spool/mqtt-queue"
#define MAX_PAYLOAD_SIZE    512

typedef struct {
    char *address;
    char *client_id;
    char *topic;
    char *telemetry_topic;
    int qos;
    long timeout_ms;
    int interval_s;
    char *ca_cert;
    char *client_cert;
    char *client_key;
    char *key_password;
    char *username;
    char *password;
} Config;

volatile sig_atomic_t running = 1;
int message_count = 0;

// --- Signal Handling ---
static void handle_sigint(int sig) {
    (void)sig;
    printf("\nShutdown signal received. Cleaning up...\n");
    running = 0;
}

// --- Utility Functions ---
static void print_usage(const char *prog_name) {
    printf(
        "Usage: %s [options]\n"
        "Options:\n"
        "  -a, --address <addr>        MQTT broker address\n"
        "  -c, --client-id <id>        Client ID\n"
        "  -t, --topic <topic>         Topic to publish\n"
        "  -T, --telemetry-topic <topic> Topic for telemetry\n"
        "  -q, --qos <level>           QoS level (0,1,2)\n"
        "  -i, --interval <sec>        Publish interval\n"
        "  -u, --username <user>       Username\n"
        "  -p, --password <pass>       Password\n"
        "  -k, --ca-cert <path>        CA certificate\n"
        "  --cert <path>               Client certificate\n"
        "  --key <path>                Client private key\n"
        "  --key-pass <pass>           Private key password\n"
        "  -h, --help                  Show help\n",
        prog_name
    );
}

// --- Command-line Parsing ---
Config parse_args(int argc, char *argv[]) {
    Config cfg = {
        .address = DEFAULT_ADDRESS,
        .client_id = DEFAULT_CLIENTID,
        .topic = DEFAULT_TOPIC,
        .telemetry_topic = DEFAULT_TELEMETRY_TOPIC,
        .qos = DEFAULT_QOS,
        .timeout_ms = DEFAULT_TIMEOUT_MS,
        .interval_s = DEFAULT_INTERVAL_S,
        .ca_cert = NULL,
        .client_cert = NULL,
        .client_key = NULL,
        .key_password = NULL,
        .username = NULL,
        .password = NULL
    };

    static struct option long_options[] = {
        {"address",   required_argument, 0, 'a'},
        {"client-id", required_argument, 0, 'c'},
        {"topic",     required_argument, 0, 't'},
        {"telemetry-topic", required_argument, 0, 'T'},
        {"qos",       required_argument, 0, 'q'},
        {"interval",  required_argument, 0, 'i'},
        {"username",  required_argument, 0, 'u'},
        {"password",  required_argument, 0, 'p'},
        {"ca-cert",   required_argument, 0, 'k'},
        {"cert",      required_argument, 0, 1001},
        {"key",       required_argument, 0, 1002},
        {"key-pass",  required_argument, 0, 1003},
        {"help",      no_argument,       0, 'h'},
        {0,0,0,0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "a:c:t:T:q:i:u:p:k:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'a': cfg.address = optarg; break;
            case 'c': cfg.client_id = optarg; break;
            case 't': cfg.topic = optarg; break;
            case 'T': cfg.telemetry_topic = optarg; break;
            case 'q': cfg.qos = atoi(optarg); break;
            case 'i': cfg.interval_s = atoi(optarg); break;
            case 'u': cfg.username = optarg; break;
            case 'p': cfg.password = optarg; break;
            case 'k': cfg.ca_cert = optarg; break;
            case 1001: cfg.client_cert = optarg; break;
            case 1002: cfg.client_key = optarg; break;
            case 1003: cfg.key_password = optarg; break;
            case 'h': print_usage(argv[0]); exit(0);
            default: print_usage(argv[0]); exit(1);
        }
    }

    return cfg;
}

// --- JSON Payload Validation ---
int validate_payload(const char *payload) {
    json_error_t error;
    json_t *root = json_loads(payload, 0, &error);
    if (!root) {
        fprintf(stderr, "JSON parse error: %s\n", error.text);
        return -1;
    }

    if (!json_is_object(root)) {
        json_decref(root);
        return -1;
    }

    json_t *temp = json_object_get(root, "temperature");
    json_t *hum = json_object_get(root, "humidity");
    json_t *ts = json_object_get(root, "timestamp");

    int valid = (temp && hum && ts &&
                 json_is_number(temp) &&
                 json_is_number(hum) &&
                 json_is_integer(ts));

    json_decref(root);
    return valid ? 0 : -1;
}

// --- Queue Management ---
int init_queue() {
    struct stat st = {0};
    if (stat(QUEUE_DIR, &st) == -1 && mkdir(QUEUE_DIR, 0755) != 0) {
        perror("Queue directory creation failed");
        return -1;
    }
    return 0;
}

int save_to_queue(const char *payload) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/msg_%ld_%d.json", QUEUE_DIR, time(NULL), rand());

    FILE *f = fopen(filepath, "w");
    if (!f) {
        perror("Queue file write failed");
        return -1;
    }
    fputs(payload, f);
    fclose(f);
    printf("Saved to queue: %s\n", filepath);
    return 0;
}

int replay_queue(MQTTClient client, const char *topic, int qos, long timeout_ms) {
    DIR *dir = opendir(QUEUE_DIR);
    if (!dir) return 0;

    struct dirent *entry;
    int replayed = 0;

    while ((entry = readdir(dir))) {
        if (strstr(entry->d_name, ".json")) {
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", QUEUE_DIR, entry->d_name);

            FILE *f = fopen(path, "r");
            if (!f) continue;

            char buffer[MAX_PAYLOAD_SIZE];
            if (fgets(buffer, sizeof(buffer), f) && validate_payload(buffer) == 0) {
                MQTTClient_message msg = MQTTClient_message_initializer;
                msg.payload = buffer;
                msg.payloadlen = (int)strlen(buffer);
                msg.qos = qos;
                msg.retained = 0;

                MQTTClient_deliveryToken token;
                int rc = MQTTClient_publishMessage(client, topic, &msg, &token);
                if (rc == MQTTCLIENT_SUCCESS) {
                    MQTTClient_waitForCompletion(client, token, timeout_ms);
                    remove(path);
                    replayed++;
                    printf("Replayed queued message: %s\n", entry->d_name);
                }
            }
            fclose(f);
        }
    }

    closedir(dir);
    return replayed;
}

// --- MQTT Connection ---
int connect_with_retry(MQTTClient *client, MQTTClient_connectOptions *opts, int max_attempts) {
    int rc, attempts = 0;
    while ((rc = MQTTClient_connect(*client, opts)) != MQTTCLIENT_SUCCESS) {
        if (++attempts >= max_attempts) return rc;
        printf("Connect attempt %d failed, retrying...\n", attempts);
        sleep(3);
    }
    printf("MQTT Connected\n");
    return MQTTCLIENT_SUCCESS;
}
