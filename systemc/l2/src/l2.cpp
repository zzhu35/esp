// Copyright (c) 2011-2019 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#include "l2.hpp"

/*
 * Processes
 */

void l2::ctrl()
{

    bool is_flush_all;
	bool is_sync;
    {
        L2_RESET;

        is_flush_all = true;
        is_to_req[0] = 1;
        is_to_req[1] = 0;

        // Reset all signals and channels
        reset_io();

        wait();
    }

    // Main loop
    while(true) {

#ifdef L2_DEBUG
	bookmark_tmp = 0;
	asserts_tmp = 0;
#endif

        bool do_flush = false;
        bool do_rsp = false;
        bool do_fwd = false;
        bool do_ongoing_flush = false;
        bool do_cpu_req = false;
		bool do_sync = false;

        l2_rsp_in_t rsp_in;
        l2_fwd_in_t fwd_in;
        l2_cpu_req_t cpu_req;

        {
            HLS_DEFINE_PROTOCOL("llc-io-check");
			if (l2_sync.nb_can_get()) {
				l2_flush.nb_get(is_sync);
				do_sync = true;
            } else if (l2_flush.nb_can_get() && reqs_cnt == N_REQS) {
                is_flush_all = get_flush();
                ongoing_flush = true;
                do_flush = true;
			} else if (l2_rsp_in.nb_can_get()) {
                get_rsp_in(rsp_in);
                do_rsp = true;
            } else if ((l2_fwd_in.nb_can_get() && !fwd_stall) || fwd_stall_ended) {
                if (!fwd_stall) {
                    get_fwd_in(fwd_in);
                } else {
                    fwd_in = fwd_in_stalled;
                }
                do_fwd = true;
            } else if (ongoing_flush) {
                if (flush_set < L2_SETS) {
                    if (!l2_fwd_in.nb_can_get() && reqs_cnt != 0)
                        do_ongoing_flush = true;

                    if (flush_way == L2_WAYS) {
                        flush_set++;
                        flush_way = 0;
                    }
                } else {
		    		flush_set = 0;
                    flush_way = 0;
                    ongoing_flush = false;

					flush_done.write(true);
					wait();
					flush_done.write(false);
                }
            } else if ((l2_cpu_req.nb_can_get() || set_conflict) &&
                       !evict_stall && (reqs_cnt != 0 || ongoing_atomic)) { // assuming
                                                                            // HPROT
                                                                            // cacheable
                wait();
				if (!set_conflict) {
                    get_cpu_req(cpu_req);
                } else {
                    cpu_req = cpu_req_conflict;
                }

                do_cpu_req = true;
            }
        }

	if (do_sync) {

		self_invalidate();

	} else if (do_flush) {


	} else if (do_rsp) {

	    // if (ongoing_flush)
	    //     RSP_WHILE_FLUSHING;

	    line_breakdown_t<l2_tag_t, l2_set_t> line_br;
	    sc_uint<REQS_BITS> reqs_hit_i;

	    line_br.l2_line_breakdown(rsp_in.addr);

	    reqs_lookup(line_br, reqs_hit_i);

	    switch (rsp_in.coh_msg) {

	    case RSP_V :
	    {

		send_rd_rsp(rsp_in.line);

		reqs[reqs_hit_i].state = GPU_I;
		reqs_cnt++;

		put_reqs(line_br.set, reqs[reqs_hit_i].way, line_br.tag, rsp_in.line,
			 reqs[reqs_hit_i].hprot, GPU_V, reqs_hit_i);
	    }
	    break;
		
	    case RSP_NACK :
	    {

		// @TODO send retry
	    }
	    break;

	    default:
		RSP_DEFAULT;

	    }

	} else if (do_fwd) {

	// TODO probably not needed

	} else if (do_ongoing_flush) {
		self_invalidate();

	} else if (do_cpu_req) { // assuming HPROT cacheable

	    addr_breakdown_t addr_br;
	    sc_uint<REQS_BITS> reqs_hit_i;

	    addr_br.breakdown(cpu_req.addr);

	    set_conflict = reqs_peek_req(addr_br.set, reqs_hit_i);

	    if (ongoing_atomic) {

		// assuming there can only be 1 atomic operation in flight at a time

			if (atomic_line_addr != addr_br.line_addr) {

				ATOMIC_OVERRIDE;

				// atomic_conflict actually, but reuse variables
				set_conflict = true;
				cpu_req_conflict = cpu_req;

				reqs[reqs_atomic_i].state = INVALID;
				reqs_cnt++;

				wait();

				put_reqs(reqs[reqs_atomic_i].set, reqs[reqs_atomic_i].way,
					reqs[reqs_atomic_i].tag, reqs[reqs_atomic_i].line,
					reqs[reqs_atomic_i].hprot, MODIFIED, reqs_atomic_i);

				ongoing_atomic = false;

			} else {

				ATOMIC_CONTINUE;

				set_conflict = false;

				switch (cpu_req.cpu_msg) {

				case READ :

				ATOMIC_CONTINUE_READ;

				send_rd_rsp(reqs[reqs_atomic_i].line);

				reqs[reqs_atomic_i].state = INVALID;
				reqs_cnt++;

				wait();

				put_reqs(reqs[reqs_atomic_i].set, reqs[reqs_atomic_i].way, reqs[reqs_atomic_i].tag,
					reqs[reqs_atomic_i].line, reqs[reqs_atomic_i].hprot, MODIFIED, reqs_atomic_i);

				ongoing_atomic = false;

				break;

				case READ_ATOMIC :

				send_rd_rsp(reqs[reqs_atomic_i].line);

				break;

				case WRITE :
				case WRITE_ATOMIC :

				ATOMIC_CONTINUE_WRITE;

				write_word(reqs[reqs_atomic_i].line, cpu_req.word, addr_br.w_off,
					addr_br.b_off, cpu_req.hsize);

				reqs[reqs_atomic_i].state = INVALID;
				reqs_cnt++;

				wait();

				put_reqs(reqs[reqs_atomic_i].set, reqs[reqs_atomic_i].way, reqs[reqs_atomic_i].tag,
					reqs[reqs_atomic_i].line, reqs[reqs_atomic_i].hprot, MODIFIED, reqs_atomic_i);

				ongoing_atomic = false;

				break;

				}
			}

	    } else if (set_conflict) {

		SET_CONFLICT;

		cpu_req_conflict = cpu_req;

	    } else {

			bool tag_hit;
			l2_way_t way_hit;
			bool empty_way_found;
			l2_way_t empty_way;

			tag_lookup(addr_br, tag_hit, way_hit, empty_way_found, empty_way);

			if (cpu_req.cpu_msg == READ_ATOMIC) {
				reqs_atomic_i = reqs_hit_i;
				atomic_line_addr = addr_br.line_addr;
			}


			if (tag_hit) {

				switch (cpu_req.cpu_msg) {

				case READ : // read hit
				HIT_READ;

				// read response
				send_rd_rsp(line_buf[way_hit]);
				break;

				case READ_ATOMIC : // read atomic hit
				// @TODO
				break;

				case WRITE :

				write_word(line_buf[way_hit], cpu_req.word, addr_br.w_off, addr_br.b_off, cpu_req.hsize);
				send_req_out(REQ_WT, cpu_req.hprot, addr_br.line_addr, line_buf[way_hit], 1 << addr_br.w_off);
				break;
				
				}

			} else if (empty_way_found) {

				unstable_state_t state_tmp;
				coh_msg_t coh_msg_tmp;

				switch (cpu_req.cpu_msg) {

				case READ :
				{
				MISS_READ;
				send_req_out(REQ_V, cpu_req.hprot, addr_br.line_addr, 0, WORD_MASK_ALL);
				fill_reqs(cpu_req.cpu_msg, addr_br, addr_br.tag, evict_way, cpu_req.hsize, GPU_IV, cpu_req.hprot, cpu_req.word, line_buf[evict_way], reqs_hit_i);

				}
				break;

				case READ_ATOMIC :
				{
					// TODO
				}
				break;

				case WRITE :
				{
				write_word(line_buf[way_hit], cpu_req.word, addr_br.w_off, addr_br.b_off, cpu_req.hsize);
				send_req_out(REQ_WT, cpu_req.hprot, addr_br.line_addr, line_buf[way_hit], 1 << addr_br.w_off);

				}
				break;

				default:
				MISS_DEFAULT;
				}

			} else {

				evict_stall = true;
				line_addr_t line_addr_evict = (tag_buf[evict_way] << L2_SET_BITS) | (addr_br.set);
				l2_tag_t tag_tmp = addr_br.tag;
				addr_br.tag = tag_buf[evict_way];

				unstable_state_t state_tmp;
				coh_msg_t coh_msg_tmp;

				state_buf[evict_way] = GPU_I;

				send_inval(line_addr_evict);
				send_req_out(REQ_V, cpu_req.hprot, addr_br.line_addr, 0, WORD_MASK_ALL);
				fill_reqs(cpu_req.cpu_msg, addr_br, addr_br.tag, evict_way, cpu_req.hsize, GPU_IV, cpu_req.hprot, cpu_req.word, line_buf[evict_way], reqs_hit_i);
			}
	    }
	}

#ifdef L2_DEBUG
	// update debug vectors
	asserts.write(asserts_tmp);
	bookmark.write(bookmark_tmp);

	reqs_cnt_dbg.write(reqs_cnt);
	set_conflict_dbg.write(set_conflict);
	cpu_req_conflict_dbg.write(cpu_req_conflict);
	evict_stall_dbg.write(evict_stall);
	fwd_stall_dbg.write(fwd_stall);
	fwd_stall_ended_dbg.write(fwd_stall_ended);
	fwd_in_stalled_dbg.write(fwd_in_stalled);
	reqs_fwd_stall_i_dbg.write(reqs_fwd_stall_i);
	ongoing_atomic_dbg.write(ongoing_atomic);
	atomic_line_addr_dbg.write(atomic_line_addr);
	reqs_atomic_i_dbg.write(reqs_atomic_i);
	ongoing_flush_dbg.write(ongoing_flush);

	for (int i = 0; i < N_REQS; i++) {
	    REQS_DBG;
	    reqs_dbg[i] = reqs[i];
	}

	for (int i = 0; i < L2_WAYS; i++) {
	    BUFS_DBG;
	    tag_buf_dbg[i] = tag_buf[i];
	    state_buf_dbg[i] = state_buf[i];
	}

	evict_way_dbg.write(evict_way);
#endif

        wait();
    }
    /*
     * End of main loop
     */
}

