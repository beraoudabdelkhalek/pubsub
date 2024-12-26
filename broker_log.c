#include <zmq.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void* capture_thread(void* arg) {
    void* context = arg;
    
    
    void* capture = zmq_socket(context, ZMQ_PAIR);
    zmq_connect(capture, "inproc://capture");
    
    printf("Capture thread started, waiting for messages...\n");
    
    while (1) {
        
        
        
        zmq_msg_t frame;
        zmq_msg_init(&frame);
        
        int rc = zmq_msg_recv(&frame, capture, 0);
        if (rc == -1) {
            zmq_msg_close(&frame);
            break;  
        }

        
        printf("[CAPTURE] Received frame of size %d: %.*s\n",
               rc, rc, (char*)zmq_msg_data(&frame));
        
        
        int more = 0;
        size_t more_size = sizeof(more);
        zmq_getsockopt(capture, ZMQ_RCVMORE, &more, &more_size);
        
        zmq_msg_close(&frame);
        
        
        if (!more) {
            printf("[CAPTURE] End of multipart message.\n");
        }
    }
    
    zmq_close(capture);
    return NULL;
}

int main() {
    void* context = zmq_ctx_new();

    
    void* xsub_socket = zmq_socket(context, ZMQ_XSUB);
    zmq_bind(xsub_socket, "tcp://*:5555");

    
    void* xpub_socket = zmq_socket(context, ZMQ_XPUB);
    zmq_bind(xpub_socket, "tcp://*:5556");

    
    void* capture_socket = zmq_socket(context, ZMQ_PAIR);
    zmq_bind(capture_socket, "inproc://capture");

    
    pthread_t cap_thread;
    pthread_create(&cap_thread, NULL, capture_thread, context);

    printf("Broker with capture is starting...\n");

    
    
    
    zmq_proxy(xsub_socket, xpub_socket, capture_socket);

    
    printf("Broker shutting down...\n");

    
    zmq_close(xsub_socket);
    zmq_close(xpub_socket);
    zmq_close(capture_socket);
    zmq_ctx_destroy(context);

    
    pthread_join(cap_thread, NULL);

    return 0;
}
