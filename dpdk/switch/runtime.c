/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <sys/time.h>
#include <math.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_icmp.h>
#include <rte_malloc.h>

#include "main.h"

static void
update_in_throughput(
		uint32_t port,
                const uint64_t sec_tsc,
		struct rte_mbuf **array,
                uint16_t n_mbufs,
		uint16_t n_read)
{
	struct switch_state *switch_state;
	struct tp_meter *tp_meter;
        uint64_t cur_tsc;
        double interval, cur_time;
	uint16_t i;

	switch_state = &app.switch_state[port];
	tp_meter = &switch_state->tp_in;
        if (!tp_meter->enabled)
		return;

	cur_tsc = rte_rdtsc();
        interval = (cur_tsc - tp_meter->intvl_start_time) / (double)sec_tsc;

	for (i = n_mbufs - n_read; i < n_mbufs; i++) {
        	if (100 <= tp_meter->intvl_num_pkts ||
				sec_tsc <= cur_tsc - tp_meter->intvl_start_time) {
			cur_time = (cur_tsc - switch_state->start_time) / (double)sec_tsc;
                	fprintf(switch_state->stat_vector, "%f %f\n", cur_time,
					(tp_meter->intvl_num_bits / interval) / 1e6);
                	tp_meter->intvl_start_time = cur_tsc;
                	tp_meter->intvl_num_bits = 0;
                	tp_meter->intvl_num_pkts = 0;
        	}
        	tp_meter->intvl_num_bits += array[i]->pkt_len * 8;
        	tp_meter->intvl_num_pkts++;
	}
}

void
app_main_loop_rx(void) {
	uint32_t i;
	const uint64_t sec_tsc = rte_get_tsc_hz();

	RTE_LOG(INFO, USER1, "Core %u is doing RX\n", rte_lcore_id());

	for (i = 0; ; i = ((i + 1) % app.n_ports)) {
		uint16_t n_mbufs, n_batch, n_read, n_write;

		n_mbufs = app.mbuf_rx[i].n_mbufs;
		n_batch = APP_MBUF_ARRAY_SIZE - n_mbufs;
		if (app.burst_size_rx_read < n_batch)
			n_batch = app.burst_size_rx_read;

		/* Read */
		if (n_batch != 0) {
			n_read = rte_eth_rx_burst(
				app.ports[i],
				0,
				&app.mbuf_rx[i].array[n_mbufs],
				n_batch);
			n_mbufs += n_read;

			/* Update throughput */
			update_in_throughput(i, sec_tsc, app.mbuf_rx[i].array, n_mbufs, n_read);
		}

		if (n_mbufs == 0)
			continue;

		/* Send to worker */	
		n_write = rte_ring_sp_enqueue_burst(
				app.rings_rx[i],
				(void *)app.mbuf_rx[i].array,
				n_mbufs, NULL);

		if (0 < n_write && n_write < n_mbufs)
			memcpy((void *)app.mbuf_rx[i].array, 
					(void *)&app.mbuf_rx[i].array[n_write], 
					sizeof(struct rte_mbuf *) * (n_mbufs - n_write));
		app.mbuf_rx[i].n_mbufs = n_mbufs - n_write;
	}
}

static const uint32_t sn_0 = ((10 << 16) | (10 << 8) | 3);
static const uint32_t sn_1 = ((10 << 16) | (10 << 8) | 1);
static const uint32_t sn_2 = ((10 << 16) | (10 << 8) | 5);
static const uint32_t sn_3 = ((10 << 16) | (10 << 8) | 7);

static uint32_t
get_dest_port(struct rte_mbuf *m, uint32_t portid)
{
	unsigned dst_port = portid;

	if (RTE_ETH_IS_IPV4_HDR(m->packet_type)) {
		struct ipv4_hdr *ipv4_hdr;
		struct ether_hdr *eth_hdr;
		uint32_t in_addr;

		eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);
		ipv4_hdr = (struct ipv4_hdr *)(eth_hdr + 1);

		in_addr = rte_be_to_cpu_32(ipv4_hdr->dst_addr) >> 8;
		if (in_addr == sn_0)
			dst_port = 0;
		else if (in_addr == sn_1)
			dst_port = 1;
		else if (in_addr == sn_2)
			dst_port = 2;
		else if (in_addr == sn_3)
			dst_port = 3;
	}

	return dst_port;
}

