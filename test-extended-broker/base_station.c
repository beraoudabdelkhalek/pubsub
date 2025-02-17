#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

int main() {
    // Create ZeroMQ context
    void *context = zmq_ctx_new();
    assert(context);

    // Create a DEALER socket for the base station
    void *socket = zmq_socket(context, ZMQ_DEALER);
    assert(socket);

    // Set an identity for the base station
    const char *bs_identity = "BaseStation";
    zmq_setsockopt(socket, ZMQ_IDENTITY, bs_identity, strlen(bs_identity));

    // Connect to the broker's base station endpoint (port 5556)
    int rc = zmq_connect(socket, "tcp://127.0.0.1:5556");
    assert(rc == 0);
    printf("Base Station connected to broker.\n");

    // Polling item for incoming messages on our DEALER socket
    zmq_pollitem_t items[] = {
        { socket, 0, ZMQ_POLLIN, 0 }
    };

    int count = 0;
    while (1) {
        // Poll for incoming messages (timeout = 3ms)
        int poll_rc = zmq_poll(items, 1, 3);
        if (poll_rc > 0 && (items[0].revents & ZMQ_POLLIN)) {
            // In a ROUTER/DEALER pattern, messages are multipart.
            // We expect messages coming from the broker in the following format:
            // [empty delimiter][message content]
            zmq_msg_t empty;
            zmq_msg_init(&empty);
            rc = zmq_msg_recv(&empty, socket, 0);
            zmq_msg_close(&empty);

            zmq_msg_t msg;
            zmq_msg_init(&msg);
            rc = zmq_msg_recv(&msg, socket, 0);
            char *msg_data = strndup(zmq_msg_data(&msg), zmq_msg_size(&msg));
            zmq_msg_close(&msg);

            // Check if it's a control message (broker notification)
            if (strncmp(msg_data, "CONNECTED", 9) == 0 ||
                strncmp(msg_data, "DISCONNECTED", 12) == 0) {
                printf("Base Station => BROKER EVENT: %s\n", msg_data);
            } else {
                // Otherwise, it's a normal uplink message from a device.
                printf("Base Station received: %s\n", msg_data);
            }
            free(msg_data);
        }

        // Send a downlink message.
        // For demonstration, we send a message to "Device1".
        // Message format: [destination device id][empty delimiter][message content]
        char dest_id[] = "Device1";
        zmq_send(socket, dest_id, strlen(dest_id), ZMQ_SNDMORE); // Destination frame
        zmq_send(socket, "", 0, ZMQ_SNDMORE);                    // Empty delimiter
        char downlink_msg[256];
        snprintf(downlink_msg, sizeof(downlink_msg),
                 "Downlink message %d from Base Station", count++);
        zmq_send(socket, downlink_msg, strlen(downlink_msg), 0);

        sleep(1);
    }

    // Clean up
    zmq_close(socket);
    zmq_ctx_destroy(context);

    return 0;
}
