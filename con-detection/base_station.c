
#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdint.h>

#define HEARTBEAT_TIMEOUT 3000 

typedef struct {
    char device_id[256];
    int64_t last_heartbeat;
} DeviceInfo;

#define MAX_DEVICES 100


int64_t get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((int64_t)tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

int main() {
    void *context = zmq_ctx_new();

    
    void *publisher = zmq_socket(context, ZMQ_PUB);
    zmq_connect(publisher, "tcp://localhost:5555");

    
    void *subscriber = zmq_socket(context, ZMQ_SUB);
    zmq_connect(subscriber, "tcp://localhost:5556");
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "uplink", 6); 

    
    zmq_pollitem_t items[] = {
        {subscriber, 0, ZMQ_POLLIN, 0}
    };

    DeviceInfo devices[MAX_DEVICES];
    int device_count = 0;

    while (1) {
        int rc = zmq_poll(items, 1, 1000); 

        if (rc == -1) {
            break; 
        }

        if (items[0].revents & ZMQ_POLLIN) {
            
            char buffer[256];
            int size = zmq_recv(subscriber, buffer, sizeof(buffer) - 1, 0);
            if (size != -1) {
                buffer[size] = '\0';
                
                char topic[256];
                char command[256];
                char device_id[256];
                char rest[256];
                sscanf(buffer, "%s %s %s %[^\n]", topic, command, device_id, rest);
                int64_t now = get_current_time_ms();

                if (strcmp(command, "JOIN") == 0) {
                    
                    
                    int found = 0;
                    for (int i = 0; i < device_count; i++) {
                        if (strcmp(devices[i].device_id, device_id) == 0) {
                            found = 1;
                            devices[i].last_heartbeat = now;
                            break;
                        }
                    }
                    if (!found && device_count < MAX_DEVICES) {
                        strcpy(devices[device_count].device_id, device_id);
                        devices[device_count].last_heartbeat = now;
                        device_count++;
                        printf("gNB: Device %s joined. Total devices: %d\n", device_id, device_count);

                        
                        time_t current_time = time(NULL);
                        char time_msg[256];
                        snprintf(time_msg, sizeof(time_msg), "downlink %ld", current_time);
                        zmq_send(publisher, time_msg, strlen(time_msg), 0);
                    }
                } else if (strcmp(command, "HEARTBEAT") == 0) {
                    
                    for (int i = 0; i < device_count; i++) {
                        if (strcmp(devices[i].device_id, device_id) == 0) {
                            devices[i].last_heartbeat = now;
                            break;
                        }
                    }
                } else if (strcmp(command, "DATA") == 0) {
                    
                    printf("gNB received data from %s: %s\n", device_id, rest);
                    
                } else {
                    
                    printf("gNB received unknown command from %s: %s\n", device_id, command);
                }
            }
        }

        
        int64_t now = get_current_time_ms();
        for (int i = 0; i < device_count; ) {
            if (now - devices[i].last_heartbeat > HEARTBEAT_TIMEOUT) {
                
                printf("gNB: Device %s disconnected.\n", devices[i].device_id);
                
                for (int j = i; j < device_count - 1; j++) {
                    devices[j] = devices[j + 1];
                }
                device_count--;
            } else {
                i++;
            }
        }

        
        if (device_count == 0) {
            int random_data = rand() % 100;
            printf("gNB: No devices connected. Generated random data: %d\n", random_data);
            sleep(1);
        } else {
            
            
            char message[256];
            snprintf(message, sizeof(message), "downlink Message from gNB at %ld", now);
            zmq_send(publisher, message, strlen(message), 0);
        }
    }

    zmq_close(publisher);
    zmq_close(subscriber);
    zmq_ctx_destroy(context);
    return 0;
}
