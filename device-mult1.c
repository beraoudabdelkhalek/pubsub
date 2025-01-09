#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main() {
    void *context = zmq_ctx_new();
    int check = 0;
    
    void *publisher = zmq_socket(context, ZMQ_PUB);
    check = zmq_connect(publisher, "tcp://127.0.0.1:5555");
    printf("pub socket created, %d\n", check);
    
    void *subscriber = zmq_socket(context, ZMQ_SUB);
    check = zmq_connect(subscriber, "tcp://127.0.0.1:5556");
    printf("2 %d\n", check);
    printf("sub socket created\n");
    check = zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "downlink", strlen("downlink"));
    printf("3 %d\n", check);
    
    zmq_pollitem_t items[] = {
        { subscriber, 0, ZMQ_POLLIN, 0 }
    };
    int count = 0;
    while (1) {
        
        int rc = zmq_poll(items, 1, 3);
        printf("rc: %d\n", rc);
        if (rc > 0 && (items[0].revents & ZMQ_POLLIN)) {
            
            char topic[256]   = {0};
            char message[256] = {0};
            int frame_index = 0;
            
            while (1) {
                zmq_msg_t frame;
                zmq_msg_init(&frame);
                int size = zmq_msg_recv(&frame, subscriber, 0);
                if (size == -1) {
                    
                    zmq_msg_close(&frame);
                    break;
                }
                
                char buffer[256] = {0};
                int copy_size = (size < (int)sizeof(buffer)) ? size : (int)sizeof(buffer) - 1;
                memcpy(buffer, zmq_msg_data(&frame), copy_size);
                buffer[copy_size] = '\0';
                
                if (frame_index == 0) {
                    strncpy(topic, buffer, sizeof(topic) - 1);
                } else if (frame_index == 1) {
                    strncpy(message, buffer, sizeof(message) - 1);
                }
                frame_index++;
                
                int more = zmq_msg_more(&frame);
                zmq_msg_close(&frame);
                if (!more) {
                    break; 
                }
            }
            
            printf("5G Device received on %s: %s\n", topic, message);
        }
        
        {
            char topic[] = "uplink";
            char message[256];
            snprintf(message, sizeof(message), "Uplink message %d from 5G Device 1", count++);
            
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
