#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>

// A simple linked list node to store registered device ids.
typedef struct DeviceNode {
    char device_id[256];
    struct DeviceNode *next;
} DeviceNode;

DeviceNode *device_list = NULL;
pthread_mutex_t device_list_mutex = PTHREAD_MUTEX_INITIALIZER;

void add_device(const char *device_id) {
    pthread_mutex_lock(&device_list_mutex);
    DeviceNode *cur = device_list;
    while (cur) {
        if (strcmp(cur->device_id, device_id) == 0) {
            pthread_mutex_unlock(&device_list_mutex);
            return;
        }
        cur = cur->next;
    }
    DeviceNode *node = malloc(sizeof(DeviceNode));
    strncpy(node->device_id, device_id, sizeof(node->device_id)-1);
    node->device_id[sizeof(node->device_id)-1] = '\0';
    node->next = device_list;
    device_list = node;
    pthread_mutex_unlock(&device_list_mutex);
    printf("Broker: Device registered: %s\n", device_id);
}

void remove_device(const char *device_id) {
    pthread_mutex_lock(&device_list_mutex);
    DeviceNode **cur = &device_list;
    while (*cur) {
        if (strcmp((*cur)->device_id, device_id) == 0) {
            DeviceNode *to_free = *cur;
            *cur = (*cur)->next;
            free(to_free);
            pthread_mutex_unlock(&device_list_mutex);
            printf("Broker: Device removed: %s\n", device_id);
            return;
        }
        cur = &((*cur)->next);
    }
    pthread_mutex_unlock(&device_list_mutex);
}

// Structure to pass arguments to the monitor thread.
typedef struct {
    void *context;
    void *monitor_socket; // PAIR socket for reading monitor events.
    void *bs_socket;      // Base station socket (bs_router) for sending control notifications.
} monitor_args_t;

// The monitor function reads events from monitor_socket and sends control messages
// to the base station via bs_socket. For a bound ROUTER socket, connection events
// come as ZMQ_EVENT_ACCEPTED and disconnect as ZMQ_EVENT_DISCONNECTED.
static void monitor_socket(void *context, void *monitor_socket, void *bs_socket) {
    while (1) {
        zmq_pollitem_t items[] = {
            { monitor_socket, 0, ZMQ_POLLIN, 0 }
        };
        int rc = zmq_poll(items, 1, -1);
        if (rc == -1) {
            break;
        }
        if (items[0].revents & ZMQ_POLLIN) {
            // First frame: event (uint16_t)
            zmq_msg_t event_msg;
            zmq_msg_init(&event_msg);
            int size = zmq_msg_recv(&event_msg, monitor_socket, 0);
            if (size == -1) {
                zmq_msg_close(&event_msg);
                continue;
            }
            uint16_t event = *(uint16_t *)zmq_msg_data(&event_msg);
            zmq_msg_close(&event_msg);

            // Second frame: address (remote endpoint info)
            zmq_msg_init(&event_msg);
            size = zmq_msg_recv(&event_msg, monitor_socket, 0);
            if (size == -1) {
                zmq_msg_close(&event_msg);
                continue;
            }
            char *address = strndup((char *)zmq_msg_data(&event_msg), size);
            zmq_msg_close(&event_msg);

            // For a bound socket, a new connection comes as ZMQ_EVENT_ACCEPTED.
            if (event == ZMQ_EVENT_DISCONNECTED) {
                printf("BROKER MONITOR: Disconnected from %s\n", address);
                // Notify base station about disconnection.
                zmq_send(bs_socket, "control", 7, ZMQ_SNDMORE);
                char ctrl_msg[256];
                snprintf(ctrl_msg, sizeof(ctrl_msg), "DISCONNECTED %s", address);
                zmq_send(bs_socket, ctrl_msg, strlen(ctrl_msg), 0);
                // In a more advanced version, you could correlate the address with a device id.
            } else if (event == ZMQ_EVENT_ACCEPTED || event == ZMQ_EVENT_CONNECTED) {
                printf("BROKER MONITOR: Connection accepted from %s\n", address);
                // Optionally, you might notify the base station about connection events.
                // For now, we rely on the device registration message to know the device id.
            }
            free(address);
        }
    }
}

static void *monitor_socket_thread(void *arg) {
    monitor_args_t *args = (monitor_args_t *)arg;
    monitor_socket(args->context, args->monitor_socket, args->bs_socket);
    return NULL;
}

