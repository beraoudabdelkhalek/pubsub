// device with zmq polling management and best practices
#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define TOPIC_MAX_LEN 256
#define MESSAGE_MAX_LEN 256

int main() {
    void *context = zmq_ctx_new();
    if (context == NULL) {
        perror("Failed to create ZeroMQ context");
        return -1;
    }

    
    void *publisher = zmq_socket(context, ZMQ_PUB);
    if (publisher == NULL) {
        perror("Failed to create publisher socket");
        zmq_ctx_destroy(context);
        return -1;
    }
    if (zmq_connect(publisher, "tcp://localhost:5555") != 0) {
        perror("Failed to connect publisher socket");
        zmq_close(publisher);
        zmq_ctx_destroy(context);
        return -1;
    }

    
    void *subscriber = zmq_socket(context, ZMQ_SUB);
    if (subscriber == NULL) {
        perror("Failed to create subscriber socket");
        zmq_close(publisher);
        zmq_ctx_destroy(context);
        return -1;
    }
    if (zmq_connect(subscriber, "tcp://localhost:5556") != 0) {
        perror("Failed to connect subscriber socket");
        zmq_close(subscriber);
        zmq_close(publisher);
        zmq_ctx_destroy(context);
        return -1;
    }
    const char *sub_topic = "downlink";
    if (zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, sub_topic, strlen(sub_topic)) != 0) {
        perror("Failed to set subscriber socket options");
        zmq_close(subscriber);
        zmq_close(publisher);
        zmq_ctx_destroy(context);
        return -1;
    }

    zmq_pollitem_t items[] = {
        { subscriber, 0, ZMQ_POLLIN, 0 }
    };

    int count = 0;
    while (1) {
        
        int rc = zmq_poll(items, 1, 1000);  
        if (rc == -1) {
            perror("zmq_poll failed");
            break;
        }
        printf("zmq_poll returned: %d\n", rc);

        if (rc > 0 && (items[0].revents & ZMQ_POLLIN)) {
            char topic[TOPIC_MAX_LEN + 1] = {0};    
            char message[MESSAGE_MAX_LEN + 1] = {0};

            int topic_size = zmq_recv(subscriber, topic, TOPIC_MAX_LEN, 0);
            if (topic_size == -1) {
                perror("Failed to receive topic");
                break;
            }
            if (topic_size < TOPIC_MAX_LEN) {
                topic[topic_size] = '\0';  
            } else {
                topic[TOPIC_MAX_LEN] = '\0';
            }

            int message_size = zmq_recv(subscriber, message, MESSAGE_MAX_LEN, 0);
            if (message_size == -1) {
                perror("Failed to receive message");
                break;
            }
            if (message_size < MESSAGE_MAX_LEN) {
                message[message_size] = '\0';  
            } else {
                message[MESSAGE_MAX_LEN] = '\0';
            }

            printf("5G Device received on '%s': %s\n", topic, message);
        }

        
        const char *pub_topic = "uplink";
        char message[MESSAGE_MAX_LEN];
        int message_size = snprintf(message, sizeof(message), "Uplink message %d from 5G Device 1", count++);
        if (message_size < 0 || message_size >= (int)sizeof(message)) {
            fprintf(stderr, "Message formatting error or message too long\n");
            break;
        }

        
        int bytes_sent = zmq_send(publisher, pub_topic, strlen(pub_topic), ZMQ_SNDMORE);
        if (bytes_sent == -1) {
            perror("Failed to send topic");
            break;
        }

        
        bytes_sent = zmq_send(publisher, message, message_size, 0);
        if (bytes_sent == -1) {
            perror("Failed to send message");
            break;
        }

        
        

        sleep(3); 
    }

    
    zmq_close(subscriber);
    zmq_close(publisher);
    zmq_ctx_destroy(context);

    return 0;
}
