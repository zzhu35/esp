// Copyright (c) 2011-2019 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#include "l2_denovo.hpp"


void l2_denovo::drain_wb()
{
    // dispatch to reqs, set drain_in_progress
    // when wb wmpty and reqs_buf has no pending ReqO, reset drain_in_progress
    for (int i = 0; i < N_WB; i++)
    {
        bool success = false;
        if (wbs[i].valid && reqs_cnt > 0) {
            dispatch_wb(success, i);
            break;
        }
        wait();

    }

    bool mshr_no_reqo = true;
    for (int i = 0; i < N_REQS; i++)
    {
        HLS_UNROLL_LOOP(ON, "wb drain");
        if (reqs[i].state == DNV_XR || reqs[i].state == DNV_XRV)
        {
            mshr_no_reqo = false;
            break;
        }
    }
    drain_in_progress = !((wbs_cnt == N_WB) && mshr_no_reqo);

}

void l2_denovo::add_wb(bool& success, addr_breakdown_t addr_br, word_t word, l2_way_t way, hprot_t hprot, bool dcs_en, bool use_owner_pred, cache_id_t pred_cid)
{
    // try to add an entry
    // dispatch to reqs if full line
    // reset success if wb full && reqs full

    sc_uint<WB_BITS> evict_wb = wb_evict++;
    for (int i = 0; i < N_WB; i++)
    {
        HLS_UNROLL_LOOP(ON, "add wb");
        if (wbs[i].valid && wbs[i].word_mask == WORD_MASK_ALL)
        {
            evict_wb = i;
        }
    }

    if (wbs_cnt == 0) {
        // try dispatching
        dispatch_wb(success, evict_wb);
        // if mshr refuse dispatching, return unsuccess
        if (!success) return;
    }

    success = true;

    bool hit = false;
    sc_uint<WB_BITS> i = 0;
    peek_wb(hit, i, addr_br);
    if (hit)
    {
        // hit in WB, DCS override
        wbs[i].dcs_en = dcs_en;
        wbs[i].use_owner_pred = use_owner_pred;
        wbs[i].pred_cid = pred_cid;

    }
    else {
        // empty in WB
        wbs[i].valid = true;
        wbs[i].tag = addr_br.tag;
        wbs[i].set = addr_br.set;
        wbs[i].way = way;
        wbs[i].word_mask = 0;
        wbs[i].line = 0;
        wbs[i].hprot = hprot;
        wbs[i].dcs_en = dcs_en;
        wbs[i].use_owner_pred = use_owner_pred;
        wbs[i].pred_cid = pred_cid;
        wbs_cnt--;
    }
    wbs[i].word_mask |= 1 << addr_br.w_off;
    wbs[i].line.range((addr_br.w_off + 1) * BITS_PER_WORD - 1, addr_br.w_off * BITS_PER_WORD) = word;

    // if (wbs[i].word_mask == WORD_MASK_ALL) {
    //     dispatch_wb(hit, i); // just try dispatching, it's ok if fail
    // }

    // for (int i = 0; i < N_WB; i++)
    // {
    //     bool success = false;
    //     if (wbs[i].valid && wbs[i].word_mask == WORD_MASK_ALL && reqs_cnt > 0)
    //     {
    //         // if all valid, dispatch
    //         dispatch_wb(success, i);
    //     }
    //     wait();
    // }

}

void l2_denovo::peek_wb(bool& hit, sc_uint<WB_BITS>& wb_i, addr_breakdown_t addr_br)
{
    HLS_CONSTRAIN_LATENCY(0, HLS_ACHIEVABLE, "l2-peek wb");
    // check wb hit
    for (int i = 0; i < N_WB; i++)
    {
        HLS_UNROLL_LOOP(ON, "peek wb");
        if (wbs[i].valid == false) {
            hit = false;
            wb_i = i;
        }

        if (wbs[i].valid && (wbs[i].tag == addr_br.tag) && (wbs[i].set == addr_br.set))
        {
            hit = true;
            wb_i = i;
            break;
        }

    }

}

void l2_denovo::dispatch_wb(bool& success, sc_uint<WB_BITS> wb_i)
{
    // dispatch one entry to reqs, sets success
    if (wbs[wb_i].valid == false || reqs_cnt == 0) {
        success = false;
        return;
    }
    // peek reqs buf
    sc_uint<REQS_BITS> reqs_i;

    for (unsigned int i = 0; i < N_REQS; ++i) {
        HLS_UNROLL_LOOP(ON, "dispatch wb");

        if (reqs[i].state == DNV_I){
            reqs_i = i;
            success = true;
        }

        wait();
        if (reqs[i].tag == wbs[wb_i].tag && reqs[i].set == wbs[wb_i].set && reqs[i].state != DNV_I){
            // found conflict, cannot dispatch WB
            success = false;
            return;
        }
    }

    // found empty entry in reqs
    addr_breakdown_t addr_br;
    line_addr_t line_addr;
    addr_br.set = wbs[wb_i].set;
    addr_br.tag = wbs[wb_i].tag;
    line_addr = (addr_br.tag << L2_SET_BITS) | (addr_br.set);

    {
        HLS_DEFINE_PROTOCOL();
        // send reqo
        if (!wbs[wb_i].dcs_en) {
            send_req_out(REQ_O, wbs[wb_i].hprot, line_addr, wbs[wb_i].line, wbs[wb_i].word_mask);
            fill_reqs(0, addr_br, 0, wbs[wb_i].way, 0, DNV_XR, wbs[wb_i].hprot, 0, wbs[wb_i].line, wbs[wb_i].word_mask, reqs_i);
        }
        else if (!wbs[wb_i].use_owner_pred) {
            send_req_out(REQ_WTfwd, wbs[wb_i].hprot, line_addr, wbs[wb_i].line, wbs[wb_i].word_mask);
            fill_reqs(0, addr_br, 0, wbs[wb_i].way, 0, DNV_XRV, wbs[wb_i].hprot, 0, wbs[wb_i].line, wbs[wb_i].word_mask, reqs_i);
        }
        else {
            send_fwd_out(FWD_WTfwd, wbs[wb_i].pred_cid, 1, line_addr, wbs[wb_i].line, wbs[wb_i].word_mask);
            fill_reqs(0, addr_br, 0, wbs[wb_i].way, 0, DNV_XRV, wbs[wb_i].hprot, 0, wbs[wb_i].line, wbs[wb_i].word_mask, reqs_i);
        }

        // free WB
        wbs[wb_i].valid = false;
        wbs_cnt++;
        wait();
    }
}


/*
 * Processes
 */

