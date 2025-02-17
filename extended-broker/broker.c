#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>

typedef struct {
    void *context;        // Not strictly needed here, but provided for completeness
    void *monitor_socket; // PAIR socket for reading monitor events
    void *xpub_socket;    // XPUB socket for sending control notifications
} monitor_args_t;

// The function that reads monitor events from the "monitor_socket" and sends
// control messages to xpub_socket with topic="control" about connect/disconnect.
static void monitor_socket(void *context, void *monitor_socket, void *xpub_socket) {
    while (1) {
        zmq_pollitem_t items[] = {
            { monitor_socket, 0, ZMQ_POLLIN, 0 }
        };
        int rc = zmq_poll(items, 1, -1);
        if (rc == -1) {
            // Error or interruption
            break;
        }

        if (items[0].revents & ZMQ_POLLIN) {
            // First frame: event + value
            zmq_msg_t event_msg;
            zmq_msg_init(&event_msg);
            int size = zmq_msg_recv(&event_msg, monitor_socket, 0);
            if (size == -1) {
                zmq_msg_close(&event_msg);
                continue;
            }
            uint16_t event = *(uint16_t *)zmq_msg_data(&event_msg);
            zmq_msg_close(&event_msg);

            // Second frame: address or network info
            zmq_msg_init(&event_msg);
            size = zmq_msg_recv(&event_msg, monitor_socket, 0);
            if (size == -1) {
                zmq_msg_close(&event_msg);
                continue;
            }
            char *address = strndup((char *)zmq_msg_data(&event_msg), size);
            zmq_msg_close(&event_msg);

            // Distinguish between ZMQ_EVENT_ACCEPTED vs. ZMQ_EVENT_CONNECTED
            // * ZMQ_EVENT_ACCEPTED is raised on a bound socket when a new connection is accepted.
            // * ZMQ_EVENT_CONNECTED is raised on a connecting socket when it connects successfully.
            if (event == ZMQ_EVENT_ACCEPTED || event == ZMQ_EVENT_CONNECTED) {
                // "Connection" event
                printf("BROKER: A socket connected => %s\n", address);
                // Notify base station on xpub: "control" + "CONNECTED <address>"
                zmq_send(xpub_socket, "control", 7, ZMQ_SNDMORE);
                char message[256];
                snprintf(message, sizeof(message), "CONNECTED %s", address);
                zmq_send(xpub_socket, message, strlen(message), 0);

            } else if (event == ZMQ_EVENT_DISCONNECTED) {
                printf("BROKER: A socket disconnected => %s\n", address);
                // Notify base station: "control" + "DISCONNECTED <address>"
                zmq_send(xpub_socket, "control", 7, ZMQ_SNDMORE);
                char message[256];
                snprintf(message, sizeof(message), "DISCONNECTED %s", address);
                zmq_send(xpub_socket, message, strlen(message), 0);
            }

            free(address);
        }
    }
}

// Thread wrapper that unpacks monitor_args_t and calls monitor_socket()
static void *monitor_socket_thread(void *arg) {
    monitor_args_t *args = (monitor_args_t *)arg;
    monitor_socket(args->context, args->monitor_socket, args->xpub_socket);
    return NULL;
}

int main() {
    void *context = zmq_ctx_new();
    assert(context);

    // Create XSUB socket for publishers
    void *xsub_socket = zmq_socket(context, ZMQ_XSUB);
    assert(xsub_socket);
    int rc = zmq_bind(xsub_socket, "tcp://*:5555");
    assert(rc == 0);

    // Create XPUB socket for subscribers
    void *xpub_socket = zmq_socket(context, ZMQ_XPUB);
    assert(xpub_socket);
    rc = zmq_bind(xpub_socket, "tcp://*:5556");
    assert(rc == 0);

    printf("Broker is running using zmq_proxy, plus monitoring...\n");

    // ------------------------------------------------------------------
    // Setup monitor for XSUB socket
    // ------------------------------------------------------------------
    rc = zmq_socket_monitor(xsub_socket, "inproc://xsub_monitor", ZMQ_EVENT_ALL);
    assert(rc == 0);
    void *xsub_mon = zmq_socket(context, ZMQ_PAIR);
    assert(xsub_mon);
    rc = zmq_connect(xsub_mon, "inproc://xsub_monitor");
    assert(rc == 0);

    // ------------------------------------------------------------------
    // Setup monitor for XPUB socket
    // ------------------------------------------------------------------
    rc = zmq_socket_monitor(xpub_socket, "inproc://xpub_monitor", ZMQ_EVENT_ALL);
    assert(rc == 0);
    void *xpub_mon = zmq_socket(context, ZMQ_PAIR);
    assert(xpub_mon);
    rc = zmq_connect(xpub_mon, "inproc://xpub_monitor");
    assert(rc == 0);

    // We'll create two threads to monitor XSUB and XPUB
    pthread_t xsub_thread;
    pthread_t xpub_thread;

    // Prepare argument structs for each thread
    monitor_args_t xsub_args = { context, xsub_mon, xpub_socket };
    monitor_args_t xpub_args = { context, xpub_mon, xpub_socket };

    pthread_create(&xsub_thread, NULL, monitor_socket_thread, &xsub_args);
    pthread_create(&xpub_thread, NULL, monitor_socket_thread, &xpub_args);

    // Now run the proxy in the main thread
    // This will forward messages between XSUB and XPUB
    zmq_proxy(xsub_socket, xpub_socket, NULL);

    // Clean up if proxy ever exits
    zmq_close(xsub_socket);
    zmq_close(xpub_socket);

    zmq_close(xsub_mon);
    zmq_close(xpub_mon);

    zmq_ctx_destroy(context);
    return 0;
}
