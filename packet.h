#pragma once
#ifndef UDPSC_PACKET
#define UDPSC_PACKET

#include <vector>
#include "protocol.h"

/**
* Вспомогательный класс для обработки пакетов.
* Позволяет получить доступ к заголовку и содержимому пакета
*/
class packet
{
public:
	packet() {
        _data.resize(sizeof(p_header),0);
        recalc_ptrs();
	}

	/**
	* Конструктор, заполняющий структуру пакета из полученных данных
	* Данные в поля НЕ копируются, они ссылаются на имеющиеся области памяти с содержимым пакета
	*
	* \param [in] p_data ссылка на пакет
	*/
	explicit packet(std::vector<BYTE> &p_data) {
		_data = std::move(p_data);

		if(_data.size() >= sizeof(p_header)){
            recalc_ptrs();
            _header->ntoh();
		}
		else{
            _data.resize(sizeof(p_header),0);
            recalc_ptrs();
		}
	}

	p_header* header() {
		return _header;
	}

	BYTE *payload() {
		return _payload;
	}

	size_t payload_size(){
        return _payload_size;
	}

	/**
	* Создает новый пакет с BigEndian(network) порядком байт, КОПИРУЯ текущий пакет.
	*
	* \return вектор с содержимым нового пакета
	*/
	std::vector<BYTE> make_packet() {
		std::vector<BYTE> packet = _data;
		p_header* header_view = reinterpret_cast<p_header*>(packet.data());
		header_view->hton();
		return packet;
	}

	/**
	* Добавляет данные к текущему пакету.
	*
	* \param [in] ptr указатель на массив данных
	* \param [in] size размер данных
	*/
	void set_payload(void* ptr, size_t size) {
		_data.resize(size+sizeof(p_header));
		std::memcpy(_data.data()+sizeof(p_header), ptr, size);

		recalc_ptrs();
	}

private:
	p_header *_header;
	std::vector<BYTE> _data;
	BYTE *_payload;
	size_t _payload_size;

	/**
	* пересчитывает указатели на структуры данных внутри пакета.
	*/
	void recalc_ptrs(){
        _header = reinterpret_cast<p_header*>(_data.data());
        _payload = reinterpret_cast<BYTE*>(_data.data() + sizeof(p_header));
        _payload_size = _data.size() - sizeof(p_header);
	}
};



#endif // !UDPSC_PACKET