/*
 * Functions
 */

inline void l2::reset_io()
{

    /* Reset put-get channels */
    l2_cpu_req.reset_get();
    l2_fwd_in.reset_get();
    l2_rsp_in.reset_get();
    l2_flush.reset_get();
    l2_rd_rsp.reset_put();
    l2_inval.reset_put();
    l2_req_out.reset_put();
    l2_rsp_out.reset_put();
#ifdef STATS_ENABLE
    l2_stats.reset_put();
#endif

    /* Reset memories */
    tags.port1.reset();
    hprots.port1.reset();
    lines.port1.reset();

    tags.port2.reset();
    hprots.port2.reset();
    lines.port2.reset();

#if (L2_WAYS >= 2)

    CACHE_REPORT_INFO("REPORT L2_WAYS");
    CACHE_REPORT_VAR(0, "L2_WAYS", L2_WAYS);

    tags.port3.reset();
    hprots.port3.reset();
    lines.port3.reset();

#if (L2_WAYS >= 4)

    tags.port4.reset();
    hprots.port4.reset();
    lines.port4.reset();

    tags.port5.reset();
    hprots.port5.reset();
    lines.port5.reset();

#if (L2_WAYS >= 8)

    tags.port6.reset();
    hprots.port6.reset();
    lines.port6.reset();

    tags.port7.reset();
    hprots.port7.reset();
    lines.port7.reset();

    tags.port8.reset();
    hprots.port8.reset();
    lines.port8.reset();

    tags.port9.reset();
    hprots.port9.reset();
    lines.port9.reset();

#endif
#endif
#endif

    evict_ways.port1.reset();
    evict_ways.port2.reset();

    /* Reset signals */

    flush_done.write(0);

#ifdef L2_DEBUG
    asserts.write(0);
    bookmark.write(0);

    /* Reset signals exported to output ports */
    reqs_cnt_dbg.write(0);
    set_conflict_dbg.write(0);
    // cpu_req_conflict_dbg.write(0);
    evict_stall_dbg.write(0);
    fwd_stall_dbg.write(0);
    fwd_stall_ended_dbg.write(0);
    // fwd_in_stalled_dbg.write(0);
    reqs_fwd_stall_i_dbg.write(0);
    ongoing_atomic_dbg.write(0);
    atomic_line_addr_dbg.write(0);
    reqs_atomic_i_dbg.write(0);
    ongoing_flush_dbg.write(0);
    flush_way_dbg.write(0);
    flush_set_dbg.write(0);
    tag_hit_req_dbg.write(0);
    way_hit_req_dbg.write(0);
    empty_found_req_dbg.write(0);
    empty_way_req_dbg.write(0);
    reqs_hit_req_dbg.write(0);
    reqs_hit_i_req_dbg.write(0);
    way_hit_fwd_dbg.write(0);
    peek_reqs_i_dbg.write(0);
    peek_reqs_i_flush_dbg.write(0);
    peek_reqs_hit_fwd_dbg.write(0);

    // for (int i = 0; i < N_REQS; i++) {
    // 	REQS_DBGPUT;
    // 	reqs_dbg[i] = 0;
    // }

    for (int i = 0; i < L2_WAYS; i++) {
    	BUFS_DBG;
    	tag_buf_dbg[i] = 0;
    	state_buf_dbg[i] = 0;
    }

    evict_way_dbg.write(0);

    /* Reset variables */
    asserts_tmp = 0;
    bookmark_tmp = 0;
#endif

    reqs_cnt = N_REQS;
    set_conflict = false;
    // cpu_req_conflict =
    evict_stall = false;
    fwd_stall = false;
    fwd_stall_ended = false;
    // fwd_in_stalled =
    reqs_fwd_stall_i = 0;
    ongoing_atomic = false;
    atomic_line_addr = 0;
    reqs_atomic_i = 0;
    ongoing_flush = false;
    flush_set = 0;
    flush_way = 0;

	for (int i = 0; i < L2_LINES; i++)
	{
        HLS_UNROLL_LOOP(ON, "reset-states");
        states[i] = GPU_I;
	}

}