void l2_denovo::ctrl()
{

    bool is_flush_all;
	bool is_sync;
    {
        L2_DENOVO_RESET;

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
        bool do_cpu_req = false;
		bool do_sync = false;

        bool can_get_rsp_in = false;
        bool can_get_req_in = false;
        bool can_get_sync_in = false;
        bool can_get_fwd_in = false;
        bool can_get_flush_in = false;

        sc_uint<L2_SET_BITS+L2_WAY_BITS>  base = 0;

        l2_rsp_in_t rsp_in;
        l2_fwd_in_t fwd_in;
        l2_cpu_req_t cpu_req;

#if (USE_WB)
#ifdef L2_DEBUG
        drain_in_progress_dbg.write(drain_in_progress);
#endif
        if (drain_in_progress) drain_wb();
#endif

        {
            HLS_DEFINE_PROTOCOL("llc-io-check");
            // HLS_CONSTRAIN_LATENCY(0, HLS_ACHIEVABLE, "l2-io-latency");

            can_get_rsp_in = l2_rsp_in.nb_can_get();
            can_get_req_in = ((l2_cpu_req.nb_can_get() && (!drain_in_progress)) || set_conflict) && !evict_stall && (reqs_cnt != 0 || ongoing_atomic); // if drain in progress, block all cpu requests
            can_get_fwd_in = (l2_fwd_in.nb_can_get() && !fwd_stall) || fwd_stall_ended;
            can_get_sync_in = l2_sync.nb_can_get();
            can_get_flush_in = l2_flush.nb_can_get();

            wait();

            if (can_get_sync_in) {
				l2_sync.nb_get(is_sync);
				do_sync = true;
            } else if (can_get_flush_in) {
				l2_flush.nb_get(is_sync);
                do_flush = true;
            } else if (can_get_rsp_in) {
                get_rsp_in(rsp_in);
                do_rsp = true;
            }
			else if (can_get_fwd_in) {
                if (!fwd_stall) {
                    get_fwd_in(fwd_in);
                } else {
                    fwd_in = fwd_in_stalled;
                }
                do_fwd = true;
            } else if (do_ongoing_flush && !drain_in_progress) {
                // if flush has not started, but drain has finished, flush
                if (!flush_complete) flush();
                else if (reqs_cnt == N_REQS)
                {
                    do_ongoing_flush = false;
                    flush_done.write(1);
                    wait();
                    flush_done.write(0);
                }
            } else if (can_get_req_in) { // assuming
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
        drain_in_progress = true;

	} else if (do_flush) {
        drain_in_progress = true;
        do_ongoing_flush = true;
        flush_line = 0;
        flush_complete = false;
	} else if (do_rsp) {

	    // if (ongoing_flush)
	    //     RSP_WHILE_FLUSHING;

	    line_breakdown_t<l2_tag_t, l2_set_t> line_br;
	    sc_uint<REQS_BITS> reqs_hit_i;

	    line_br.l2_line_breakdown(rsp_in.addr);

        base = line_br.set << L2_WAY_BITS;
        read_set(line_br.set);
	    reqs_lookup(line_br, reqs_hit_i);

	    switch (rsp_in.coh_msg) {
        case RSP_O:
        {
            // if done, don't affect current state
            bool bad = false;
            for (int i = 0; i < WORDS_PER_LINE; i++) {
                HLS_UNROLL_LOOP(ON, "watch dog");
                if ((rsp_in.word_mask & 1 << i) && state_buf[reqs[reqs_hit_i].way][i] != DNV_R) bad = true;
            }
#ifdef L2_DEBUG
            if (bad) watch_dog.write(WDOG_1);
#endif

            reqs[reqs_hit_i].word_mask &= ~(rsp_in.word_mask);

            if (reqs[reqs_hit_i].word_mask == 0) {
                reqs[reqs_hit_i].state = DNV_I;
                reqs_cnt++;
            }

        }
        break;
        // respond AMO one word
        case RSP_WTdata:
        {
            line_t line = 0;
            for (int i = 0; i < WORDS_PER_LINE; i++) {
                HLS_UNROLL_LOOP(ON, "rsp_v");
                if (rsp_in.word_mask & 1 << i) {
                    // found a valid bit in response word mask
                    line.range(BITS_PER_WORD, 0) = rsp_in.line.range((i + 1) * BITS_PER_WORD - 1, i * BITS_PER_WORD); // write back new data
                    break;
                }
            }

            {                
                HLS_DEFINE_PROTOCOL("denovo-resp");
                send_rd_rsp(line);
                reqs[reqs_hit_i].state = DNV_I;
                reqs_cnt++;
                //put_reqs(line_br.set, reqs[reqs_hit_i].way, line_br.tag, reqs[reqs_hit_i].line, reqs[reqs_hit_i].hprot, DNV_I, reqs_hit_i);
            }
        }
        break;
	    case RSP_V :
	    {

            for (int i = 0; i < WORDS_PER_LINE; i++) {
                HLS_UNROLL_LOOP(ON, "rsp_v");
                if (rsp_in.word_mask & (1 << i)) {
                    // found a valid bit in response word mask
                    reqs[reqs_hit_i].line.range((i + 1) * BITS_PER_WORD - 1, i * BITS_PER_WORD) = rsp_in.line.range((i + 1) * BITS_PER_WORD - 1, i * BITS_PER_WORD); // write back new data
                    reqs[reqs_hit_i].word_mask |= 1 << i;
                }
            }

            // all words valid now
            if (reqs[reqs_hit_i].word_mask == WORD_MASK_ALL){
                
                HLS_DEFINE_PROTOCOL("denovo-resp");
                // touched_buf[reqs[reqs_hit_i].way][reqs[reqs_hit_i].w_off] = true;
                send_rd_rsp(reqs[reqs_hit_i].line);
                reqs[reqs_hit_i].state = DNV_I;
                reqs_cnt++;
                put_reqs(line_br.set, reqs[reqs_hit_i].way, line_br.tag, reqs[reqs_hit_i].line, reqs[reqs_hit_i].hprot, current_valid_state, reqs_hit_i);
            }
	    }
	    break;
		
        case RSP_NACK :
        {
            if(reqs[reqs_hit_i].state == DNV_IV_DCS){
                HLS_DEFINE_PROTOCOL("send reqV miss pred");
                send_req_out(REQ_V, reqs[reqs_hit_i].hprot, (reqs[reqs_hit_i].tag, reqs[reqs_hit_i].set), 0, ~reqs[reqs_hit_i].word_mask);
                reqs[reqs_hit_i].state = DNV_IV;
            }else{
                reqs[reqs_hit_i].retry++;
                {
                    HLS_DEFINE_PROTOCOL("send retry");
                    send_req_out((reqs[reqs_hit_i].retry < MAX_RETRY) ? REQ_V : REQ_Odata, reqs[reqs_hit_i].hprot, (reqs[reqs_hit_i].tag, reqs[reqs_hit_i].set), 0, ~reqs[reqs_hit_i].word_mask);
                }
            }
        }
        break;

	    default:
		    RSP_DEFAULT;

	    }

	} else if (do_fwd) {
	    addr_breakdown_t addr_br;
	    addr_br.breakdown(fwd_in.addr << OFFSET_BITS);
        fwd_stall_ended = false;
	    reqs_peek_fwd(addr_br);
        base = addr_br.set << L2_WAY_BITS;
        bool tag_hit, word_hit;
        l2_way_t way_hit;
        bool empty_way_found;
        l2_way_t empty_way;
        tag_lookup(addr_br, tag_hit, way_hit, empty_way_found, empty_way, word_hit);
        if (fwd_stall) {
		    SET_CONFLICT;
            fwd_in_stalled = fwd_in;
            // try to handle this fwd_in as much as we can and resolve fwd_stall
            bool success = false;
            switch (fwd_in.coh_msg)
            {
                case FWD_WB_ACK:
                {
                    if (reqs[reqs_fwd_stall_i].state == DNV_RI)
                    {
                        // erase this line when WB complete
                        // no need to call put_reqs here because there is no state change to either SRAM or xxx_buf
                        reqs[reqs_fwd_stall_i].state = DNV_I;
                        reqs_cnt++;
                        success = true;
                    }

                }
                break;
                case FWD_RVK_O:
                {
                    if (reqs[reqs_fwd_stall_i].state == DNV_RI)
                    {
                        // handle DNV_RI - LLC_OV deadlock
                        HLS_DEFINE_PROTOCOL("deadlock-solver-1");
                        send_rsp_out(RSP_RVK_O, 0, false, addr_br.line_addr, reqs[reqs_fwd_stall_i].line, reqs[reqs_fwd_stall_i].word_mask);
                        success = true;
                    }

                }
                break;
                default:
                break;

            }

            // if we are able to serve this fwd_in, resolve fwd_stall
            if (success)
                fwd_stall = false;

	    } else {
            switch (fwd_in.coh_msg) {
                case FWD_REQ_O:
                {
                    word_mask_t nack_mask = 0;
                    word_mask_t ack_mask = 0;
                    // line exists
                    if (tag_hit) {
                        for (int i = 0; i < WORDS_PER_LINE; i++)
                        {
                            HLS_UNROLL_LOOP(ON, "1");
                            if ((fwd_in.word_mask & (1 << i)) && state_buf[way_hit][i] == DNV_R) { // if reqo and we have this word in registered
                                // go to invalid state
                                state_buf[way_hit][i] = DNV_I;
                                // touched_buf[way_hit][i] = false; // enable flush on next sync
                                ack_mask |= 1 << i;
                            }
                            if ((fwd_in.word_mask & (1 << i)) && state_buf[way_hit][i] != DNV_R) { // if reqo and we do not have this word in registered
                                nack_mask |= 1 << i;
                            }
                        }

                        if (nack_mask) {
                            send_rsp_out(RSP_NACK, fwd_in.req_id, true, fwd_in.addr, 0, nack_mask);
                        }

                        if (ack_mask) {
                            send_rsp_out(RSP_O, fwd_in.req_id, true, fwd_in.addr, 0, ack_mask);
                        }
                    }
                    else
                    {
                        send_rsp_out(RSP_NACK, fwd_in.req_id, true, fwd_in.addr, 0, fwd_in.word_mask);
                    }
                    
                }
                break;
                case FWD_REQ_V:
                {
                    word_mask_t nack_mask = 0;
                    word_mask_t ack_mask = 0;
                    // line exists
                    if (tag_hit) {
                        for (int i = 0; i < WORDS_PER_LINE; i++)
                        {
                            HLS_UNROLL_LOOP(ON, "1");
                            if ((fwd_in.word_mask & (1 << i)) && state_buf[way_hit][i] >= current_valid_state) { // if reqv and we have this word in valid or registered
                                ack_mask |= 1 << i;
                            }
                            if ((fwd_in.word_mask & (1 << i)) && state_buf[way_hit][i] < current_valid_state) { // if reqv and we have this word in invalid
                                nack_mask |= 1 << i;
                            }
                        }

                        if (nack_mask) {
                            send_rsp_out(RSP_NACK, fwd_in.req_id, true, fwd_in.addr, 0, nack_mask);
                        }

                        if (ack_mask) {
                            send_rsp_out(RSP_V, fwd_in.req_id, true, fwd_in.addr, line_buf[way_hit], ack_mask);
                        }
                    }
                    else
                    {
                        send_rsp_out(RSP_NACK, fwd_in.req_id, true, fwd_in.addr, 0, fwd_in.word_mask);
                    }
                    
                }
                break;
                case FWD_RVK_O:
                {
                    word_mask_t rsp_mask = 0;
                    if (tag_hit) {
                        for (int i = 0; i < WORDS_PER_LINE; i++)
                        {
                            HLS_UNROLL_LOOP(ON, "1");
                            if ((fwd_in.word_mask & (1 << i)) && state_buf[way_hit][i] == DNV_R) { // if reqo and we have this word in registered
                                rsp_mask |= 1 << i;
                                state_buf[way_hit][i] = DNV_I;
                                // touched_buf[way_hit][i] = false; // enable flush on next sync
                            }
                        }
                        if (rsp_mask) {
                            send_rsp_out(RSP_RVK_O, fwd_in.req_id, false, fwd_in.addr, line_buf[way_hit], rsp_mask);
                        }

                    }
                }
                break;
                case FWD_REQ_S:
                {
                    word_mask_t rsp_mask = 0;
                    if (tag_hit) {
                        for (int i = 0; i < WORDS_PER_LINE; i++)
                        {
                            HLS_UNROLL_LOOP(ON, "1");
                            if ((fwd_in.word_mask & (1 << i)) && state_buf[way_hit][i] == DNV_R) { // if reqo and we have this word in registered
                                rsp_mask |= 1 << i;
                                state_buf[way_hit][i] = current_valid_state;
                            }
                        }
                        if (rsp_mask) {
                            HLS_DEFINE_PROTOCOL("send rsp s");
                            send_rsp_out(RSP_RVK_O, fwd_in.req_id, false, fwd_in.addr, line_buf[way_hit], rsp_mask);
                            wait();
                            send_rsp_out(RSP_S, fwd_in.req_id, true, fwd_in.addr, line_buf[way_hit], rsp_mask);
                        }

                    }

                }
                break;
                case FWD_WTfwd:
                {
                    // @TODO check WB
                    word_mask_t word_mask = 0;
                    if(tag_hit){
                        for (int i = 0; i < WORDS_PER_LINE; i++){
                            HLS_UNROLL_LOOP(ON, "1");
                            if (fwd_in.word_mask & (1 << i)) {
                                if (state_buf[way_hit][i] == DNV_R) {
                                    line_buf[way_hit].range(BITS_PER_WORD*(i+1)-1,BITS_PER_WORD*i) = fwd_in.line.range(BITS_PER_WORD*(i+1)-1,BITS_PER_WORD*i);
                                } else {
                                    word_mask |= 1 << i;
                                    // touched_buf[way_hit][i] = false;
                                }
                            }
                        }
                        lines.port1[0][base + way_hit] = line_buf[way_hit];
                        if (word_mask) {
                            send_req_out(REQ_WT, 1, fwd_in.addr, fwd_in.line, word_mask);
                        }
                    }else{
                        // not found in the cache, send req_wt back to llc
                        send_req_out(REQ_WT, 1, fwd_in.addr, fwd_in.line, fwd_in.word_mask);
                    }
                    send_rsp_out(RSP_O, fwd_in.req_id, true, fwd_in.addr, 0, fwd_in.word_mask);
                }
                break;
                default:
                break;
            }
        }
	} else if (do_cpu_req) { // assuming HPROT cacheable

	    addr_breakdown_t addr_br;
	    sc_uint<REQS_BITS> reqs_empty_i;
        sc_uint<REQS_BITS> reqs_empty_i2;

	    addr_br.breakdown(cpu_req.addr);

	    reqs_peek_req(addr_br.set, reqs_empty_i, reqs_empty_i2);
        base = addr_br.set << L2_WAY_BITS;


        if (set_conflict) {
		SET_CONFLICT;

		cpu_req_conflict = cpu_req;

	    } else {

			bool tag_hit, word_hit;
			l2_way_t way_hit;
			bool empty_way_found;
			l2_way_t empty_way;

			tag_lookup(addr_br, tag_hit, way_hit, empty_way_found, empty_way, word_hit);

            bool wb_hit = false;
            sc_uint<WB_BITS> wb_i = 0;
            peek_wb(wb_hit, wb_i, addr_br);
            bool wb_word_hit = wb_hit && (wbs[wb_i].word_mask & (1 << addr_br.w_off));

            if (cpu_req.amo) {
                coh_msg_t msg;
                switch (cpu_req.amo)
                {
                    case AMO_SWAP :
                        msg = REQ_AMO_SWAP;
                        break;
                    case AMO_ADD :
                        msg = REQ_AMO_ADD;
                        break;
                    case AMO_AND :
                        msg = REQ_AMO_AND;
                        break;
                    case AMO_OR :
                        msg = REQ_AMO_OR;
                        break;
                    case AMO_XOR :
                        msg = REQ_AMO_XOR;
                        break;
                    case AMO_MAX :
                        msg = REQ_AMO_MAX;
                        break;
                    case AMO_MAXU :
                        msg = REQ_AMO_MAXU;
                        break;
                    case AMO_MIN :
                        msg = REQ_AMO_MIN;
                        break;
                    case AMO_MINU :
                        msg = REQ_AMO_MINU;
                        break;

                    default:
                        break;
                }
                line_t line;
                line.range((addr_br.w_off + 1) * BITS_PER_WORD - 1, addr_br.w_off * BITS_PER_WORD) = cpu_req.word;

                if (word_hit && (state_buf[way_hit][addr_br.w_off] == DNV_R || (state_buf[way_hit][addr_br.w_off] == current_valid_state))) {
                    state_buf[way_hit][addr_br.w_off] = DNV_R;

                    calc_amo(line_buf[way_hit], line, msg, 1 << addr_br.w_off);
                    lines.port1[0][base + way_hit] = line_buf[way_hit];
                    line = line >> (BITS_PER_WORD * addr_br.w_off);
                    send_rd_rsp(line);
                }
                else
                {
                    fill_reqs(0, addr_br, 0, 0, 0, DNV_AMO, cpu_req.hprot, 0, 0, 0, reqs_empty_i);
                    send_req_out(msg, cpu_req.hprot, addr_br.line_addr, line, 1 << addr_br.w_off);

                }
            }

            else if (cpu_req.cpu_msg == WRITE)
            {
                l2_way_t way_write;
                if (tag_hit || empty_way_found) way_write = (tag_hit) ? way_hit : empty_way;
                else {
                    // eviction
                    line_addr_t line_addr_evict = (tag_buf[evict_way] << L2_SET_BITS) | (addr_br.set);

                    word_mask_t word_mask = 0;
                    for (int i = 0; i < WORDS_PER_LINE; i ++)
                    {
                        HLS_UNROLL_LOOP(ON, "3");
                        if (state_buf[evict_way][i] == DNV_R)
                        {
                            word_mask |= 1 << i;
                        }
                    }

                    if (word_mask) {
                        addr_breakdown_t evict_addr_br = addr_br;
                        evict_addr_br.tag = tag_buf[evict_way];
#if (USE_WB)
                        // check in wb for eviction conflict
                        // if in WB, move into MSHR, set_conflict
                        bool hit_evict = false;
                        sc_uint<WB_BITS> wb_i_evict = 0;
                        peek_wb(hit_evict, wb_i_evict, evict_addr_br);
                        if (hit_evict){
                            // we don't have ownership for the eviction yet
                            // attempt to dispatch
                            dispatch_wb(hit_evict, wb_i_evict); // reusing hit for success here...
                            set_conflict = true;
                            cpu_req_conflict = cpu_req;
                            // continue control loop
#ifdef L2_DEBUG
                            set_conflict_dbg.write(set_conflict);
#endif
                            wait();
                            continue;
                        }

#endif
                        HLS_DEFINE_PROTOCOL("spandex_dual_req");
                        send_req_out(REQ_WB, hprot_buf[evict_way], line_addr_evict, line_buf[evict_way], word_mask);
                        wait();
                        fill_reqs(0, evict_addr_br, 0, 0, 0, DNV_RI, 0, 0, line_buf[evict_way], word_mask, reqs_empty_i);
                    }

                    for (int i = 0; i < WORDS_PER_LINE; i ++)
                    {
                        HLS_UNROLL_LOOP(ON, "4");
                        state_buf[evict_way][i] = DNV_I;
                        // touched_buf[evict_way][i] = false;
                    }
                    send_inval(line_addr_evict);
                    way_write = evict_way;
                }
                write_word(line_buf[way_write], cpu_req.word, addr_br.w_off, addr_br.b_off, cpu_req.hsize);
                lines.port1[0][base + way_write] = line_buf[way_write];
                hprots.port1[0][base + way_write] = cpu_req.hprot;
                tags.port1[0][base + way_write] = addr_br.tag;
                evict_ways.port1[0][addr_br.set] = way_write + 1;
                if(cpu_req.dcs_en){
                        switch (cpu_req.dcs){
                            case DCS_ReqWTfwd:
                            {
                                if ((!word_hit) || (state_buf[way_write][addr_br.w_off] != DNV_R)) { // if no hit or not in registered
#if (USE_WB)
                                    bool success = false;
                                    add_wb(success, addr_br, cpu_req.word, way_write, cpu_req.hprot, cpu_req.dcs_en, cpu_req.use_owner_pred, cpu_req.pred_cid);
                                    if (!success)
                                    {
                                        // if wb refused attempted insertion, raise set_conflict
                                        set_conflict = true;
                                        cpu_req_conflict = cpu_req;
                                    } else {
                                        state_buf[way_write][addr_br.w_off] = current_valid_state;
                                        // touched_buf[way_write][addr_br.w_off] = false;
                                    }
#else
                                    HLS_DEFINE_PROTOCOL("req_wtfwd out");
                                    if(cpu_req.use_owner_pred){
                                        send_fwd_out(FWD_WTfwd, cpu_req.pred_cid, 1, addr_br.line_addr, line_buf[way_write], 1 << addr_br.w_off);
                                    }else{
                                        send_req_out(REQ_WTfwd, cpu_req.hprot, addr_br.line_addr, line_buf[way_write], 1 << addr_br.w_off);
                                    }
                                    wait();
                                    state_buf[way_write][addr_br.w_off] = current_valid_state;
                                    // touched_buf[way_write][addr_br.w_off] = false;
#endif

                                }
                            }
                            break;
                            default:
                            break;
                        }
                }else{
                    if ((!word_hit) || (state_buf[way_write][addr_br.w_off] != DNV_R)) { // if no hit or not in registered
                        HLS_DEFINE_PROTOCOL("spandex_dual_req");
#if (USE_WB)
                        bool success = false;
                        add_wb(success, addr_br, cpu_req.word, way_write, cpu_req.hprot, cpu_req.dcs_en, cpu_req.use_owner_pred, cpu_req.pred_cid);
                        if (!success)
                        {
                            // if wb refused attempted insertion, raise set_conflict
                            set_conflict = true;
                            cpu_req_conflict = cpu_req;
                        } else {
                            state_buf[way_write][addr_br.w_off] = DNV_R; // directly go to registered
                        }

#else
                        send_req_out(REQ_O, cpu_req.hprot, addr_br.line_addr, line_buf[way_write], 1 << addr_br.w_off); // send registration
                        wait();
                        state_buf[way_write][addr_br.w_off] = DNV_R; // directly go to registered
#endif
                    }
                }

            }
            // else if read
            // assuming line granularity read MESI style
			else if (tag_hit) {
                HIT_READ;
                word_mask_t word_mask = 0;
                for (int i = 0; i < WORDS_PER_LINE; i ++)
                {
                    HLS_UNROLL_LOOP(ON, "2");
                    if (state_buf[way_hit][i] < current_valid_state)
                    {
                        word_mask |= 1 << i;
                    }
                }
                
                if (word_mask) // some words are present but not whole line. send reqv
                {
                    if(cpu_req.dcs_en && cpu_req.use_owner_pred){
                        send_fwd_out(FWD_REQ_V, cpu_req.pred_cid, 1, addr_br.line_addr, 0, word_mask);
				        fill_reqs(cpu_req.cpu_msg, addr_br, addr_br.tag, way_hit, cpu_req.hsize, DNV_IV_DCS, cpu_req.hprot, cpu_req.word, line_buf[way_hit], ~word_mask, reqs_empty_i);
                    }else{
                        send_req_out(REQ_V, cpu_req.hprot, addr_br.line_addr, 0, word_mask);
				        fill_reqs(cpu_req.cpu_msg, addr_br, addr_br.tag, way_hit, cpu_req.hsize, DNV_IV, cpu_req.hprot, cpu_req.word, line_buf[way_hit], ~word_mask, reqs_empty_i);
                    }
                }
                else {
                    // touched_buf[way_hit][addr_br.w_off] = true;
                    send_rd_rsp(line_buf[way_hit]);
                }
                

			}
            else if (empty_way_found) {

                if(cpu_req.dcs_en && cpu_req.use_owner_pred){
                    send_fwd_out(FWD_REQ_V, cpu_req.pred_cid, 1, addr_br.line_addr, 0, WORD_MASK_ALL);
				    fill_reqs(cpu_req.cpu_msg, addr_br, addr_br.tag, empty_way, cpu_req.hsize, DNV_IV_DCS, cpu_req.hprot, cpu_req.word, line_buf[empty_way], 0, reqs_empty_i);
                }else{
				    send_req_out(REQ_V, cpu_req.hprot, addr_br.line_addr, 0, WORD_MASK_ALL);
				    fill_reqs(cpu_req.cpu_msg, addr_br, addr_br.tag, empty_way, cpu_req.hsize, DNV_IV, cpu_req.hprot, cpu_req.word, line_buf[empty_way], 0, reqs_empty_i);
                }
			} else {
                // eviction
				line_addr_t line_addr_evict = (tag_buf[evict_way] << L2_SET_BITS) | (addr_br.set);
                addr_breakdown_t evict_addr_br = addr_br;

#if (USE_WB)
                // check in wb for eviction conflict
                // if in WB, move into MSHR, set_conflict
                evict_addr_br.tag = tag_buf[evict_way];
                bool hit_evict = false;
                sc_uint<WB_BITS> wb_i_evict = 0;
                peek_wb(hit_evict, wb_i_evict, evict_addr_br);
                if (hit_evict){
                    // we don't have ownership for the eviction yet
                    // attempt to dispatch
                    dispatch_wb(hit_evict, wb_i_evict); // reusing hit for success here...
                    // give up and raise set_conflicct
                    set_conflict = true;
                    cpu_req_conflict = cpu_req;
                } else {
#endif

                    word_mask_t word_mask = 0;
                    for (int i = 0; i < WORDS_PER_LINE; i ++)
                    {
                        HLS_UNROLL_LOOP(ON, "3");
                        if (state_buf[evict_way][i] == DNV_R)
                        {
                            word_mask |= 1 << i;
                        }
                    }

                    for (int i = 0; i < WORDS_PER_LINE; i ++)
                    {
                        HLS_UNROLL_LOOP(ON, "4");
                        state_buf[evict_way][i] = DNV_I;
                        // touched_buf[evict_way][i] = false;
                        // Update it now b/c set_conflict could be true
                        // states[base + evict_way][i] = DNV_I;
                        // touched[base + evict_way][i] = false;
                    }
                    {
                        sc_uint<DNV_STABLE_STATE_BITS * WORDS_PER_LINE> line_state = 0;
                        for (int j = 0; j < WORDS_PER_LINE; j++) {
                            HLS_UNROLL_LOOP(ON, "5");
                            line_state |= DNV_I << (j * DNV_STABLE_STATE_BITS);
                        }
                        states.port1[0][base + evict_way] = line_state;
                        // touched.port1[0][base + evict_way] = 0;
                    }
                    send_inval(line_addr_evict);



                    if(reqs_cnt >= 2 || word_mask == 0){

                        if(cpu_req.dcs_en && cpu_req.use_owner_pred){
                            HLS_DEFINE_PROTOCOL("read miss send fwd req v");
                            send_fwd_out(FWD_REQ_V, cpu_req.pred_cid, 1, addr_br.line_addr, 0, WORD_MASK_ALL);
                            fill_reqs(cpu_req.cpu_msg, addr_br, addr_br.tag, evict_way, cpu_req.hsize, DNV_IV_DCS, cpu_req.hprot, cpu_req.word, line_buf[evict_way], 0, reqs_empty_i);
                        }else{
                            HLS_DEFINE_PROTOCOL("read miss send req v");
                            send_req_out(REQ_V, cpu_req.hprot, addr_br.line_addr, 0, WORD_MASK_ALL);
                            fill_reqs(cpu_req.cpu_msg, addr_br, addr_br.tag, evict_way, cpu_req.hsize, DNV_IV, cpu_req.hprot, cpu_req.word, line_buf[evict_way], 0, reqs_empty_i);
                            wait();
                        }

                        if (word_mask)
                        {
                            HLS_DEFINE_PROTOCOL("send wb after reqv");
                            send_req_out(REQ_WB, hprot_buf[evict_way], line_addr_evict, line_buf[evict_way], word_mask);
                            fill_reqs(0, evict_addr_br, 0, 0, 0, DNV_RI, 0, 0, line_buf[evict_way], word_mask, reqs_empty_i2);
                        }

                    }else{

                        if (word_mask)
                        {
                            HLS_DEFINE_PROTOCOL("spandex_dual_req");
                            send_req_out(REQ_WB, hprot_buf[evict_way], line_addr_evict, line_buf[evict_way], word_mask);
                            fill_reqs(0, evict_addr_br, 0, 0, 0, DNV_RI, 0, 0, line_buf[evict_way], word_mask, reqs_empty_i);
                            //wait();
                        }

                        // make it set_conflict so that next ctrl loop there is an empty way
                        set_conflict = true;
                        cpu_req_conflict = cpu_req;
                    }

#if (USE_WB)
                }
#endif
            }      
	    }
	}
    if ((do_cpu_req && !set_conflict) || (do_fwd && !fwd_stall) || do_rsp){
        for (int i = 0; i < L2_WAYS; i++) {
            sc_uint<DNV_STABLE_STATE_BITS * WORDS_PER_LINE> line_state = 0;
            // word_mask_t line_touched = 0;
            for (int j = 0; j < WORDS_PER_LINE; j++) {
                HLS_UNROLL_LOOP(ON, "5");
                line_state |= state_buf[i][j] << (j * DNV_STABLE_STATE_BITS);
                // line_touched |= touched_buf[i][j] << (j);
            }
            states.port1[0][base + i] = line_state;
            // touched.port1[0][base + i] = line_touched;
        }
    }

#ifdef L2_DEBUG
	// update debug vectors
	asserts.write(asserts_tmp);
	bookmark.write(bookmark_tmp);

	reqs_cnt_dbg.write(reqs_cnt);
    current_valid_state_dbg.write(current_valid_state);
    wbs_cnt_dbg.write(wbs_cnt);
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

    for (int i = 0; i < N_WB; i++) {
        HLS_UNROLL_LOOP(ON, "wb dbg");
        wbs_dbg[i] = wbs[i];
    }

	for (int i = 0; i < L2_WAYS; i++) {
	    BUFS_DBG;
	    tag_buf_dbg[i] = tag_buf[i];
        for (int j = 0; j < WORDS_PER_LINE; j++) {
	        HLS_UNROLL_LOOP(ON, "5");
            state_buf_dbg[i][j] = state_buf[i][j];
        }
	}

    // for (int i = 0; i < L2_LINES; i++) {
    //     HLS_UNROLL_LOOP(ON, "states_dbg_i");
    //     for (int j = 0; j < WORDS_PER_LINE; j++) {
    //         HLS_UNROLL_LOOP(ON, "states_dbg_j");
    //         states_dbg[i][j] = states[i][j];

    //     }

    // }

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

inline void l2_denovo::reset_io()
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
    states.port1.reset();
    // touched.port1.reset();

    tags.port2.reset();
    hprots.port2.reset();
    lines.port2.reset();
    states.port2.reset();
    // touched.port2.reset();

#if (L2_WAYS >= 2)

    CACHE_REPORT_INFO("REPORT L2_WAYS");
    CACHE_REPORT_VAR(0, "L2_WAYS", L2_WAYS);

    tags.port3.reset();
    hprots.port3.reset();
    lines.port3.reset();
    states.port3.reset();
    // touched.port3.reset();

#if (L2_WAYS >= 4)

    tags.port4.reset();
    hprots.port4.reset();
    lines.port4.reset();
    states.port4.reset();
    // touched.port4.reset();

    tags.port5.reset();
    hprots.port5.reset();
    lines.port5.reset();
    states.port5.reset();
    // touched.port5.reset();

#if (L2_WAYS >= 8)

    tags.port6.reset();
    hprots.port6.reset();
    lines.port6.reset();
    states.port6.reset();
    // touched.port6.reset();

    tags.port7.reset();
    hprots.port7.reset();
    lines.port7.reset();
    states.port7.reset();
    // touched.port7.reset();

    tags.port8.reset();
    hprots.port8.reset();
    lines.port8.reset();
    states.port8.reset();
    // touched.port8.reset();

    tags.port9.reset();
    hprots.port9.reset();
    lines.port9.reset();
    states.port9.reset();
    // touched.port9.reset();

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
    current_valid_state_dbg.write(0);
    wbs_cnt_dbg.write(0);
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
    watch_dog.write(0);
    flush_line_dbg.write(0);
    drain_in_progress_dbg.write(0);

    // for (int i = 0; i < N_REQS; i++) {
    // 	REQS_DBGPUT;
    // 	reqs_dbg[i] = 0;
    // }

    for (int i = 0; i < L2_WAYS; i++) {
    	BUFS_DBG;
    	tag_buf_dbg[i] = 0;
        // for (int j = 0; j < WORDS_PER_LINE; j++) {
        //     HLS_UNROLL_LOOP(ON, "6");
        //     states[i][j] = DNV_I;
        //     touched[i][j] = false;
        // }
    }

    evict_way_dbg.write(0);

    /* Reset variables */
    asserts_tmp = 0;
    bookmark_tmp = 0;
#endif

    reqs_cnt = N_REQS;
    wbs_cnt = N_WB;
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
    drain_in_progress = false;
    do_ongoing_flush = false;
    wb_evict = 0;
    flush_set = 0;
    flush_way = 0;
    current_valid_state = 1;

	// for (int i = 0; i < L2_LINES; i++)
	// {
    //     HLS_UNROLL_LOOP(ON, "reset-states");
    //     for (int j = 0; j < WORDS_PER_LINE; j++) {
    //         HLS_UNROLL_LOOP(ON, "7");
    //         states[i][j] = DNV_I;
    //     }
	// }

}


/* Functions to receive input messages */

void l2_denovo::get_cpu_req(l2_cpu_req_t &cpu_req)
{
    GET_CPU_REQ;

    l2_cpu_req.nb_get(cpu_req);
}

void l2_denovo::get_fwd_in(l2_fwd_in_t &fwd_in)
{
    L2_GET_FWD_IN;


    l2_fwd_in.nb_get(fwd_in);
}

void l2_denovo::get_rsp_in(l2_rsp_in_t &rsp_in)
{
    L2_GET_RSP_IN;

    l2_rsp_in.nb_get(rsp_in);

}

bool l2_denovo::get_flush()
{
    GET_FLUSH;

    bool flush_tmp = false;

    // is_flush_all == 0 -> flush data, not instructions
    // is_flush_all == 1 -> flush data and instructions
    l2_flush.nb_get(flush_tmp);

    return flush_tmp;
}

/* Functions to send output messages */

void l2_denovo::send_rd_rsp(line_t line)
{
    SEND_RD_RSP;

    l2_rd_rsp_t rd_rsp;

    rd_rsp.line = line;

    l2_rd_rsp.put(rd_rsp);
}

void l2_denovo::send_inval(line_addr_t addr_inval)
{
    SEND_INVAL;

    l2_inval.put(addr_inval);
}

void l2_denovo::send_req_out(coh_msg_t coh_msg, hprot_t hprot, line_addr_t line_addr, line_t line, word_mask_t word_mask)
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

void l2_denovo::send_rsp_out(coh_msg_t coh_msg, cache_id_t req_id, bool to_req, line_addr_t line_addr, line_t line, word_mask_t word_mask)
{
    // SEND_RSP_OUT;

    l2_rsp_out_t rsp_out;

    rsp_out.coh_msg = coh_msg;
    rsp_out.req_id  = req_id;
    rsp_out.to_req  = to_req;
    rsp_out.addr    = line_addr;
    rsp_out.line    = line;
	rsp_out.word_mask = word_mask;

    while (!l2_rsp_out.nb_can_put()) wait();

    l2_rsp_out.nb_put(rsp_out);
}

void l2_denovo::send_fwd_out(coh_msg_t coh_msg, cache_id_t dst_id, bool to_dst, line_addr_t line_addr, line_t line, word_mask_t word_mask)
{
    // SEND_FWD_OUT;

    l2_fwd_out_t fwd_out;

    fwd_out.coh_msg = coh_msg;
    fwd_out.req_id  = dst_id;
    fwd_out.to_req  = to_dst;
    fwd_out.addr    = line_addr;
    fwd_out.line    = line;
	fwd_out.word_mask = word_mask;

    while (!l2_fwd_out.nb_can_put()) wait();

    l2_fwd_out.nb_put(fwd_out);
}

#ifdef STATS_ENABLE
void l2_denovo::send_stats(bool stats)
{
    SEND_STATS;

    l2_stats.put(stats);
}
#endif


/* Functions to move around buffered lines */

void l2_denovo::fill_reqs(cpu_msg_t cpu_msg, addr_breakdown_t addr_br, l2_tag_t tag_estall, l2_way_t way_hit,
		   hsize_t hsize, unstable_state_t state, hprot_t hprot, word_t word, line_t line, word_mask_t word_mask,
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
    reqs[reqs_i].word_mask   = word_mask;
    reqs[reqs_i].retry       = 0;
    reqs_word_mask_in[reqs_i] = ~word_mask;

    reqs_cnt--;
}

void l2_denovo::put_reqs(l2_set_t set, l2_way_t way, l2_tag_t tag, line_t line, hprot_t hprot, dnv_state_t state,
		  sc_uint<REQS_BITS> reqs_i)
{
    PUT_REQS;

    sc_uint<L2_SET_BITS+L2_WAY_BITS> base = set << L2_WAY_BITS;

    lines.port1[0][base + way]  = line;
    hprots.port1[0][base + way] = hprot;
    for (int i = 0; i < WORDS_PER_LINE; i++) {
        HLS_UNROLL_LOOP(ON, "put reqs");
        if (reqs_word_mask_in[reqs_i] & 1 << i) // @TODO be careful some reqs buffer might not have word_mask set
            state_buf[way][i] = state;
    }
    tags.port1[0][base + way]   = tag;

    // if necessary end the forward messages stall
    if (fwd_stall && reqs_fwd_stall_i == reqs_i) {
        fwd_stall_ended = true;
    }
}

/* Functions to search for cache lines either in memory or buffered */
inline void l2_denovo::read_set(l2_set_t set)
{
    //Manual unroll because these are explicit memories, see commented code
    // below for implicit memories usage
    sc_uint<L2_SET_BITS+L2_WAY_BITS> base = set << L2_WAY_BITS;

    sc_uint<DNV_STABLE_STATE_BITS * WORDS_PER_LINE> line_state;
    // word_mask_t line_touched;

    tag_buf[0] = tags.port2[0][base + 0];
    line_state = states.port2[0][base + 0];
    // line_touched = touched.port2[0][base + 0];
    for (int i = 0; i < WORDS_PER_LINE; i++) {
        HLS_UNROLL_LOOP(ON, "8");
        state_buf[0][i] = line_state >> (i * DNV_STABLE_STATE_BITS);
        // touched_buf[0][i] = (line_touched >> i) & 1;

    }
    hprot_buf[0] = hprots.port2[0][base + 0];
    line_buf[0] = lines.port2[0][base + 0];

#if (L2_WAYS >= 2)

    tag_buf[1] = tags.port3[0][base + 1];
    line_state = states.port3[0][base + 1];
    // line_touched = touched.port3[0][base + 1];
    for (int i = 0; i < WORDS_PER_LINE; i++) {
        HLS_UNROLL_LOOP(ON, "9");
        state_buf[1][i] = line_state >> (i * DNV_STABLE_STATE_BITS);
        // touched_buf[1][i] = (line_touched >> i) & 1;
    }
    hprot_buf[1] = hprots.port3[0][base + 1];
    line_buf[1] = lines.port3[0][base + 1];

#if (L2_WAYS >= 4)

    tag_buf[2] = tags.port4[0][base + 2];
    line_state = states.port4[0][base + 2];
    // line_touched = touched.port4[0][base + 2];
    for (int i = 0; i < WORDS_PER_LINE; i++) {
        HLS_UNROLL_LOOP(ON, "10");
        state_buf[2][i] = line_state >> (i * DNV_STABLE_STATE_BITS);
        // touched_buf[2][i] = (line_touched >> i) & 1;
    }
    hprot_buf[2] = hprots.port4[0][base + 2];
    line_buf[2] = lines.port4[0][base + 2];

    tag_buf[3] = tags.port5[0][base + 3];
    line_state = states.port5[0][base + 3];
    // line_touched = touched.port5[0][base + 3];
    for (int i = 0; i < WORDS_PER_LINE; i++) {
        HLS_UNROLL_LOOP(ON, "11");
        state_buf[3][i] = line_state >> (i * DNV_STABLE_STATE_BITS);
        // touched_buf[3][i] = (line_touched >> i) & 1;
    }
    hprot_buf[3] = hprots.port5[0][base + 3];
    line_buf[3] = lines.port5[0][base + 3];

#if (L2_WAYS >= 8)

    tag_buf[4] = tags.port6[0][base + 4];
    line_state = states.port6[0][base + 4];
    // line_touched = touched.port6[0][base + 4];
    for (int i = 0; i < WORDS_PER_LINE; i++) {
        HLS_UNROLL_LOOP(ON, "12");
        state_buf[4][i] = line_state >> (i * DNV_STABLE_STATE_BITS);
        // touched_buf[4][i] = (line_touched >> i) & 1;
    }
    hprot_buf[4] = hprots.port6[0][base + 4];
    line_buf[4] = lines.port6[0][base + 4];

    tag_buf[5] = tags.port7[0][base + 5];
    line_state = states.port7[0][base + 5];
    // line_touched = touched.port7[0][base + 5];
    for (int i = 0; i < WORDS_PER_LINE; i++) {
        HLS_UNROLL_LOOP(ON, "13");
        state_buf[5][i] = line_state >> (i * DNV_STABLE_STATE_BITS);
        // touched_buf[5][i] = (line_touched >> i) & 1;
    }
    hprot_buf[5] = hprots.port7[0][base + 5];
    line_buf[5] = lines.port7[0][base + 5];

    tag_buf[6] = tags.port8[0][base + 6];
    line_state = states.port8[0][base + 6];
    // line_touched = touched.port8[0][base + 6];
    for (int i = 0; i < WORDS_PER_LINE; i++) {
        HLS_UNROLL_LOOP(ON, "14");
        state_buf[6][i] = line_state >> (i * DNV_STABLE_STATE_BITS);
        // touched_buf[6][i] = (line_touched >> i) & 1;
    }
    hprot_buf[6] = hprots.port8[0][base + 6];
    line_buf[6] = lines.port8[0][base + 6];

    tag_buf[7] = tags.port9[0][base + 7];
    line_state = states.port9[0][base + 7];
    // line_touched = touched.port9[0][base + 7];
    for (int i = 0; i < WORDS_PER_LINE; i++) {
        HLS_UNROLL_LOOP(ON, "15");
        state_buf[7][i] = line_state >> (i * DNV_STABLE_STATE_BITS);
        // touched_buf[7][i] = (line_touched >> i) & 1;
    }
    hprot_buf[7] = hprots.port9[0][base + 7];
    line_buf[7] = lines.port9[0][base + 7];

#endif
#endif
#endif
}

void l2_denovo::tag_lookup(addr_breakdown_t addr_br, bool &tag_hit, l2_way_t &way_hit, bool &empty_way_found, l2_way_t &empty_way, bool &word_hit)
{
    TAG_LOOKUP;

    tag_hit = false;
    word_hit = false;
    empty_way_found = false;

    read_set(addr_br.set);
    // wait();
    evict_way = evict_ways.port2[0][addr_br.set];

    for (int i = L2_WAYS-1; i >=0; --i) {
	TAG_LOOKUP_LOOP;

	// tag_buf[i]   = tags[base + i];
	// state_buf[i] = states[base + i];
	// hprot_buf[i] = hprots[base + i];
	// line_buf[i] = lines[base + i];

    bool line_present = false;
    for (int j = 0; j < WORDS_PER_LINE; j++)
    {
        HLS_UNROLL_LOOP(ON, "16");
        if (state_buf[i][j] >= current_valid_state) {
            line_present = true;
        }
    }

	if (tag_buf[i] == addr_br.tag && line_present) {
	    tag_hit = true;
	    way_hit = i;
	}

	if (tag_buf[i] == addr_br.tag && line_present && state_buf[i][addr_br.w_off] >= current_valid_state) {
	    word_hit = true;
	}

	if (!line_present) {
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


void l2_denovo::reqs_lookup(line_breakdown_t<l2_tag_t, l2_set_t> line_br, sc_uint<REQS_BITS> &reqs_hit_i)
{
    REQS_LOOKUP;

    bool reqs_hit = false;

    for (unsigned int i = 0; i < N_REQS; ++i) {
	REQS_LOOKUP_LOOP;

	if (reqs[i].tag == line_br.tag && reqs[i].set == line_br.set && reqs[i].state != DNV_I) {
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

void l2_denovo::reqs_peek_req(l2_set_t set, sc_uint<REQS_BITS> &reqs_i, sc_uint<REQS_BITS> &reqs_i2)
{
    REQS_PEEK_REQ;

    set_conflict = reqs_cnt == 0; // if no empty reqs left, cannot process CPU request

    bool find_one_empty = false;

    for (unsigned int i = 0; i < N_REQS; ++i) {
        REQS_PEEK_REQ_LOOP;

        if (reqs[i].state == DNV_I)
            reqs_i = i;

        if (reqs[i].state == DNV_I && find_one_empty == false){
            reqs_i2 = i;
            find_one_empty = true;
        }

        if (reqs[i].set == set && reqs[i].state != DNV_I){
            set_conflict = true;
        }
    }

#ifdef L2_DEBUG
    peek_reqs_i_dbg.write(reqs_i);
#endif
}

void l2_denovo::reqs_peek_flush(l2_set_t set, sc_uint<REQS_BITS> &reqs_i)
{
    REQS_PEEK_REQ;

    for (unsigned int i = 0; i < N_REQS; ++i) {
        REQS_PEEK_REQ_LOOP;

        if (reqs[i].state == DNV_I)
            reqs_i = i;
    }

#ifdef L2_DEBUG
    peek_reqs_i_flush_dbg.write(reqs_i);
#endif
}


void l2_denovo::reqs_peek_fwd(addr_breakdown_t addr_br)
{
    REQS_PEEK_REQ;

    fwd_stall = false;

    for (unsigned int i = 0; i < N_REQS; ++i) {
        REQS_PEEK_REQ_LOOP;

        if (reqs[i].tag == addr_br.tag && reqs[i].set == addr_br.set && reqs[i].state != DNV_I){
            fwd_stall = true;
            reqs_fwd_stall_i = i;
        }
    }
}


void l2_denovo::self_invalidate()
{
    if(current_valid_state != DNV_MAX_V){
        current_valid_state++;
    }else{
        for (int i = 0; i < L2_LINES; i++){
            HLS_UNROLL_LOOP(OFF, "self invalidate");
            sc_uint<DNV_STABLE_STATE_BITS * WORDS_PER_LINE> line_state, line_state_in;
            {
                HLS_DEFINE_PROTOCOL("self invalidate read");
                wait();
                line_state = states.port2[0][i];
                wait();
            }

            dnv_state_t state_tmp;
            line_state_in = 0;
            for (int j = 0; j < WORDS_PER_LINE; j++){
                HLS_UNROLL_LOOP(ON, "8");
                state_tmp = line_state >> (j * DNV_STABLE_STATE_BITS);
                if(state_tmp != DNV_R){
                    state_tmp = DNV_I;
                }
                line_state_in |= state_tmp << (j * DNV_STABLE_STATE_BITS);
            }

            {
                HLS_DEFINE_PROTOCOL("self invalidate write");
                wait();
                states.port1[0][i] = line_state_in;
                wait();
            }
        }
        current_valid_state = 1;
    }
}

void l2_denovo::flush()
{
    sc_uint<DNV_STABLE_STATE_BITS * WORDS_PER_LINE> line_state;
    bool success = false;
    line_t line_data;
    hprot_t line_hprot;
    l2_tag_t line_tag;
    {
        HLS_DEFINE_PROTOCOL("flush read");
        wait();
        line_data = lines.port2[0][flush_line];
        line_hprot = hprots.port2[0][flush_line];
        line_tag = tags.port2[0][flush_line];
        line_state = states.port2[0][flush_line];
        wait();
    }

    line_addr_t flush_addr = (line_tag << L2_SET_BITS) | (flush_line >> L2_WAY_BITS);
    word_mask_t flush_owned = 0;
    addr_breakdown_t flush_addr_br;

    flush_addr_br.breakdown(flush_addr << OFFSET_BITS);

    for (unsigned int i = 0; i < WORDS_PER_LINE; i++)
    {
        HLS_UNROLL_LOOP(ON, "flush");
        if ((line_state >> (i * DNV_STABLE_STATE_BITS)) & ((1 << DNV_STABLE_STATE_BITS) - 1) == DNV_R)
        {
            flush_owned |= 1 << i;
        }
    }

    if (flush_owned == 0) 
    {
        // do nothing
        success = true;
    }
    else
    {
        // send req_wb
        sc_uint<REQS_BITS> reqs_i;
        for (unsigned int i = 0; i < N_REQS; ++i) {
            HLS_UNROLL_LOOP(ON, "flush");

            if (reqs[i].state == DNV_I){
                reqs_i = i;
            }
            if (reqs[i].tag == line_tag && reqs[i].set == flush_line >> L2_WAY_BITS && reqs[i].state != DNV_I){
                // found conflict, cannot dispatch WB
                success = false;
                return;
            }
        }
        {
            HLS_DEFINE_PROTOCOL("flush req");
            send_req_out(REQ_WB, line_hprot, flush_addr, line_data, flush_owned);
            fill_reqs(0, flush_addr_br, 0, 0, 0, DNV_RI, 0, 0, line_data, flush_owned, reqs_i);
            success = true;
        }
    }
    

    if (success) {
        HLS_DEFINE_PROTOCOL("flush write");
        wait();
        states.port1[0][flush_line] = 0;
        wait();
        flush_line++;
    }

    if (flush_line == L2_LINES - 1)
    {
        current_valid_state = 1;
        flush_complete = true;
    }
#ifdef L2_DEBUG
    flush_line_dbg.write(flush_line);
#endif
}