int main() {
    void *context = zmq_ctx_new();
    assert(context);

    // Create a ROUTER socket for devices (devices use DEALER with ZMQ_IDENTITY).
    void *device_router = zmq_socket(context, ZMQ_ROUTER);
    int rc = zmq_bind(device_router, "tcp://*:5555");
    assert(rc == 0);

    // Create a ROUTER socket for the base station.
    void *bs_router = zmq_socket(context, ZMQ_ROUTER);
    rc = zmq_bind(bs_router, "tcp://*:5556");
    assert(rc == 0);

    printf("Broker running using ROUTER/ROUTER pattern with monitoring...\n");

    // Set up monitor on the device_router socket.
    rc = zmq_socket_monitor(device_router, "inproc://device_monitor", ZMQ_EVENT_ALL);
    assert(rc == 0);
    void *monitor_socket = zmq_socket(context, ZMQ_PAIR);
    assert(monitor_socket);
    rc = zmq_connect(monitor_socket, "inproc://device_monitor");
    if (rc != 0) {
        fprintf(stderr, "Failed to connect monitor socket: %s\n", zmq_strerror(zmq_errno()));
        exit(1);
    }

    // Create a monitor thread for device_router events.
    pthread_t monitor_thread;
    monitor_args_t mon_args;
    mon_args.context = context;
    mon_args.monitor_socket = monitor_socket;
    mon_args.bs_socket = bs_router; // We'll use bs_router to send control messages to the base station.
    pthread_create(&monitor_thread, NULL, monitor_socket_thread, &mon_args);

    // Main proxy loop: forward messages between device_router and bs_router.
    // Also, process registration messages to learn device identities.
    zmq_pollitem_t items[2];
    items[0].socket = device_router;
    items[0].fd = 0;
    items[0].events = ZMQ_POLLIN;
    items[0].revents = 0;
    items[1].socket = bs_router;
    items[1].fd = 0;
    items[1].events = ZMQ_POLLIN;
    items[1].revents = 0;

    while (1) {
        rc = zmq_poll(items, 2, -1);
        if (rc == -1)
            break;

        // Message from device -> base station.
        if (items[0].revents & ZMQ_POLLIN) {
            // Expected format: [device_id][empty][message]
            zmq_msg_t identity;
            zmq_msg_init(&identity);
            zmq_msg_recv(&identity, device_router, 0);

            zmq_msg_t empty;
            zmq_msg_init(&empty);
            zmq_msg_recv(&empty, device_router, 0);
            zmq_msg_close(&empty);

            zmq_msg_t content;
            zmq_msg_init(&content);
            zmq_msg_recv(&content, device_router, 0);
            char *msg_data = strndup(zmq_msg_data(&content), zmq_msg_size(&content));
            zmq_msg_close(&content);

            // Check if the message is a registration message.
            if (strncmp(msg_data, "REGISTER:", 9) == 0) {
                char *dev_id = msg_data + 9;
                add_device(dev_id);
            }

            // Forward the message (including the identity) to the base station.
            zmq_msg_send(&identity, bs_router, ZMQ_SNDMORE);
            zmq_send(bs_router, "", 0, ZMQ_SNDMORE);
            zmq_send(bs_router, msg_data, strlen(msg_data), 0);

            free(msg_data);
            zmq_msg_close(&identity);
        }

        // Message from base station -> device.
        if (items[1].revents & ZMQ_POLLIN) {
            // Expected format: [bs_id][empty][destination_id][empty][message]
            zmq_msg_t bs_identity;
            zmq_msg_init(&bs_identity);
            zmq_msg_recv(&bs_identity, bs_router, 0);

            zmq_msg_t delim;
            zmq_msg_init(&delim);
            zmq_msg_recv(&delim, bs_router, 0);
            zmq_msg_close(&delim);

            zmq_msg_t dest;
            zmq_msg_init(&dest);
            zmq_msg_recv(&dest, bs_router, 0);
            char *dest_id = strndup(zmq_msg_data(&dest), zmq_msg_size(&dest));
            zmq_msg_close(&dest);

            zmq_msg_init(&delim);
            zmq_msg_recv(&delim, bs_router, 0);
            zmq_msg_close(&delim);

            zmq_msg_t bs_content;
            zmq_msg_init(&bs_content);
            zmq_msg_recv(&bs_content, bs_router, 0);
            char *bs_msg = strndup(zmq_msg_data(&bs_content), zmq_msg_size(&bs_content));
            zmq_msg_close(&bs_content);

            // Forward message to the destination device via device_router.
            zmq_send(device_router, dest_id, strlen(dest_id), ZMQ_SNDMORE);
            zmq_send(device_router, "", 0, ZMQ_SNDMORE);
            zmq_send(device_router, bs_msg, strlen(bs_msg), 0);

            free(dest_id);
            free(bs_msg);
            zmq_msg_close(&bs_identity);
        }
    }

    pthread_join(monitor_thread, NULL);
    zmq_close(device_router);
    zmq_close(bs_router);
    zmq_ctx_destroy(context);
    return 0;
}
