// device with zmq polling management (multi-part receive)
#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main() {
    void *context = zmq_ctx_new();
    int check = 0;

    // Publisher socket for uplink messages
    void *publisher = zmq_socket(context, ZMQ_PUB);
    check = zmq_connect(publisher, "tcp://127.0.0.1:5555");
    printf("pub socket created, %d\n", check);

    // Subscriber socket for downlink messages
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
        // Check for incoming downlink messages (timeout = 3ms)
        int rc = zmq_poll(items, 1, 3);
        printf("rc: %d\n", rc);

        if (rc > 0 && (items[0].revents & ZMQ_POLLIN)) {
            // We'll store the first two frames in these buffers
            char topic[256]   = {0};
            char message[256] = {0};

            int frame_index = 0;
            while (1) {
                zmq_msg_t frame;
                zmq_msg_init(&frame);

                // Non-blocking receive to mirror your original use of ZMQ_DONTWAIT
                int size = zmq_msg_recv(&frame, subscriber, ZMQ_DONTWAIT);
                if (size == -1) {
                    // If error or no more parts available immediately
                    zmq_msg_close(&frame);
                    break;
                }

                // Copy frame data into a temporary buffer for printing or storing
                char buffer[256] = {0};
                int copy_size = (size < (int)sizeof(buffer)) ? size : (int)sizeof(buffer) - 1;
                memcpy(buffer, zmq_msg_data(&frame), copy_size);
                buffer[copy_size] = '\0';

                // First frame goes to 'topic', second to 'message', ignoring additional frames
                if (frame_index == 0) {
                    strncpy(topic, buffer, sizeof(topic) - 1);
                } else if (frame_index == 1) {
                    strncpy(message, buffer, sizeof(message) - 1);
                }
                frame_index++;

                // Check if more frames exist in this multi-part message
                int more = 0;
                size_t more_size = sizeof(more);
                zmq_getsockopt(subscriber, ZMQ_RCVMORE, &more, &more_size);

                // Close the current frame
                zmq_msg_close(&frame);

                if (!more) {
                    // No more frames for this message
                    break;
                }
            }

            // Print the result like before
            // "5G Device received on <topic>: <message>"
            printf("5G Device received on %s: %s\n", topic, message);
        }

        // Send uplink message (two frames: topic + content)
        {
            char topic[] = "uplink";
            char message[256];
            snprintf(message, sizeof(message), "Uplink message %d from 5G Device 1", count++);
            // First frame: topic
            zmq_send(publisher, topic, strlen(topic), ZMQ_SNDMORE);
            // Second frame: actual message
            zmq_send(publisher, message, strlen(message), 0);
        }

        // Sleep for 1 second before sending next message
        sleep(1);
    }

    zmq_close(publisher);
    zmq_close(subscriber);
    zmq_ctx_destroy(context);

    return 0;
}
