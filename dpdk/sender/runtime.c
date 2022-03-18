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
#include <sys/time.h>

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
#include <rte_icmp.h>
#include <rte_malloc.h>

#include "main.h"

static const uint64_t ROCC_RL_T = 5; /* us */

static void
apply_rate(uint32_t port, uint64_t rate)
{
	struct sender_state *sender_state;

	sender_state = &app.sender_state[port];
	sender_state->rate = rate;
	sender_state->bkt_size = (rate * ROCC_RL_T) / 8.0; // rate in Mbps
	sender_state->rate_limit = true;

	//RTE_LOG(INFO, USER1, "port: %u, rate: %lu\n", port, rate);
}

static void
update_in_throughput(
                uint32_t port,
                const uint64_t sec_tsc,
                struct rte_mbuf **array,
                uint16_t n_mbufs,
                uint16_t n_read)
{
        struct sender_state *sender_state;
	struct tp_meter *tp_meter;
        uint64_t cur_tsc;
        double interval, cur_time;
        uint16_t i;

        sender_state = &app.sender_state[port];
	tp_meter = &sender_state->tp_in;
	if (!tp_meter->enabled)
                return;

        cur_tsc = rte_rdtsc();
        interval = (cur_tsc - tp_meter->intvl_start_time) / (double)sec_tsc;

        for (i = n_mbufs - n_read; i < n_mbufs; i++) {
                if (100 <= tp_meter->intvl_num_pkts ||
				sec_tsc <= cur_tsc - tp_meter->intvl_start_time) {
			cur_time = (cur_tsc - sender_state->start_time) / (double)sec_tsc;
			fprintf(sender_state->stat_vector, "%f %f\n", cur_time,
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
		uint32_t j;

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

		/* Consume rate messages */
		for (j = 0; j < n_mbufs; j++) {
			struct rte_mbuf *buf;
			struct ether_hdr *eth_hdr;
			struct ipv4_hdr *ipv4_hdr;
			struct icmp_hdr *icmp_hdr;
			
			buf = app.mbuf_rx[i].array[j];
			if (!RTE_ETH_IS_IPV4_HDR(buf->packet_type))
				continue;
			eth_hdr = rte_pktmbuf_mtod(buf, struct ether_hdr *);
			ipv4_hdr = (struct ipv4_hdr *)(eth_hdr + 1);
			if (ipv4_hdr->next_proto_id != IPPROTO_ICMP)
				continue;
			icmp_hdr = (struct icmp_hdr *)(ipv4_hdr + 1);
			if (icmp_hdr->icmp_type != IP_ICMP_ROCC_RATE)
				continue;

			apply_rate(i, *(uint64_t*)(icmp_hdr + 1));
			rte_pktmbuf_free(buf);
			if (j + 1 < n_mbufs) { /* There is next buf */
				memcpy((void *)&app.mbuf_rx[i].array[j],
						(void *)&app.mbuf_rx[i].array[j + 1],
						sizeof(struct rte_mbuf *) * (n_mbufs - 1 - j));
			}
			n_mbufs--;
			j--;
		}

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

static uint32_t
get_dest_port(uint32_t portid)
{
	if ((portid % 2) == 0)
		return portid + 1;
	else
		return portid - 1;
}

void
app_main_loop_worker(void) {
	uint32_t i;

	RTE_LOG(INFO, USER1, "Core %u is doing work (port mapping)\n",
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
                        uint32_t dst_port = get_dest_port(i);
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

static void
app_process_rl_timer(
                uint64_t *prev_tsc, uint64_t *timer_tsc,
                const uint64_t rl_tsc, uint32_t port)
{
        uint64_t cur_tsc, diff_tsc;
	struct sender_state *sender_state;

	sender_state = &app.sender_state[port];
	if (!sender_state->rate_limit)
		return;

        cur_tsc = rte_rdtsc();
        diff_tsc = cur_tsc - *prev_tsc;

        *timer_tsc += diff_tsc;

        if (unlikely(*timer_tsc >= rl_tsc)) {
                /* Reset RL bucket size */
		/*fprintf(sender_state->stat_vector, "int: %lu\n",
		 		*timer_tsc * US_PER_S / rte_get_tsc_hz());*/
		if (sender_state->bkt_size <= sender_state->curr_bkt_size) {
			sender_state->curr_bkt_size -= sender_state->bkt_size;
		}
		sender_state->can_send = sender_state->curr_bkt_size < sender_state->bkt_size;
                *timer_tsc = 0;
        }

        *prev_tsc = cur_tsc;
}

static void
update_out_throughput(
		struct sender_state *sender_state,
		const uint64_t sec_tsc,
		uint32_t bits)
{
	struct tp_meter *tp_meter;
	uint64_t cur_tsc;
	double interval, cur_time;

	tp_meter = &sender_state->tp_out;
        if (!tp_meter->enabled)
                return;

	cur_tsc = rte_rdtsc();
	interval = (cur_tsc - tp_meter->intvl_start_time) / (double)sec_tsc;

	if (100 <= tp_meter->intvl_num_pkts ||
			sec_tsc <= cur_tsc - tp_meter->intvl_start_time) {
		cur_time = (cur_tsc - sender_state->start_time) / (double)sec_tsc;
		fprintf(sender_state->stat_vector, "%f %f\n", cur_time,
				(tp_meter->intvl_num_bits / interval) / 1e6);
		tp_meter->intvl_start_time = cur_tsc;
		tp_meter->intvl_num_bits = 0;
		tp_meter->intvl_num_pkts = 0;
	}
	tp_meter->intvl_num_bits += bits;
	tp_meter->intvl_num_pkts++;
}

void
app_main_loop_tx(void) {
	uint32_t i;
	uint64_t prev_tsc, timer_tsc;
	const uint64_t rl_tsc = (rte_get_tsc_hz() / US_PER_S) * ROCC_RL_T;
	const uint64_t sec_tsc = rte_get_tsc_hz();

	prev_tsc = 0;
        timer_tsc = 0;

	RTE_LOG(INFO, USER1, "Core %u is doing TX\n", rte_lcore_id());

	for (i = 0; ; i = ((i + 1) % app.n_ports)) {
		struct sender_state *sender_state;
		struct rte_mbuf *mbuf;
		uint64_t pkt_len;
		uint16_t n_mbufs, n_batch, n_read, n_write;

		/* Process rate limit timer */
		app_process_rl_timer(&prev_tsc, &timer_tsc, rl_tsc, i);

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

                /* Send */
		sender_state = &app.sender_state[i];
		mbuf = app.mbuf_tx[i].array[0];
		pkt_len = mbuf->pkt_len;
		n_write = 0;
		if (sender_state->can_send) {
                        update_mac(mbuf, i);
                	n_write = rte_eth_tx_burst(app.ports[i], 0, &mbuf, 1);
		}
		if (n_write != 0) { /* Written; n_write == 1 */
			if (n_write < n_mbufs)
				memcpy((void *)app.mbuf_tx[i].array,
						(void *)&app.mbuf_tx[i].array[n_write],
						sizeof(struct rte_mbuf *) * (n_mbufs - n_write));
			if (sender_state->rate_limit) {
				sender_state->curr_bkt_size += pkt_len;
				sender_state->can_send = sender_state->curr_bkt_size < sender_state->bkt_size;
			}
			update_out_throughput(sender_state, sec_tsc, pkt_len * 8);
		}
                app.mbuf_tx[i].n_mbufs = n_mbufs - n_write;
		/*fprintf(sender_state->stat_vector, "port: %u, bkt: %f, curr: %f, pkt: %lu, sent: %d\n",
				i, sender_state->bkt_size, sender_state->curr_bkt_size, pkt_len, n_write);*/
	}
}
