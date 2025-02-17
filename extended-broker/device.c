#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main() {
    void *context = zmq_ctx_new();

    // Publisher socket for uplink
    void *publisher = zmq_socket(context, ZMQ_PUB);
    zmq_connect(publisher, "tcp://127.0.0.1:5555");

    // Subscriber socket for downlink
    void *subscriber = zmq_socket(context, ZMQ_SUB);
    zmq_connect(subscriber, "tcp://127.0.0.1:5556");
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "downlink", strlen("downlink"));

    zmq_pollitem_t items[] = {
        { subscriber, 0, ZMQ_POLLIN, 0 }
    };

    int count = 0;
    while (1) {
        // Check for incoming downlink
        int rc = zmq_poll(items, 1, 3);
        if (rc > 0 && (items[0].revents & ZMQ_POLLIN)) {
            char topic[256];
            char msg[256];
            int tsize = zmq_recv(subscriber, topic, sizeof(topic), ZMQ_DONTWAIT);
            topic[tsize] = '\0';
            int msize = zmq_recv(subscriber, msg, sizeof(msg), ZMQ_DONTWAIT);
            msg[msize] = '\0';
            printf("Device received on %s: %s\n", topic, msg);
        }

        // Send uplink
        {
            char topic[] = "uplink";
            char message[256];
            snprintf(message, sizeof(message),
                     "Uplink message %d from Device", count++);
            zmq_send(publisher, topic, strlen(topic), ZMQ_SNDMORE);
            zmq_send(publisher, message, strlen(message), 0);
        }
        sleep(1);
    }

    zmq_close(publisher);
    zmq_close(subscriber);
    zmq_ctx_destroy(context);
    return 0;
}
