#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

typedef struct {
    void *context;      // ZeroMQ context
    void *publisher;    // Publisher socket
    void *subscriber;   // Subscriber socket
} Device;

int main() {
    // Dynamically allocate memory for the device structure
    Device *device = (Device *)malloc(sizeof(Device));
    if (!device) {
        perror("Failed to allocate memory for device");
        return 1;
    }

    // Create ZeroMQ context
    device->context = zmq_ctx_new();

    // Publisher socket for uplink messages
    device->publisher = zmq_socket(device->context, ZMQ_PUB);
    zmq_connect(device->publisher, "tcp://localhost:5555");

    // Subscriber socket for downlink messages
    device->subscriber = zmq_socket(device->context, ZMQ_SUB);
    zmq_connect(device->subscriber, "tcp://localhost:5556");
    zmq_setsockopt(device->subscriber, ZMQ_SUBSCRIBE, "downlink", strlen("downlink"));

    zmq_pollitem_t items[] = {
        { device->subscriber, 0, ZMQ_POLLIN, 0 }
    };

    int count = 0;
    while (1) {
        // Check for incoming downlink messages
        int rc = zmq_poll(items, 1, 1000);

        if (rc > 0 && items[0].revents & ZMQ_POLLIN) {
            char topic[256] = {0};
            char message[256] = {0};
            zmq_recv(device->subscriber, topic, sizeof(topic) - 1, 0);
            zmq_recv(device->subscriber, message, sizeof(message) - 1, 0);
            printf("5G Device received on %s: %s\n", topic, message);
        }

        // Send uplink message
        char topic[] = "uplink";
        char message[256];
        snprintf(message, sizeof(message), "Uplink message %d from 5G Device 1\n", count++);
        zmq_send(device->publisher, topic, strlen(topic), ZMQ_SNDMORE);
        zmq_send(device->publisher, message, strlen(message), 0);

        sleep(3); // Send every 3 seconds
    }

    // Clean up
    zmq_close(device->publisher);
    zmq_close(device->subscriber);
    zmq_ctx_destroy(device->context);
    free(device);

    return 0;
}
