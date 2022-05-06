#pragma once
#ifndef UDPSC_PROTOCOL
#define UDPSC_PROTOCOL

#include <cstring>

#ifdef _WIN32
#include <Winsock2.h>
#else
#include <arpa/inet.h>
#endif

#ifdef __GNUC__
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif

typedef uint8_t BYTE;
enum class p_type : uint8_t {ACK = 0, PUT = 1};

PACK(struct p_header {
	uint32_t seq_number; // номер пакета
	uint32_t seq_total; // количество пакетов с данными
	p_type type; // тип пакета: 0 == ACK, 1 == PUT
	BYTE id[8]; // 8 байт - идентификатор, отличающий один файл от другого

	p_header() {
		seq_number = 0;
		seq_total = 0;
		type = p_type::ACK;
		std::memset(id, 0, sizeof id);
	}

	void ntoh()	{
		seq_number = ntohl(seq_number);
		seq_total = ntohl(seq_total);
	}

	void hton() {
		seq_number = htonl(seq_number);
		seq_total = htonl(seq_total);
	}
});

#endif // !UDPSC_PROTOCOL
