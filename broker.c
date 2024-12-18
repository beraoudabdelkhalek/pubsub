#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    void *context = zmq_ctx_new();
    printf("Broker is running using zmq proxy\n");
    // XSUB socket for publishers
    void *xsub_socket = zmq_socket(context, ZMQ_XSUB);
    printf("Broker is running using zmq proxy1\n");
    zmq_bind(xsub_socket, "tcp://*:5555");
    
    // XPUB socket for subscribers
    void *xpub_socket = zmq_socket(context, ZMQ_XPUB);
    zmq_bind(xpub_socket, "tcp://*:5556");
    printf("Broker is running using zmq proxy2\n");

    // Proxy between XSUB and XPUB sockets
    zmq_proxy(xsub_socket, xpub_socket, NULL);
    printf("Broker is running using zmq proxy\n");

    // Clean up
    zmq_close(xsub_socket);
    zmq_close(xpub_socket);
    zmq_ctx_destroy(context);

    return 0;
}