/* Functions to receive input messages */

void l2::get_cpu_req(l2_cpu_req_t &cpu_req)
{
    GET_CPU_REQ;

    l2_cpu_req.nb_get(cpu_req);
}

void l2::get_fwd_in(l2_fwd_in_t &fwd_in)
{
    L2_GET_FWD_IN;


    l2_fwd_in.nb_get(fwd_in);
}

void l2::get_rsp_in(l2_rsp_in_t &rsp_in)
{
    L2_GET_RSP_IN;

    l2_rsp_in.nb_get(rsp_in);

}

bool l2::get_flush()
{
    GET_FLUSH;

    bool flush_tmp = false;

    // is_flush_all == 0 -> flush data, not instructions
    // is_flush_all == 1 -> flush data and instructions
    l2_flush.nb_get(flush_tmp);

    return flush_tmp;
}

/* Functions to send output messages */

void l2::send_rd_rsp(line_t line)
{
    SEND_RD_RSP;

    l2_rd_rsp_t rd_rsp;

    rd_rsp.line = line;

    l2_rd_rsp.put(rd_rsp);
}

void l2::send_inval(line_addr_t addr_inval)
{
    SEND_INVAL;

    l2_inval.put(addr_inval);
}

void l2::send_req_out(coh_msg_t coh_msg, hprot_t hprot, line_addr_t line_addr, line_t line, word_mask_t word_mask)
{
    SEND_REQ_OUT;

    l2_req_out_t req_out;

    req_out.coh_msg = coh_msg;
    req_out.hprot = hprot;
    req_out.addr = line_addr;
    req_out.line = line;
	req_out.word_mask = word_mask;

    while (!l2_req_out.nb_can_put()) wait();

    l2_req_out.nb_put(req_out);
}

