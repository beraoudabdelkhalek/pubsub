#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main() {
    void *context = zmq_ctx_new();

    // socket for downlink messages (PUB)
    void *publisher = zmq_socket(context, ZMQ_PUB);
    zmq_connect(publisher, "tcp://localhost:5555");

    // socket for uplink messages (SUB)
    void *subscriber = zmq_socket(context, ZMQ_SUB);
    zmq_connect(subscriber, "tcp://localhost:5556");

    // Subscribe to "uplink"
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "uplink", strlen("uplink"));
    // Also subscribe to "control" to see broker notifications
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "control", strlen("control"));

    zmq_pollitem_t items[] = {
        { subscriber, 0, ZMQ_POLLIN, 0 }
    };

    int count = 0;
    while (1) {
        int rc = zmq_poll(items, 1, 3); // Poll every 3ms
        printf("rc: %d\n", rc);

        if (rc > 0 && (items[0].revents & ZMQ_POLLIN)) {
            // We'll receive two frames: topic + content
            char topic[256];
            char msg[256];
            int tsize = zmq_recv(subscriber, topic, sizeof(topic), ZMQ_DONTWAIT);
            if (tsize > 0) topic[tsize] = '\0';

            int msize = zmq_recv(subscriber, msg, sizeof(msg), ZMQ_DONTWAIT);
            if (msize > 0) msg[msize] = '\0';

            // Check topic
            if (strcmp(topic, "control") == 0) {
                // Broker event
                printf("Base Station => BROKER EVENT: %s\n", msg);
            } else if (strcmp(topic, "uplink") == 0) {
                // Normal device message
                printf("Base Station received on %s: %s\n", topic, msg);
            }
        }

        // Send downlink message
        {
            char downlink_topic[] = "downlink";
            char downlink_msg[256];
            snprintf(downlink_msg, sizeof(downlink_msg),
                     "Downlink message %d from Base Station", count++);
            zmq_send(publisher, downlink_topic, strlen(downlink_topic), ZMQ_SNDMORE);
            zmq_send(publisher, downlink_msg, strlen(downlink_msg), 0);
        }

        sleep(1);
    }

    zmq_close(publisher);
    zmq_close(subscriber);
    zmq_ctx_destroy(context);

    return 0;
}
