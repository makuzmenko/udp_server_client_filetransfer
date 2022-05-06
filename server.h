#pragma once
#ifndef UDPSC_SERVER
#define UDPSC_SERVER

#include <functional>
#include <map>
#include "protocol.h"
#include "packet.h"
#include "stream.h"
#include "udp_server.h"
#include "iobserver.h"

/**
* server_context - класс реализующий бизнес логику обработки входящих пакетов,
* может работать сразу с множеством входящих файлов.
* Однопоточный.
*
* В ТЗ написано что сервер должен хранить данные в оперативе.
* Правильнее было бы хранить их сразу на диске.
*/
class server_context
{
public:
    /// конструктор по-умолчанию удален
	server_context() = delete;

	/**
	* Конструктор
	*
	* \param [in] _srv указатель на UDP сервер
	*/
	explicit server_context(udp_server *_srv) : udp_srv(_srv) {
		if (!_srv)
			throw "udp_server address is NULL\n";
	}

	/// Регистрирует обсервер
	void register_observer(iobserver *observer){
        observer_list.push_back(observer);
	}

	/// Уведомляет все обсерверы о событии
	void notify_all(event_type type, uint32_t chunk_id, std::string msg = ""){
        for(auto i : observer_list){
            i->event(type, chunk_id, msg);
        }
	}

	/**
	* Функция для обратного вызова. Вызывается UDP сервером mServer
	*
	* \param [in] a указатель на структуру sockPtr на которую ссылается epoll event
	* \param [in] addr указатель на структуру sockaddr_in
	* \param [in] buf указатель на буфер с содержимым пакета
	* \param [in] len размер пакета
	*/
	void receive_packet(sockPtr* a, const struct sockaddr_in* addr, char *buf, int len/*std::vector<BYTE>& new_packet*/)
	{
        std::vector<BYTE> new_packet((BYTE*)buf,(BYTE*)(buf+len));

		packet pkt(new_packet);

		if (pkt.header()->type == p_type::PUT)
		{
			auto stream = &streams[std::vector<BYTE>(std::begin(pkt.header()->id), std::end(pkt.header()->id))];
            std::vector<BYTE> p_data(pkt.payload(), pkt.payload()+pkt.payload_size());
			stream->put(pkt.header()->seq_total,
						pkt.header()->seq_number,
						p_data);

            notify_all(event_type::receive_chunk,pkt.header()->seq_number, std::string((char*)&(pkt.header()->id[0]),8));

			if (stream->get_size() == pkt.header()->seq_total) {
				uint32_t crc32 = stream->get_crc32();
				pkt.set_payload(&crc32, sizeof(crc32));
			}

			pkt.header()->seq_total = stream->get_size();
			pkt.header()->type = p_type::ACK;

			auto resp = pkt.make_packet();

			udp_srv->send_udp(*a, addr, (char*)resp.data(), resp.size());
			notify_all(event_type::send_ack,pkt.header()->seq_number, std::string((char*)&(pkt.header()->id[0]),8));
		}
	}

private:
	udp_server* udp_srv;
	std::map<std::vector<BYTE>, stream> streams;
	std::vector<iobserver*> observer_list;
};

#endif // !UDPSC_SERVER

