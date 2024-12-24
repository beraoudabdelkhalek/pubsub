// device with socket management with fd and epoll
#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <stdlib.h>

typedef struct {
    void *context;        
    void *publisher;      
    void *subscriber;     
    int pub_fd;           
    int sub_fd;           
    char device_id[50];   
} Device;


Device* init_device(const char *device_id, const char *pub_topic, const char *sub_topic, const char *pub_addr, const char *sub_addr) {
    Device *device = (Device *)malloc(sizeof(Device));
    if (!device) {
        perror("Failed to allocate memory");
        exit(1);
    }

    device->context = zmq_ctx_new();

    device->publisher = zmq_socket(device->context, ZMQ_PUB);
    zmq_connect(device->publisher, pub_addr);

    device->subscriber = zmq_socket(device->context, ZMQ_SUB);
    zmq_connect(device->subscriber, sub_addr);
    zmq_setsockopt(device->subscriber, ZMQ_SUBSCRIBE, sub_topic, strlen(sub_topic));

    
    size_t fd_size = sizeof(device->pub_fd); 
    if (zmq_getsockopt(device->publisher, ZMQ_FD, &device->pub_fd, &fd_size) != 0) {
        perror("Failed to get publisher socket file descriptor");
        exit(1);
    }

    fd_size = sizeof(device->sub_fd); 
    if (zmq_getsockopt(device->subscriber, ZMQ_FD, &device->sub_fd, &fd_size) != 0) {
        perror("Failed to get subscriber socket file descriptor");
        exit(1);
    }

    strncpy(device->device_id, device_id, sizeof(device->device_id) - 1);
    device->device_id[sizeof(device->device_id) - 1] = '\0';

    printf("[%s] Initialized. Publishing on %s, Subscribing to %s\n", device_id, pub_topic, sub_topic);

    return device;
}

void publish_message(Device *device, const char *topic, const char *message) {
    zmq_send(device->publisher, topic, strlen(topic), ZMQ_SNDMORE);
    zmq_send(device->publisher, message, strlen(message), 0);
    // printf("[%s] Published: %s %s\n", device->device_id, topic, message);
}

void poll_sockets(Device *device) {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1 failed");
        exit(1);
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = device->sub_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, device->sub_fd, &ev) == -1) {
        perror("epoll_ctl failed");
        exit(1);
    }

    struct epoll_event events[1];
    while (1) {
        int nfds = epoll_wait(epoll_fd, events, 1, 1000); 
        printf("nfds: %d\n", nfds);
        if (nfds == -1) {
            perror("epoll_wait failed");
            break;
        }

        if (nfds > 0 && (events[0].events & EPOLLIN)) {
            int zmq_events = 0;
            size_t zmq_events_size = sizeof(zmq_events);

            // loop to read all available messages
            while (1) {
                zmq_getsockopt(device->subscriber, ZMQ_EVENTS, &zmq_events, &zmq_events_size);
                if (zmq_events & ZMQ_POLLIN) {
                    char topic[256];
                    char message[256];
                    printf("solve issue\n");
                    int rc = zmq_recv(device->subscriber, topic, sizeof(topic), 0);
                    if (rc == -1) {
                        perror("Failed to receive topic");
                        break;
                    }
                    topic[rc] = '\0';

                
                    rc = zmq_recv(device->subscriber, message, sizeof(message), 0);
                    if (rc == -1) {
                        perror("Failed to receive message");
                        break;
                    }
                    message[rc] = '\0';

                    printf("[%s] Received on %s: %s\n", device->device_id, topic, message);
                } else {
                    // No more messages to read
                    break;
                }
            }
        }
        static int count = 0;
        count++;
        char message[256];
        snprintf(message, sizeof(message), "Uplink message %d from %s", count, device->device_id);
        publish_message(device, "uplink", message);
        
    }

    close(epoll_fd);
}

void cleanup_device(Device *device) {
    zmq_close(device->publisher);
    zmq_close(device->subscriber);
    zmq_ctx_destroy(device->context);
    printf("closing");
    free(device);
}

int main() {

    Device *device = init_device("5GDevice1", "uplink", "downlink", "tcp://localhost:5555", "tcp://localhost:5556");

    
    poll_sockets(device);

    
    cleanup_device(device);

    return 0;
}
