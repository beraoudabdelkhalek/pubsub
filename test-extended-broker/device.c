#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <device_id>\n", argv[0]);
        return -1;
    }
    const char *device_id = argv[1];

    void *context = zmq_ctx_new();

    // Use a DEALER socket with identity set.
    void *socket = zmq_socket(context, ZMQ_DEALER);
    assert(socket);
    zmq_setsockopt(socket, ZMQ_IDENTITY, device_id, strlen(device_id));

    int rc = zmq_connect(socket, "tcp://127.0.0.1:5555");
    assert(rc == 0);
    printf("Device %s connected to broker.\n", device_id);

    // Send registration message: [empty delimiter][REGISTER:<device_id>]
    zmq_send(socket, "", 0, ZMQ_SNDMORE);
    char reg_msg[256];
    snprintf(reg_msg, sizeof(reg_msg), "REGISTER:%s", device_id);
    zmq_send(socket, reg_msg, strlen(reg_msg), 0);

    // Poll for incoming messages and send uplink messages.
    zmq_pollitem_t items[] = {
        { socket, 0, ZMQ_POLLIN, 0 }
    };

    int count = 0;
    while (1) {
        int poll_rc = zmq_poll(items, 1, 3);
        if (poll_rc > 0 && (items[0].revents & ZMQ_POLLIN)) {
            // For ROUTER/DEALER pattern, expect: [empty delimiter][message]
            zmq_msg_t empty;
            zmq_msg_init(&empty);
            zmq_msg_recv(&empty, socket, 0);
            zmq_msg_close(&empty);

            zmq_msg_t msg;
            zmq_msg_init(&msg);
            zmq_msg_recv(&msg, socket, 0);
            char *msg_data = strndup(zmq_msg_data(&msg), zmq_msg_size(&msg));
            zmq_msg_close(&msg);

            printf("Device %s received: %s\n", device_id, msg_data);
            free(msg_data);
        }

        // Send an uplink message: [empty delimiter][uplink message]
        zmq_send(socket, "", 0, ZMQ_SNDMORE);
        char uplink_msg[256];
        snprintf(uplink_msg, sizeof(uplink_msg), "Uplink message %d from %s", count++, device_id);
        zmq_send(socket, uplink_msg, strlen(uplink_msg), 0);
        printf("Device %s sent: %s\n", device_id, uplink_msg);

        sleep(1);
    }

    zmq_close(socket);
    zmq_ctx_destroy(context);
    return 0;
}
