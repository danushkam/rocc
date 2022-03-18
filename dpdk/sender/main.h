/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#ifndef _MAIN_H_
#define _MAIN_H_

#include <stdbool.h>

#ifndef APP_MBUF_ARRAY_SIZE
#define APP_MBUF_ARRAY_SIZE 256
#endif

struct app_mbuf_array {
	struct rte_mbuf *array[APP_MBUF_ARRAY_SIZE];
	uint16_t n_mbufs;
};

#ifndef APP_MAX_PORTS
#define APP_MAX_PORTS 4
#endif

struct tp_meter {
        bool enabled;
        uint64_t intvl_num_pkts;
        uint64_t intvl_num_bits;
        uint64_t intvl_start_time;
};

struct sender_state {
	/* Rate limiter */
	bool rate_limit;
	bool can_send;
	uint64_t rate;
	double bkt_size;
	double curr_bkt_size;
	struct tp_meter tp_in;
        struct tp_meter tp_out;
	FILE *stat_vector;
	uint64_t start_time;
};

struct app_params {
	/* CPU cores */
	uint32_t core_rx;
	uint32_t core_worker;
	uint32_t core_tx;

	/* Ports*/
	uint32_t ports[APP_MAX_PORTS];
	uint32_t n_ports;

	/* Rings */
	struct rte_ring *rings_rx[APP_MAX_PORTS];
	struct rte_ring *rings_tx[APP_MAX_PORTS];
	uint32_t ring_rx_size;
	uint32_t ring_tx_size;

	/* Internal buffers */
	struct app_mbuf_array mbuf_rx[APP_MAX_PORTS];
	struct app_mbuf_array mbuf_wk[APP_MAX_PORTS];
	struct app_mbuf_array mbuf_tx[APP_MAX_PORTS];

	/* Buffer pool */
	struct rte_mempool *pool;
	uint32_t pool_buffer_size;
	uint32_t pool_size;
	uint32_t pool_cache_size;

	/* Burst sizes */
	uint32_t burst_size_rx_read;
	uint32_t burst_size_rx_write;
	uint32_t burst_size_worker_read;
	uint32_t burst_size_worker_write;
	uint32_t burst_size_tx_read;
	uint32_t burst_size_tx_write;

	/* Ethernet addresses */
	struct ether_addr port_eth_addr[APP_MAX_PORTS];
	struct ether_addr next_hop_eth_addr[APP_MAX_PORTS];

	/* sender state */
	struct sender_state sender_state[APP_MAX_PORTS];
} __rte_cache_aligned;

extern struct app_params app;

int app_parse_args(int argc, char **argv);
void app_print_usage(void);
void app_init(void);
int app_lcore_main_loop(void *arg);

void app_main_loop_rx(void);
void app_main_loop_worker(void);
void app_main_loop_tx(void);

#define IP_ICMP_ROCC_RATE 50

#define APP_FLUSH 0
#ifndef APP_FLUSH
#define APP_FLUSH 0x3FF
#endif

#define APP_METADATA_OFFSET(offset) (sizeof(struct rte_mbuf) + (offset))

#endif /* _MAIN_H_ */
