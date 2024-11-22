#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

typedef struct {
    void *context;      
    void *publisher;    
    void *subscriber;   
    int sub_fd;         
} Device;

int main() {
    
    Device *device = (Device *)malloc(sizeof(Device));
    if (!device) {
        perror("Failed to allocate memory for device");
        return 1;
    }

    
    device->context = zmq_ctx_new();

    
    device->publisher = zmq_socket(device->context, ZMQ_PUB);
    zmq_connect(device->publisher, "tcp://localhost:5555");

    
    device->subscriber = zmq_socket(device->context, ZMQ_SUB);
    zmq_connect(device->subscriber, "tcp://localhost:5556");
    zmq_setsockopt(device->subscriber, ZMQ_SUBSCRIBE, "downlink", strlen("downlink"));

    
    size_t fd_size = sizeof(device->sub_fd);
    if (zmq_getsockopt(device->subscriber, ZMQ_FD, &device->sub_fd, &fd_size) == -1) {
        perror("zmq_getsockopt failed");
        return 1;
    }

    
    zmq_pollitem_t items[] = {
        { NULL, device->sub_fd, ZMQ_POLLIN, 0 }
    };

    int count = 0;
    while (1) {
        
        int rc = zmq_poll(items, 1, 1000);  

        if (rc == -1) {
            perror("zmq_poll failed");
            break;
        }

        if (rc > 0 && items[0].revents & ZMQ_POLLIN) {
            
            while (1) {
                int events = 0;
                size_t events_size = sizeof(events);
                if (zmq_getsockopt(device->subscriber, ZMQ_EVENTS, &events, &events_size) == -1) {
                    perror("zmq_getsockopt ZMQ_EVENTS failed");
                    break;
                }

                if (events & ZMQ_POLLIN) {
                    
                    char topic[256] = {0};
                    char message[256] = {0};
                    zmq_recv(device->subscriber, topic, sizeof(topic) - 1, 0);
                    zmq_recv(device->subscriber, message, sizeof(message) - 1, 0);
                    printf("5G Device received on %s: %s\n", topic, message);
                } else {
                    
                    break;
                }
            }
        }

        
        char topic[] = "uplink";
        char message[256];
        snprintf(message, sizeof(message), "Uplink message %d from 5G Device 1\n", count++);
        zmq_send(device->publisher, topic, strlen(topic), ZMQ_SNDMORE);
        zmq_send(device->publisher, message, strlen(message), 0);

        sleep(3); 
    }

    
    zmq_close(device->publisher);
    zmq_close(device->subscriber);
    zmq_ctx_destroy(device->context);
    free(device);

    return 0;
}