void l2::send_rsp_out(coh_msg_t coh_msg, cache_id_t req_id, bool to_req, line_addr_t line_addr, line_t line)
{
    // SEND_RSP_OUT;

    // l2_rsp_out_t rsp_out;

    // rsp_out.coh_msg = coh_msg;
    // rsp_out.req_id  = req_id;
    // rsp_out.to_req  = to_req;
    // rsp_out.addr    = line_addr;
    // rsp_out.line    = line;
	// rsp_out.word_mask = WORD_MASK_ALL;

    // while (!l2_rsp_out.nb_can_put()) wait();

    // l2_rsp_out.nb_put(rsp_out);
}

#ifdef STATS_ENABLE
void l2::send_stats(bool stats)
{
    SEND_STATS;

    l2_stats.put(stats);
}
#endif


/* Functions to move around buffered lines */

void l2::fill_reqs(cpu_msg_t cpu_msg, addr_breakdown_t addr_br, l2_tag_t tag_estall, l2_way_t way_hit,
		   hsize_t hsize, unstable_state_t state, hprot_t hprot, word_t word, line_t line,
		   sc_uint<REQS_BITS> reqs_i)
{
    FILL_REQS;

    reqs[reqs_i].cpu_msg     = cpu_msg;
    reqs[reqs_i].tag	     = addr_br.tag;
    reqs[reqs_i].tag_estall  = tag_estall;
    reqs[reqs_i].set	     = addr_br.set;
    reqs[reqs_i].way	     = way_hit;
    reqs[reqs_i].hsize	     = hsize;
    reqs[reqs_i].w_off       = addr_br.w_off;
    reqs[reqs_i].b_off       = addr_br.b_off;
    reqs[reqs_i].state	     = state;
    reqs[reqs_i].hprot	     = hprot;
    reqs[reqs_i].invack_cnt  = MAX_N_L2;
    reqs[reqs_i].word	     = word;
    reqs[reqs_i].line	     = line;

    reqs_cnt--;
}

