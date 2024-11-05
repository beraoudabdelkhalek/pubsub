#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main() {
    void *context = zmq_ctx_new();

    // socket for uplink messages
    void *publisher = zmq_socket(context, ZMQ_PUB);
    zmq_connect(publisher, "tcp://localhost:5555");

    // socket for downlink messages
    void *subscriber = zmq_socket(context, ZMQ_SUB);
    zmq_connect(subscriber, "tcp://localhost:5556");
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "downlink", strlen("downlink"));

    zmq_pollitem_t items[] = {
        { subscriber, 0, ZMQ_POLLIN, 0 }
    };

    int count = 0;
    while (1) {
        // Check for incoming downlink messages
        int rc = zmq_poll(items, 1, 1000); // Poll every second

        if (rc > 0 && items[0].revents & ZMQ_POLLIN) {
            char topic[256];
            char message[256];
            zmq_recv(subscriber, topic, sizeof(topic), 0);
            zmq_recv(subscriber, message, sizeof(message), 0);
            printf("5G Device received on %s: %s\n", topic, message);
        }

        // Send uplink message
        char topic[] = "uplink";
        char message[256];
        snprintf(message, sizeof(message), "Uplink message %d from 5G Device 2\n", count++);
        zmq_send(publisher, topic, strlen(topic), ZMQ_SNDMORE);
        zmq_send(publisher, message, strlen(message), 0);
        // printf("5G Device sent on %s: %s\n", topic, message);

        sleep(3); // Send every 3 seconds
    }

    zmq_close(publisher);
    zmq_close(subscriber);
    zmq_ctx_destroy(context);

    return 0;
}
