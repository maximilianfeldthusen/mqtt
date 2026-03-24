
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
#include "MQTTClient.h"
#include <jansson.h>


#define DEFAULT_ADDRESS     "ssl://your-mqtt-broker.com:8883"
#define DEFAULT_CLIENTID    "AdvancedCClient"
#define DEFAULT_TOPIC       "sensors/environment"
#define DEFAULT_QOS         1
#define DEFAULT_TIMEOUT     10000L
#define DEFAULT_INTERVAL    5
#define QUEUE_DIR           "/var/spool/mqtt-queue"


typedef struct {
    char *address;
    char *clientid;
    char *topic;
    int qos;
    long timeout;
    int interval;
    char *ca_cert;
    char *client_cert;
    char *client_key;
    char *key_password;
    char *username;
    char *password;
} Config;


volatile int running = 1;
int message_count = 0;


void handle_sigint(int sig) {
    printf("\nShutdown signal received. Flushing queue and exiting...\n");
    running = 0;
}


void print_usage(const char *prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -a, --address <addr>      MQTT broker address\n");
    printf("  -c, --client-id <id>      Client ID\n");
    printf("  -t, --topic <topic>       Topic to publish\n");
    printf("  -q, --qos <level>         QoS level (0,1,2)\n");
    printf("  -i, --interval <sec>      Publish interval\n");
    printf("  -u, --username <user>     Username\n");
    printf("  -p, --password <pass>     Password\n");
    printf("  -k, --ca-cert <path>      CA Certificate path\n");
    printf("  --cert <path>             Client Certificate path (for mTLS)\n");
    printf("  --key <path>              Client Private Key path (for mTLS)\n");
    printf("  --key-pass <pass>         Private Key password (optional)\n");
    printf("  -h, --help                Show help\n");
}


Config parse_args(int argc, char *argv[]) {
    Config config = {
        .address = DEFAULT_ADDRESS,
        .clientid = DEFAULT_CLIENTID,
        .topic = DEFAULT_TOPIC,
        .qos = DEFAULT_QOS,
        .timeout = DEFAULT_TIMEOUT,
        .interval = DEFAULT_INTERVAL,
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
        {"qos",       required_argument, 0, 'q'},
        {"interval",  required_argument, 0, 'i'},
        {"username",  required_argument, 0, 'u'},
        {"password",  required_argument, 0, 'p'},
        {"ca-cert",   required_argument, 0, 'k'},
        {"cert",      required_argument, 0, 1001},
        {"key",       required_argument, 0, 1002},
        {"key-pass",  required_argument, 0, 1003},
        {"help",      no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };


    int opt;
    while ((opt = getopt_long(argc, argv, "a:c:t:q:i:u:p:k:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'a': config.address = optarg; break;
            case 'c': config.clientid = optarg; break;
            case 't': config.topic = optarg; break;
            case 'q': config.qos = atoi(optarg); break;
            case 'i': config.interval = atoi(optarg); break;
            case 'u': config.username = optarg; break;
            case 'p': config.password = optarg; break;
            case 'k': config.ca_cert = optarg; break;
            case 1001: config.client_cert = optarg; break;
            case 1002: config.client_key = optarg; break;
            case 1003: config.key_password = optarg; break;
            case 'h': print_usage(argv[0]); exit(0);
            default: print_usage(argv[0]); exit(1);
        }
    }
    return config;
}


// --- JSON Validation ---
int validate_payload(const char *payload) {
    json_error_t error;
    json_t *root = json_loads(payload, 0, &error);
    if (!root) {
        fprintf(stderr, "JSON Validation Failed: %s\n", error.text);
        return -1;
    }


    if (!json_is_object(root)) {
        json_decref(root);
        return -1;
    }


    json_t *temp = json_object_get(root, "temperature");
    json_t *hum = json_object_get(root, "humidity");
    json_t *ts = json_object_get(root, "timestamp");


    if (!temp || !hum || !ts ||
        !json_is_number(temp) || !json_is_number(hum) || !json_is_integer(ts)) {
        json_decref(root);
        return -1;
    }


    json_decref(root);
    return 0;
}


// --- Queue Management ---
int init_queue() {
    struct stat st = {0};
    if (stat(QUEUE_DIR, &st) == -1) {
        if (mkdir(QUEUE_DIR, 0755) != 0) {
            perror("Failed to create queue directory");
            return -1;
        }
    }
    return 0;
}


int save_to_queue(const char *payload) {
    char filename[512];
    snprintf(filename, sizeof(filename), "%s/msg_%ld_%d.json", QUEUE_DIR, time(NULL), rand());
   
    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("Failed to open queue file");
        return -1;
    }
    fputs(payload, f);
    fclose(f);
    printf("Message saved to queue: %s\n", filename);
    return 0;
}


int replay_queue(MQTTClient client, MQTTClient_connectOptions *conn_opts, const char *topic, int qos, long timeout) {
    DIR *dir = opendir(QUEUE_DIR);
    if (!dir) return 0;


    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".json")) {
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s/%s", QUEUE_DIR, entry->d_name);
           
            FILE *f = fopen(filepath, "r");
            if (!f) continue;


            char buffer[1024];
            if (fgets(buffer, sizeof(buffer), f)) {
                // Validate before republishing
                if (validate_payload(buffer) == 0) {
                    MQTTClient_message pubmsg = MQTTClient_message_initializer;
                    pubmsg.payload = buffer;
                    pubmsg.payloadlen = (int)strlen(buffer);
                    pubmsg.qos = qos;
                    pubmsg.retained = 0;


                    MQTTClient_deliveryToken token;
                    int rc = MQTTClient_publishMessage(client, topic, &pubmsg, &token);
                   
                    if (rc == MQTTCLIENT_SUCCESS) {
                        MQTTClient_waitForCompletion(client, token, timeout);
                        remove(filepath); // Delete only on success
                        count++;
                        printf("Replayed queued message: %s\n", entry->d_name);
                    } else {
                        fprintf(stderr, "Failed to replay message %s (code: %d)\n", entry->d_name, rc);
                    }
                } else {
                    printf("Skipping invalid queued message: %s\n", entry->d_name);
                    remove(filepath); // Remove invalid data to prevent infinite loops
                }
            }
            fclose(f);
        }
    }
    closedir(dir);
    return count;
}