void
app_main_loop_worker(void) {
	uint32_t i;

	RTE_LOG(INFO, USER1, "Core %u is doing work (L3 forwarding)\n",
		rte_lcore_id());

	for (i = 0; ; i = ((i + 1) % app.n_ports)) {
		uint16_t n_mbufs, n_batch, n_read;
		int ret;
		uint32_t j;

		n_mbufs = app.mbuf_wk[i].n_mbufs;
		n_batch = APP_MBUF_ARRAY_SIZE - n_mbufs;
		if (app.burst_size_worker_read < n_batch)
			n_batch = app.burst_size_worker_read;
		
		/* Read */
		if (n_batch != 0) {
			n_read = rte_ring_sc_dequeue_burst(
					app.rings_rx[i],
					(void **)&app.mbuf_wk[i].array[n_mbufs],
					n_batch,
					NULL);
			n_mbufs += n_read;
		}
		
		if (n_mbufs == 0)
			continue;

		/* Send to TX */
		for (j = 0; j < n_mbufs; j++) {
			uint32_t dst_port = get_dest_port(
					app.mbuf_wk[i].array[j], i);
			ret = rte_ring_sp_enqueue(
					app.rings_tx[dst_port],
					(void *)app.mbuf_wk[i].array[j]);
			if (ret != 0)
				break;
		}
	
		if (0 < j && j < n_mbufs)
			memcpy((void *)app.mbuf_wk[i].array, 
					(void *)&app.mbuf_wk[i].array[j], 
					sizeof(struct rte_mbuf *) * (n_mbufs - j));
		app.mbuf_wk[i].n_mbufs = n_mbufs - j;
	}
}

static void
update_mac(struct rte_mbuf *m, unsigned portid)
{
	struct ether_hdr *eth;

	eth = rte_pktmbuf_mtod(m, struct ether_hdr *);

	/* s_addr = MAC of the dest port */
	ether_addr_copy(&app.port_eth_addr[portid], &eth->s_addr);

	/* d_addr = MAC of the next hop */
	ether_addr_copy(&app.next_hop_eth_addr[portid], &eth->d_addr);
}

static const uint64_t ROCC_CC_T = 100; /* us */
static const int F_MIN = 100; /* Mbps */
static const int F_MAX = 10 * 1000; /* Mbps */
static const int Q_MAX = 210 * 1000; /* Bytes */
static const int Q_MID = 150 * 1000; /* Bytes */
static const int Q_REF = 75 * 1000; /* Bytes */
static const int LINK_BW = 10 * 1000; /* Mbps */

static int
get_stable_rate(int q_curr, int q_old, int r_curr)
{
	double A, B, a, b, coef, ch;
	int correction;
	int rate;

        A = 0.4 / pow(2, 7);
        B = 0.4 / pow(2, 5);

	q_old = (q_curr + q_old) / 2;
	
	ch = LINK_BW / 2.0;
        if ((q_curr < (Q_REF / 8)) && (q_old < (Q_REF / 8))) {
            coef = 1;
        } else {
            coef = r_curr / ch;
        }
	
	if (coef < (1/64.0)) {
            coef = 1/64.0;
        }

        if (1 < coef) {
            coef = 1;
        }
	
	a = A * coef;
	b = B * coef;
	correction = a * (q_curr - Q_REF) + b * (q_curr - q_old);
	rate = r_curr - correction;
	
	return rate;
}

static uint64_t
get_fair_rate(uint32_t port)
{
 	struct switch_state *switch_state;
	int r_new;
	int r_curr;
	int q_curr;
	int q_old;
	
	switch_state = &app.switch_state[port];
	q_curr = switch_state->q_curr;
	q_old = switch_state->q_old;
	r_curr = switch_state->r_curr;

	// MD
	if ((Q_MAX < q_curr) && 
	    (Q_MAX < (q_curr - q_old))) {
		r_new = F_MIN;
	} else if (Q_MID < (q_curr - q_old)) {
		r_new = r_curr / 2;
	} else { // PI
		r_new = get_stable_rate(q_curr, q_old, r_curr);
	}
	
	if (r_new < F_MIN) {
		r_new = F_MIN;
	}
	if (F_MAX < r_new) {
		r_new = F_MAX;
	}

	switch_state->q_old = q_curr;
	switch_state->r_curr = r_new;

	return r_new;
}

