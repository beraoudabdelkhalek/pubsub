#include <zmq.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void* capture_thread(void* arg) {
    void* context = arg;
    
    // Create the capture socket and connect to inproc://capture
    void* capture = zmq_socket(context, ZMQ_PAIR);
    zmq_connect(capture, "inproc://capture");
    
    printf("Capture thread started, waiting for messages...\n");
    
    while (1) {
        // A ZeroMQ multipart message may have multiple frames.
        // We'll read all frames until no more remain.
        
        zmq_msg_t frame;
        zmq_msg_init(&frame);
        
        int rc = zmq_msg_recv(&frame, capture, 0);
        if (rc == -1) {
            zmq_msg_close(&frame);
            break;  // Interrupted or context shutdown
        }

        // Print the received frame
        printf("[CAPTURE] Received frame of size %d: %.*s\n",
               rc, rc, (char*)zmq_msg_data(&frame));
        
        // Check if there is more to this multipart message
        int more = 0;
        size_t more_size = sizeof(more);
        zmq_getsockopt(capture, ZMQ_RCVMORE, &more, &more_size);
        
        zmq_msg_close(&frame);
        
        // If no more frames, this message is complete
        if (!more) {
            printf("[CAPTURE] End of multipart message.\n");
        }
    }
    
    zmq_close(capture);
    return NULL;
}

int main() {
    void* context = zmq_ctx_new();

    // 1) Create XSUB socket for publishers
    void* xsub_socket = zmq_socket(context, ZMQ_XSUB);
    zmq_bind(xsub_socket, "tcp://*:5555");

    // 2) Create XPUB socket for subscribers
    void* xpub_socket = zmq_socket(context, ZMQ_XPUB);
    zmq_bind(xpub_socket, "tcp://*:5556");

    // 3) Create a capture PAIR socket, bind it
    void* capture_socket = zmq_socket(context, ZMQ_PAIR);
    zmq_bind(capture_socket, "inproc://capture");

    // 4) Start a thread to receive from the capture socket
    pthread_t cap_thread;
    pthread_create(&cap_thread, NULL, capture_thread, context);

    printf("Broker with capture is starting...\n");

    // 5) Use zmq_proxy with the capture socket as the third argument
    //    This will forward every message passing through the proxy
    //    to our capture_socket as well.
    zmq_proxy(xsub_socket, xpub_socket, capture_socket);

    // If zmq_proxy ever returns, it’s usually due to an error or shutdown
    printf("Broker shutting down...\n");

    // Clean up
    zmq_close(xsub_socket);
    zmq_close(xpub_socket);
    zmq_close(capture_socket);
    zmq_ctx_destroy(context);

    // Join capture thread (optional in a real app)
    pthread_join(cap_thread, NULL);

    return 0;
}