void l2::put_reqs(l2_set_t set, l2_way_t way, l2_tag_t tag, line_t line, hprot_t hprot, state_t state,
		  sc_uint<REQS_BITS> reqs_i)
{
    PUT_REQS;

    sc_uint<L2_SET_BITS+L2_WAY_BITS> base = set << L2_WAY_BITS;

    lines.port1[0][base + way]  = line;
    hprots.port1[0][base + way] = hprot;
    states[base + way] = state;
    tags.port1[0][base + way]   = tag;

    // if necessary end the forward messages stall
    if (fwd_stall && reqs_fwd_stall_i == reqs_i) {
	fwd_stall_ended = true;
    }
}

/* Functions to search for cache lines either in memory or buffered */
inline void l2::read_set(l2_set_t set)
{
    //Manual unroll because these are explicit memories, see commented code
    // below for implicit memories usage
    sc_uint<L2_SET_BITS+L2_WAY_BITS> base = set << L2_WAY_BITS;

    tag_buf[0] = tags.port2[0][base + 0];
    state_buf[0] = states[base + 0];
    hprot_buf[0] = hprots.port2[0][base + 0];
    line_buf[0] = lines.port2[0][base + 0];

#if (L2_WAYS >= 2)

    tag_buf[1] = tags.port3[0][base + 1];
    state_buf[1] = states[base + 1];
    hprot_buf[1] = hprots.port3[0][base + 1];
    line_buf[1] = lines.port3[0][base + 1];

#if (L2_WAYS >= 4)

    tag_buf[2] = tags.port4[0][base + 2];
    state_buf[2] = states[base + 2];
    hprot_buf[2] = hprots.port4[0][base + 2];
    line_buf[2] = lines.port4[0][base + 2];

    tag_buf[3] = tags.port5[0][base + 3];
    state_buf[3] = states[base + 3];
    hprot_buf[3] = hprots.port5[0][base + 3];
    line_buf[3] = lines.port5[0][base + 3];

#if (L2_WAYS >= 8)

    tag_buf[4] = tags.port6[0][base + 4];
    state_buf[4] = states[base + 4];
    hprot_buf[4] = hprots.port6[0][base + 4];
    line_buf[4] = lines.port6[0][base + 4];

    tag_buf[5] = tags.port7[0][base + 5];
    state_buf[5] = states[base + 5];
    hprot_buf[5] = hprots.port7[0][base + 5];
    line_buf[5] = lines.port7[0][base + 5];

    tag_buf[6] = tags.port8[0][base + 6];
    state_buf[6] = states[base + 6];
    hprot_buf[6] = hprots.port8[0][base + 6];
    line_buf[6] = lines.port8[0][base + 6];

    tag_buf[7] = tags.port9[0][base + 7];
    state_buf[7] = states[base + 7];
    hprot_buf[7] = hprots.port9[0][base + 7];
    line_buf[7] = lines.port9[0][base + 7];

#endif
#endif
#endif
}

