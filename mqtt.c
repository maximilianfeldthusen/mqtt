
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include "MQTTClient.h"

#define ADDRESS     "ssl://your-mqtt-broker.com:8883"
#define CLIENTID    "AdvancedCClient"
#define TOPIC       "sensors/environment"
#define QOS         1
#define TIMEOUT     10000L

volatile int running = 1;

void handle_sigint(int sig) {
    running = 0;
}

float read_temp_sensor() {
    // Simulate temperature reading
    return 20.0 + (rand() % 1000) / 100.0;
}

float read_hum_sensor() {
    return 40.0 + (rand() % 1000) / 100.0;
}

int main() {
    signal(SIGINT, handle_sigint);
    srand(time(NULL));

    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;

    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);

    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.reliable = 0;

    ssl_opts.enableServerCertAuth = 1;
    ssl_opts.trustStore = "/path/to/ca.crt"; // Update with path to your CA cert
    conn_opts.ssl = &ssl_opts;

    int rc;
    while ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("MQTT connection failed with code %d, retrying...\n", rc);
        sleep(3);
    }

    while (running) {
        char payload[256];
        float temp = read_temp_sensor();
        float hum = read_hum_sensor();

        snprintf(payload, sizeof(payload),
                 "{\"temperature\": %.2f, \"humidity\": %.2f, \"timestamp\": %ld}",
                 temp, hum, time(NULL));

        MQTTClient_message pubmsg = MQTTClient_message_initializer;
        pubmsg.payload = payload;
        pubmsg.payloadlen = (int)strlen(payload);
        pubmsg.qos = QOS;
        pubmsg.retained = 0;

        MQTTClient_deliveryToken token;
        MQTTClient_publishMessage(client, TOPIC, &pubmsg, &token);
        printf("Published: %s\n", payload);
        MQTTClient_waitForCompletion(client, token, TIMEOUT);
        sleep(5);
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    printf("MQTT client shutdown.\n");

    return 0;
}
