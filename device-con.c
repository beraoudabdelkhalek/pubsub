
// Device with zmq polling management and connection detection
#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

int main() {
    void *context = zmq_ctx_new();
    int check = 0;

    
    void *publisher = zmq_socket(context, ZMQ_PUB);
    check = zmq_connect(publisher, "tcp://127.0.0.1:5555");
    printf("Publisher socket created, zmq_connect returned: %d\n", check);

    
    int rc = zmq_socket_monitor(publisher, "inproc://pub_monitor", ZMQ_EVENT_CONNECTED);
    assert(rc == 0);
    void *pub_mon = zmq_socket(context, ZMQ_PAIR);
    rc = zmq_connect(pub_mon, "inproc://pub_monitor");
    assert(rc == 0);

    
    void *subscriber = zmq_socket(context, ZMQ_SUB);
    check = zmq_connect(subscriber, "tcp://127.0.0.1:5556");
    printf("Subscriber socket created, zmq_connect returned: %d\n", check);
    check = zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "downlink", strlen("downlink"));
    printf("zmq_setsockopt returned: %d\n", check);

    
    rc = zmq_socket_monitor(subscriber, "inproc://sub_monitor", ZMQ_EVENT_CONNECTED);
    assert(rc == 0);
    void *sub_mon = zmq_socket(context, ZMQ_PAIR);
    rc = zmq_connect(sub_mon, "inproc://sub_monitor");
    assert(rc == 0);

    
    int pub_connected = 0;
    int sub_connected = 0;

    
    while (!pub_connected || !sub_connected) {
        
        zmq_msg_t event_msg;
        zmq_msg_init(&event_msg);
        rc = zmq_msg_recv(&event_msg, pub_mon, ZMQ_DONTWAIT);
        if (rc != -1) {
            uint16_t event = *(uint16_t *)zmq_msg_data(&event_msg);
            zmq_msg_close(&event_msg);

            if (event == ZMQ_EVENT_CONNECTED) {
                printf("Publisher socket connected\n");
                pub_connected = 1;
            }
        } else {
            zmq_msg_close(&event_msg);
        }

        
        zmq_msg_init(&event_msg);
        rc = zmq_msg_recv(&event_msg, sub_mon, ZMQ_DONTWAIT);
        if (rc != -1) {
            uint16_t event = *(uint16_t *)zmq_msg_data(&event_msg);
            zmq_msg_close(&event_msg);

            if (event == ZMQ_EVENT_CONNECTED) {
                printf("Subscriber socket connected\n");
                sub_connected = 1;
            }
        } else {
            zmq_msg_close(&event_msg);
        }

        
        usleep(10000); 
    }

    
    zmq_close(pub_mon);
    zmq_close(sub_mon);

    
    zmq_pollitem_t items[] = {
        { subscriber, 0, ZMQ_POLLIN, 0 }
    };

    int count = 0;
    while (1) {
        
        int rc = zmq_poll(items, 1, 1000);

        if (rc > 0 && items[0].revents & ZMQ_POLLIN) {
            char topic[256] = {0};
            char message[256] = {0};
            zmq_recv(subscriber, topic, sizeof(topic) - 1, 0);
            zmq_recv(subscriber, message, sizeof(message) - 1, 0);
            printf("5G Device received on %s: %s\n", topic, message);
        }

        
        char topic[] = "uplink";
        char message[256];
        snprintf(message, sizeof(message), "Uplink message %d from 5G Device 1\n", count++);
        zmq_send(publisher, topic, strlen(topic), ZMQ_SNDMORE);
        zmq_send(publisher, message, strlen(message), 0);

        sleep(3); 
    }

    
    zmq_close(publisher);
    zmq_close(subscriber);
    zmq_ctx_destroy(context);

    return 0;
}

