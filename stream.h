#pragma once
#ifndef UDPSC_STREAM
#define UDPSC_STREAM

#include <vector>
#include <map>
#include <stddef.h>
#include <stdint.h>

uint32_t crc32c(uint32_t crc, const unsigned char* buf, size_t len)
{
	int k;
	crc = ~crc;
	while (len--) {
		crc ^= *buf++;
		for (k = 0; k < 8; k++)
			crc = crc & 1 ? (crc >> 1) ^ 0x82f63b78 : crc >> 1;
	}
	return ~crc;
}

/**
* Класс хранит в себе все сегменты данных
*/
class stream
{
public:
	stream() : size(0), full_filled(false), crc32(0) {}

	/**
	* Заполняет сегмент новыми данными, и изменяет размер цепочки сегментов если понадобится
	*
	* \param [in] seq_total кол-во сегментов
	* \param [in] seq номер ткущего сегмента
	* \param [in] p_data ссылка на вектор с содержимым пакета
	*/
	void put(uint32_t seq_total, uint32_t seq, std::vector<BYTE> &p_data) {

		fit_size(seq_total);

		if (seq < data.size() && p_data.size() != 0) {
			if (data[seq].size() == 0)
				size++;

			if (data[seq] != p_data) {
				data[seq] = std::move(p_data);
			}

			if(size == data.size())
				calc_crc32();// расчитываем crc32 если получили все пакеты и не расчитывали crc32 до этого
		}
	}

	uint32_t get_size() {
		return size;
	}

	uint32_t get_crc32() {
		return crc32;
	}

private:
	std::vector< std::vector<BYTE> > data;
	std::vector<uint32_t> crc32_list;
	uint32_t size;
	bool full_filled;
	uint32_t crc32;

	/**
	* Устанавливает размер цепочки
	*
	* \param [in] size новый размер цепочки
	*/
	void fit_size(uint32_t size) {
		if (data.size() != size) {
			data.resize(size);
			crc32_list.resize(size);
		}
	}

	/**
	* Расчитывает контрольную сумму CRC32
	*/
	void calc_crc32() {
        for (auto i = data.begin(); i != data.end(); i++) {
            crc32 = crc32c(crc32, (unsigned char *)i->data(), i->size());
		}
	}

};

#endif //UDPSC_STREAM
