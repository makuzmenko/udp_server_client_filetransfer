#ifndef OBSERVER_BASE_H
#define OBSERVER_BASE_H

#include "iobserver.h"

class observer_base : public iobserver
{
public:
    observer_base() {}
    virtual ~observer_base() {}
    virtual void event(event_type type, uint32_t chunk_id, std::string msg = ""){
        std::lock_guard<std::mutex> lg(mutex);

        if(msg.size() != 0)
            std::cout << msg << ": ";

        switch(type){
        case event_type::create_chunks:
                std::cout << "Create " << chunk_id << " chunks" << std::endl;
            break;
        case event_type::send_chunk:
                std::cout << "Send chunk id " << chunk_id << std::endl;
            break;
        case event_type::receive_ack:
                std::cout << "Receive ACK chunk id " << chunk_id << std::endl;
            break;
        case event_type::receive_chunk:
                std::cout << "Receive chunk id " << chunk_id << std::endl;
            break;
        case event_type::send_ack:
                std::cout << "Send ACK chunk id " << chunk_id << std::endl;
            break;
        default:
            break;
        };

        std::cout << std::flush;

    }
private:
    std::mutex mutex;
};


#endif // OBSERVER_BASE_H