#define IP_VERSION 0x40
#define IP_HDRLEN  0x05 /* default IP header length == five 32-bits words. */
#define IP_DEFTTL  64   /* from RFC 1340. */
#define IP_VHL_DEF (IP_VERSION | IP_HDRLEN)

static struct rte_mbuf*
create_icmp_msg(uint16_t src_port, uint32_t src_addr, uint32_t dst_addr, uint64_t rate)
{
	struct rte_mbuf *created_pkt;
        struct ether_hdr *eth_hdr;
	struct ipv4_hdr *ipv4_hdr;
        struct icmp_hdr *icmp_hdr;
	uint64_t *payload;

        size_t pkt_size;

	created_pkt = rte_pktmbuf_alloc(app.pool);
        if (created_pkt == NULL) {
		RTE_LOG(WARNING, USER1,
				"Failed to allocate mbuf for ICMP message\n");
                return NULL;
        }

	pkt_size = sizeof(struct ether_hdr) +
		sizeof(struct ipv4_hdr) + sizeof(struct icmp_hdr) + sizeof(uint64_t);
        created_pkt->data_len = pkt_size;
        created_pkt->pkt_len = pkt_size;

	/* Ethernet */
	eth_hdr = rte_pktmbuf_mtod(created_pkt, struct ether_hdr *);
	rte_eth_macaddr_get(src_port, &eth_hdr->s_addr);
	memset(&eth_hdr->d_addr, 0xFF, ETHER_ADDR_LEN);
        eth_hdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);

	/* IPv4 */
	ipv4_hdr = (struct ipv4_hdr *)((char *)eth_hdr +
			sizeof(struct ether_hdr));
	ipv4_hdr->version_ihl = IP_VHL_DEF;
	ipv4_hdr->next_proto_id = IPPROTO_ICMP;
	ipv4_hdr->total_length = rte_cpu_to_be_16(
			sizeof(struct ipv4_hdr) + sizeof(struct icmp_hdr) +
			sizeof(uint64_t));
	ipv4_hdr->time_to_live = IP_DEFTTL;
	ipv4_hdr->src_addr = rte_cpu_to_be_32(src_addr);
	ipv4_hdr->dst_addr = rte_cpu_to_be_32(dst_addr);
	ipv4_hdr->hdr_checksum = 0;

	/* ICMP */
	icmp_hdr = (struct icmp_hdr *)((char *)ipv4_hdr +
			sizeof(struct ipv4_hdr));
	icmp_hdr->icmp_type = IP_ICMP_ROCC_RATE;
	icmp_hdr->icmp_code = 0;

	/* Payload */
	payload = (uint64_t *)((char*)icmp_hdr + sizeof(struct icmp_hdr));
	*payload = rate;

	return created_pkt;
}

static void
send_fair_rate(uint32_t port)
{
	struct rte_mbuf *mbuf1, *mbuf2, *mbuf3;
	uint64_t rate;
	uint32_t src_addr, dst_addr;

	rate = get_fair_rate(port);

	src_addr = (10 << 24) | (10 << 16) | (2 << 8) | 2;
	dst_addr = (10 << 24) | (10 << 16) | (2 << 8) | 1;
	mbuf1 = create_icmp_msg(1, src_addr, dst_addr, rate);
	
	src_addr = (10 << 24) | (10 << 16) | (4 << 8) | 2;
	dst_addr = (10 << 24) | (10 << 16) | (4 << 8) | 1;
	mbuf2 = create_icmp_msg(2, src_addr, dst_addr, rate);
	
	src_addr = (10 << 24) | (10 << 16) | (6 << 8) | 2;
	dst_addr = (10 << 24) | (10 << 16) | (6 << 8) | 1;
	mbuf3 = create_icmp_msg(3, src_addr, dst_addr, rate);
	
	rte_eth_tx_burst(1, 0, &mbuf1, 1);
	rte_eth_tx_burst(2, 0, &mbuf2, 1);
	rte_eth_tx_burst(3, 0, &mbuf3, 1);
}

