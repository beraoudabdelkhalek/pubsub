
#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    void *context = zmq_ctx_new();

    
    void *frontend = zmq_socket(context, ZMQ_XSUB);
    zmq_bind(frontend, "tcp://*:5555");

    
    void *backend = zmq_socket(context, ZMQ_XPUB);
    zmq_bind(backend, "tcp://*:5556");

    
    zmq_proxy(frontend, backend, NULL);

    
    zmq_close(frontend);
    zmq_close(backend);
    zmq_ctx_destroy(context);
    return 0;
}