void l2::tag_lookup(addr_breakdown_t addr_br, bool &tag_hit, l2_way_t &way_hit, bool &empty_way_found,
		    l2_way_t &empty_way)
{
    TAG_LOOKUP;

    tag_hit = false;
    empty_way_found = false;

    read_set(addr_br.set);
    evict_way = evict_ways.port2[0][addr_br.set];

    for (int i = L2_WAYS-1; i >=0; --i) {
	TAG_LOOKUP_LOOP;

	// HLS_BREAK_ARRAY_DEPENDENCY(tags); // TODO are these needed?
	// HLS_BREAK_ARRAY_DEPENDENCY(states);
	// HLS_BREAK_ARRAY_DEPENDENCY(hprots);
	// HLS_BREAK_ARRAY_DEPENDENCY(lines);

	// tag_buf[i]   = tags[base + i];
	// state_buf[i] = states[base + i];
	// hprot_buf[i] = hprots[base + i];
	// line_buf[i] = lines[base + i];

	if (tag_buf[i] == addr_br.tag && state_buf[i] != INVALID) {
	    tag_hit = true;
	    way_hit = i;
	}

	if (state_buf[i] == INVALID) {
	    empty_way_found = true;
	    empty_way = i;
	}
    }

#ifdef L2_DEBUG
    tag_hit_req_dbg.write(tag_hit);
    way_hit_req_dbg.write(way_hit);
    empty_found_req_dbg.write(empty_way_found);
    empty_way_req_dbg.write(empty_way);
#endif
}

void l2::tag_lookup_fwd(line_breakdown_t<l2_tag_t, l2_set_t> line_br, bool &tag_hit, l2_way_t &way_hit)
{
    TAG_LOOKUP;

    read_set(line_br.set);
	tag_hit = false;

    for (int i = L2_WAYS-1; i >=0; --i) {
	TAG_LOOKUP_LOOP;

	// HLS_BREAK_ARRAY_DEPENDENCY(tags); // TODO are these needed?
	// HLS_BREAK_ARRAY_DEPENDENCY(states);
	// HLS_BREAK_ARRAY_DEPENDENCY(hprots);
	// HLS_BREAK_ARRAY_DEPENDENCY(lines);

	// tag_buf[i]   = tags[base + i];
	// state_buf[i] = states[base + i];
	// hprot_buf[i] = hprots[base + i];
	// line_buf[i] = lines[base + i];

	if (tag_buf[i] == line_br.tag && state_buf[i] != INVALID) {
	    way_hit = i;
		tag_hit = true;
	}
    }

#ifdef L2_DEBUG
    way_hit_fwd_dbg.write(way_hit);
#endif
}

