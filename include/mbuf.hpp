#ifndef MBUF_HPP
#define MBUF_HPP

#ifdef WITH_DPDK

#include <cstdint>
#include <rte_mbuf.h>

struct mbuf : public rte_mbuf {
	void *getData() { return this->buf_addr; }
	uint16_t getDataLen() { return this->data_len; };
	void setDataLen(uint16_t l) { this->data_len = l; };
	uint16_t getBufLen() { return this->buf_len; }
};

#endif /* WITH_DPDK */

#endif /* MBUF_HPP */