// --- Main Logic ---
int connect_with_retry(MQTTClient *client, MQTTClient_connectOptions *conn_opts, const char *address) {
    int rc, attempts = 0;
    while ((rc = MQTTClient_connect(*client, conn_opts)) != MQTTCLIENT_SUCCESS) {
        attempts++;
        if (attempts > 10) {
            printf("Connection failed after 10 attempts.\n");
            return -1;
        }
        printf("Connection failed (attempt %d), retrying in 3s...\n", attempts);
        sleep(3);
    }
    printf("Connected successfully.\n");
    return 0;
}


int main(int argc, char *argv[]) {
    Config config = parse_args(argc, argv);
    signal(SIGINT, handle_sigint);
    srand(time(NULL));


    // Initialize Queue
    if (init_queue() != 0) {
        fprintf(stderr, "Warning: Could not initialize message queue. Offline buffering disabled.\n");
    }


    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;


    if (MQTTClient_create(&client, config.address, config.clientid, MQTTCLIENT_PERSISTENCE_NONE, NULL) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to create client.\n");
        return 1;
    }


    // Configure Options
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    if (config.username) conn_opts.username = config.username;
    if (config.password) conn_opts.password = config.password;


    // SSL/TLS Setup
    ssl_opts.enableServerCertAuth = 1;
    if (config.ca_cert) ssl_opts.trustStore = config.ca_cert;
    if (config.client_cert) ssl_opts.keyStore = config.client_cert;
    if (config.client_key) ssl_opts.privateKey = config.client_key;
    if (config.key_password) ssl_opts.privateKeyPassword = config.key_password;
    conn_opts.ssl = &ssl_opts;


    // Connect
    if (connect_with_retry(&client, &conn_opts, config.address) != 0) {
        MQTTClient_destroy(&client);
        return 1;
    }


    // Replay Queue if connected
    int replayed = replay_queue(client, &conn_opts, config.topic, config.qos, config.timeout);
    if (replayed > 0) printf("Replayed %d messages from queue.\n", replayed);


    printf("Starting sensor loop on topic: %s\n", config.topic);


    while (running) {
        char payload[256];
        float temp = 20.0 + (rand() % 1000) / 100.0;
        float hum = 40.0 + (rand() % 1000) / 100.0;


        snprintf(payload, sizeof(payload),
                 "{\"temperature\": %.2f, \"humidity\": %.2f, \"timestamp\": %ld, \"count\": %d}",
                 temp, hum, time(NULL), ++message_count);


        // Validate locally before publishing
        if (validate_payload(payload) != 0) {
            fprintf(stderr, "Generated invalid payload, skipping.\n");
            sleep(config.interval);
            continue;
        }


        MQTTClient_message pubmsg = MQTTClient_message_initializer;
        pubmsg.payload = payload;
        pubmsg.payloadlen = (int)strlen(payload);
        pubmsg.qos = config.qos;
        pubmsg.retained = 0;


        MQTTClient_deliveryToken token;
        int rc = MQTTClient_publishMessage(client, config.topic, &pubmsg, &token);


        if (rc == MQTTCLIENT_SUCCESS) {
            MQTTClient_waitForCompletion(client, token, config.timeout);
            printf("[%d] Published: %s\n", message_count, payload);
        } else {
            fprintf(stderr, "Publish failed (code %d). Saving to queue...\n", rc);
            save_to_queue(payload);
        }


        sleep(config.interval);
    }


    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    printf("Shutdown complete. Total messages: %d\n", message_count);
    return 0;
}