void l2::reqs_lookup(line_breakdown_t<l2_tag_t, l2_set_t> line_br, sc_uint<REQS_BITS> &reqs_hit_i)
{
    REQS_LOOKUP;

    bool reqs_hit = false;

    for (unsigned int i = 0; i < N_REQS; ++i) {
	REQS_LOOKUP_LOOP;

	if (reqs[i].tag == line_br.tag && reqs[i].set == line_br.set && reqs[i].state != INVALID) {
	    reqs_hit = true;
	    reqs_hit_i = i;
	}
    }

#ifdef L2_DEBUG
    reqs_hit_req_dbg.write(reqs_hit);
    reqs_hit_i_req_dbg.write(reqs_hit_i);
#endif
    // REQS_LOOKUP_ASSERT;
}

bool l2::reqs_peek_req(l2_set_t set, sc_uint<REQS_BITS> &reqs_i)
{
    REQS_PEEK_REQ;

    set_conflict = false;

    for (unsigned int i = 0; i < N_REQS; ++i) {
	REQS_PEEK_REQ_LOOP;

	if (reqs[i].state == INVALID)
	    reqs_i = i;

	if (reqs[i].set == set && reqs[i].state != INVALID)
	    set_conflict = true;
    }

#ifdef L2_DEBUG
    peek_reqs_i_dbg.write(reqs_i);
#endif

    return set_conflict;
}

void l2::reqs_peek_flush(l2_set_t set, sc_uint<REQS_BITS> &reqs_i)
{
    REQS_PEEK_REQ;

    for (unsigned int i = 0; i < N_REQS; ++i) {
	REQS_PEEK_REQ_LOOP;

	if (reqs[i].state == INVALID)
	    reqs_i = i;
    }

#ifdef L2_DEBUG
    peek_reqs_i_flush_dbg.write(reqs_i);
#endif
}


bool l2::reqs_peek_fwd(line_breakdown_t<l2_tag_t, l2_set_t> line_br,
		       sc_uint<REQS_BITS> &reqs_i, bool &reqs_hit, mix_msg_t coh_msg)
{
    REQS_PEEK_FWD;

    bool fwd_stall_tmp = false;

    reqs_hit = false;
    reqs_i = 0;

    for (unsigned int i = 0; i < N_REQS; ++i) {
	REQS_PEEK_FWD_LOOP;

	if (reqs[i].state != INVALID &&
	    reqs[i].tag == line_br.tag &&
	    reqs[i].set == line_br.set) {

	    reqs_hit = true;
	    reqs_i = i;

	    fwd_stall_tmp = true;

	    if (coh_msg == FWD_PUTACK) {

		fwd_stall_tmp = false;

	    } else if (coh_msg == FWD_INV || coh_msg == FWD_INV_LLC) {

		if (reqs[i].state != ISD)
		    fwd_stall_tmp = false;
	    } else {

		if (reqs[i].state == MIA)
		    fwd_stall_tmp = false;
	    }
	}
    }

    reqs_fwd_stall_i = reqs_i;

#ifdef L2_DEBUG
    peek_reqs_hit_fwd_dbg.write(reqs_hit);
#endif

    return fwd_stall_tmp;
}


void l2::self_invalidate()
{
	for (int i = 0; i < L2_LINES; i++)
	{
        HLS_UNROLL_LOOP(ON, "reset-states");
        states[i] = GPU_I;
	}

	for (int i = 0; i < N_REQS; i++)
	{
        HLS_UNROLL_LOOP(ON, "reset-reqs");
        if (reqs[i].state == GPU_IV) reqs[i].state = GPU_IV;
	}

}