static void
app_process_cc_timer(
		uint64_t *prev_tsc, const uint64_t cc_tsc,
		const uint64_t sec_tsc, uint32_t port)
{
	uint64_t cur_tsc, diff_tsc;
	struct switch_state *switch_state;
	double cur_time; // sec
	
	switch_state = &app.switch_state[port];
	if (!switch_state->enabled)
		return;
	
	cur_tsc = rte_rdtsc();
	diff_tsc = cur_tsc - *prev_tsc;
	
	if (unlikely(cc_tsc <= diff_tsc)) {
		/* Calculate and send rate */
		send_fair_rate(port);
	
		cur_time = (cur_tsc - switch_state->start_time) / (double)sec_tsc;
		fprintf(switch_state->stat_vector, "%f %f %f\n",
				cur_time,
				switch_state->q_curr / 1000.0,
				switch_state->r_curr / 1000.0);
		*prev_tsc = cur_tsc;
	}
}

static void
update_out_throughput(
		uint32_t port,
                const uint64_t sec_tsc,
		struct rte_mbuf **array,
		uint16_t n_write)
{
	struct switch_state *switch_state;
	struct tp_meter *tp_meter;
        uint64_t cur_tsc;
        double interval, cur_time;
	uint16_t i;

	switch_state = &app.switch_state[port];
	tp_meter = &switch_state->tp_out;
        if (!tp_meter->enabled)
		return;

	cur_tsc = rte_rdtsc();
        interval = (cur_tsc - tp_meter->intvl_start_time) / (double)sec_tsc;

	for (i = 0; i < n_write; i++) {
        	if (100 <= tp_meter->intvl_num_pkts ||
				sec_tsc <= cur_tsc - tp_meter->intvl_start_time) {
			cur_time = (cur_tsc - switch_state->start_time) / (double)sec_tsc;
                	fprintf(switch_state->stat_vector, "%f %f\n", cur_time,
					(tp_meter->intvl_num_bits / interval) / 1e6);
                	tp_meter->intvl_start_time = cur_tsc;
                	tp_meter->intvl_num_bits = 0;
                	tp_meter->intvl_num_pkts = 0;
        	}
        	tp_meter->intvl_num_bits += array[i]->pkt_len * 8;
        	tp_meter->intvl_num_pkts++;
	}
}

void
app_main_loop_tx(void) {
	uint32_t i;
	uint64_t prev_tsc;
	const uint64_t cc_tsc = (rte_get_tsc_hz() / US_PER_S) * ROCC_CC_T;
	const uint64_t sec_tsc = rte_get_tsc_hz();

	prev_tsc = 0;

	RTE_LOG(INFO, USER1, "Core %u is doing TX\n", rte_lcore_id());
	
	for (i = 0; ; i = ((i + 1) % app.n_ports)) {
		uint16_t n_mbufs, n_batch, n_read, n_write;
		uint32_t k;

		/* Process ROCC timer */
		app_process_cc_timer(&prev_tsc, cc_tsc, sec_tsc, i);

		n_mbufs = app.mbuf_tx[i].n_mbufs;
		n_batch = APP_MBUF_ARRAY_SIZE - n_mbufs;
		if (app.burst_size_tx_read < n_batch)
			n_batch = app.burst_size_tx_read;

		/* Read */
		if (n_batch != 0) { /* Buffer not full */
			n_read = rte_ring_sc_dequeue_burst(
					app.rings_tx[i], 
					(void **)&app.mbuf_tx[i].array[n_mbufs],
					n_batch,
					NULL);
			n_mbufs += n_read;
		}

		if (n_mbufs == 0)
			continue;

		for (k = 0; k < n_mbufs; k++)
			update_mac(app.mbuf_tx[i].array[k], i);
		
		/* Send */
		n_write = rte_eth_tx_burst(
				app.ports[i],
				0,
				app.mbuf_tx[i].array,
				n_mbufs);

		if (n_write != 0) {
			/* Update throughput */
			update_out_throughput(i, sec_tsc, app.mbuf_tx[i].array, n_write);
		}

		if (0 < n_write && n_write < n_mbufs)
			memcpy((void *)app.mbuf_tx[i].array, 
					(void *)&app.mbuf_tx[i].array[n_write], 
					sizeof(struct rte_mbuf *) * (n_mbufs - n_write));
		app.mbuf_tx[i].n_mbufs = n_mbufs - n_write;

		struct switch_state *switch_state = &app.switch_state[i];
		switch_state->q_curr = 0;
		for (k = 0; k < app.mbuf_tx[i].n_mbufs; k++)
			switch_state->q_curr += app.mbuf_tx[i].array[k]->pkt_len;
	}
}
