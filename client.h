#pragma once
#ifndef UDPSC_CLIENT
#define UDPSC_CLIENT

#include <iostream>
#include <fstream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <map>
#include <mutex>
#include <random>
#include <algorithm>

#include "packet.h"
#include "stream.h"
#include "udp_client.h"
#include "iobserver.h"

#define UDP_PDU (1472-sizeof(p_header))

#ifdef _WIN32
#include <windows.h>
void _sleep(unsigned milliseconds)
{
	Sleep(milliseconds);
}
#else
#include <unistd.h>
void _sleep(unsigned milliseconds)
{
	usleep(milliseconds * 1000);
}
#endif

/**
* client_context - класс реализующий бизнес логику отправки файлов на сервер,
* Однопоточный.
*/
class client_context
{
public:
    ///конструктор по-умолчанию удален
	client_context() = delete;

    /**
    * Конструктор
	*
	* \param [in] _clnt указатель на UDP клиент
	*/
	explicit client_context(udp_client* _clnt) : clnt(_clnt), crc32(0), completed(false) {
		if (!_clnt)
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
	* Функция для отправки файла серверу.
	* Отправляет сегменты файла в случайном порядке, за исключением последнего
	* сегмента, он отправляется всегда последним.
	*
	* \param [in] id идентификатор файла
	* \param [in] file ссылка на std::ifstream отправляемого файла
	* \param [in] data_rate максимальная скорость передачи данных
	* \param [in] timeout таймаут ожидания отправки сегментов
	* \return успех передачи файла
	*/
	bool send_file(BYTE id[8], std::ifstream& file, size_t data_rate = 1250000, size_t timeout = 1000) {

		make_chunks_from_file(file);

		packet pkt;
		memcpy(&(pkt.header()->id[0]) , &id[0], 8);
		pkt.header()->seq_total = (uint32_t)chunks.size();
		pkt.header()->type = p_type::PUT;

		crc32 = calc_crc32_from_file(file);

		try {
			while (chunks.size() != 0) {
				_sleep(timeout/5);
				size_t remain_data = data_rate/5;

				std::lock_guard<std::mutex> lg(mutex);
				for (auto i : chunks) {
					if (remain_data >= 0) {
                        remain_data -= send_chunk(pkt, file, i);
					}
					else {
						break;
					}
				}
			}

			return completed;
		}
		catch (...) {
			std::cerr << "file processing fail" << std::endl;
			return false;
		}

		return false;
	}

    /**
	* Функция для обратного вызова. Вызывается UDP клиентом mClient при получении пакета
	*
	* \param [in] buf указатель на буфер с содержимым пакета
	* \param [in] len размер пакета
	*/
	void receive_packet(char *buf, int len) {
		std::lock_guard<std::mutex> lg(mutex);

		std::vector<BYTE> new_packet((BYTE*)buf,(BYTE*)(buf+len));
		packet pkt(new_packet);

		if (pkt.header()->type == p_type::ACK) {
			chunks.erase(std::remove(chunks.begin(), chunks.end(), pkt.header()->seq_number), chunks.end());
			notify_all(event_type::receive_ack,pkt.header()->seq_number,std::string((char*)&(pkt.header()->id[0]),8));

			if (pkt.header()->seq_total == seq_total) {
				if (pkt.payload_size() == sizeof(uint32_t)) {
					uint32_t* remote_crc32 = reinterpret_cast<uint32_t*>(pkt.payload());
					if (*remote_crc32 == crc32) {
						completed = true;
						chunks.clear();
					}
				}
			}
		}
	}
protected:

    /**
    * Отправляет сегмент файла серверу
    *
    * \param [in] pkt ссылка на структуру пакета
    * \param [in] file ссылка на файл
    * \param [in] i номер отправляемого сегмента
    * \return размер отправленного сегмента
    */
    size_t send_chunk(packet &pkt, std::ifstream& file, uint32_t i){
        pkt.header()->seq_number = i;
        auto p = read_chunk_from_file(file, i);
        pkt.set_payload(p.data(), p.size());
        size_t p_size = pkt.payload_size();
        std::vector<BYTE> new_packet = std::move(pkt.make_packet());
        clnt->send((char*)new_packet.data(), new_packet.size());
        notify_all(event_type::send_chunk,i,std::string((char*)&(pkt.header()->id[0]),8));

        return p_size;
    }

    /**
    * Создает вектор со случайным расположением номеров
    * сегментов для отправки серверу. Последний сегмент
    * всегда будет в конце
    *
    * \param [in] file ссылка на файл
    */
	void make_chunks_from_file(std::ifstream &file) {

		if (!file.is_open())
			throw "file not found\n";

		chunks.clear();

		file.seekg(0, file.end);
		size_t f_size = file.tellg();
		file.seekg(0, file.beg);

		seq_total = (f_size / UDP_PDU);// + 1;
		chunks.resize(seq_total);
		notify_all(event_type::create_chunks, seq_total);

		std::random_device rd;
		std::mt19937 g(rd());
		uint32_t chunk_n = 0;
		std::for_each(chunks.begin(), chunks.end(), [&chunk_n](auto& p) { p = chunk_n++; });
		std::shuffle(chunks.begin(), chunks.end(), g);

		chunks.push_back(seq_total);
		seq_total++;
	}

	/**
    * Создает вектор с содержимым сегмента файла по его номеру
    *
    * \param [in] file ссылка на файл
    * \param [in] chunk_n номер отправляемого сегмента
    * \return вектор с содержимым сегмента
    */
	std::vector<BYTE> read_chunk_from_file(std::ifstream & file, uint32_t chunk_n) {
		char buffer[UDP_PDU];

		if (!file.is_open())
			throw "file not found\n";

		file.seekg(0, file.beg);
		file.seekg(chunk_n * UDP_PDU, file.beg);
		file.read(&buffer[0], UDP_PDU);
		std::streamsize s = file.gcount();

		file.clear();

		std::vector<BYTE> ret = std::vector<BYTE>(std::begin(buffer), std::begin(buffer) + s);
		return ret;
	}

	/**
    * Расчитывает котрольную сумму crc32 для файла
    *
    * \param [in] file ссылка на файл
    * \return контрольная сумма
    */
	uint32_t calc_crc32_from_file(std::ifstream& file) {
		uint32_t crc32 = 0;
		for (size_t i = 0; i < chunks.size(); i++) {
			auto data = read_chunk_from_file(file, i);
			crc32 = crc32c(crc32, (unsigned char*)data.data(), data.size());
		}
		return crc32;
	}

private:
	udp_client* clnt;
	std::mutex mutex;
	std::vector<uint32_t> chunks;
	uint32_t crc32;
	uint32_t seq_total;
	bool completed;

	std::vector<iobserver*> observer_list;
};

#endif //UDPSC_CLIENT
