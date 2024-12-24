
#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdint.h>

#define HEARTBEAT_INTERVAL 1000 


int64_t get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((int64_t)tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: device <device_id>\n");
        return -1;
    }

    char *device_id = argv[1];

    void *context = zmq_ctx_new();

    
    void *publisher = zmq_socket(context, ZMQ_PUB);
    zmq_connect(publisher, "tcp://localhost:5555");

    
    void *subscriber = zmq_socket(context, ZMQ_SUB);
    zmq_connect(subscriber, "tcp://localhost:5556");
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "downlink", 8); 

    
    char join_msg[256];
    snprintf(join_msg, sizeof(join_msg), "uplink JOIN %s", device_id);
    zmq_send(publisher, join_msg, strlen(join_msg), 0);

    
    zmq_pollitem_t items[] = {
        {subscriber, 0, ZMQ_POLLIN, 0}
    };

    int64_t last_heartbeat = get_current_time_ms();
    int received_initial_time = 0;

    while (1) {
        int rc = zmq_poll(items, 1, HEARTBEAT_INTERVAL);

        if (rc == -1) {
            break; 
        }

        if (items[0].revents & ZMQ_POLLIN) {
            
            char buffer[256];
            int size = zmq_recv(subscriber, buffer, sizeof(buffer) - 1, 0);
            if (size != -1) {
                buffer[size] = '\0';
                
                char topic[256];
                char payload[256];
                sscanf(buffer, "%s %[^\n]", topic, payload);
                printf("Device %s received message: %s\n", device_id, payload);

                if (!received_initial_time) {
                    
                    printf("Device %s received initial time: %s\n", device_id, payload);
                    received_initial_time = 1;
                } else {
                    
                    
                }
            }
        }

        int64_t now = get_current_time_ms();
        if (now - last_heartbeat >= HEARTBEAT_INTERVAL) {
            
            char heartbeat_msg[256];
            snprintf(heartbeat_msg, sizeof(heartbeat_msg), "uplink HEARTBEAT %s", device_id);
            zmq_send(publisher, heartbeat_msg, strlen(heartbeat_msg), 0);

            
            if (received_initial_time) {
                char data_msg[256];
                snprintf(data_msg, sizeof(data_msg), "uplink DATA %s %ld", device_id, now);
                zmq_send(publisher, data_msg, strlen(data_msg), 0);
            }

            last_heartbeat = now;
        }
    }

    zmq_close(publisher);
    zmq_close(subscriber);
    zmq_ctx_destroy(context);
    return 0;
}
