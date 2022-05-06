#ifndef IOBSERVER_H
#define IOBSERVER_H

#include <string>

enum class event_type {create_chunks, send_chunk, receive_ack, send_ack, receive_chunk};

class iobserver
{
    public:
        iobserver() {}
        virtual ~iobserver() {}

        virtual void event(event_type type, uint32_t chunk_id, std::string msg = "") = 0;

    protected:

    private:
};

#endif // IOBSERVER_H
