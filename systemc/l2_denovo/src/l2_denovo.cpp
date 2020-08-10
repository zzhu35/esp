// Copyright (c) 2011-2019 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#include "l2_denovo.hpp"

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
        bool do_ongoing_flush = false;
        bool do_cpu_req = false;
		bool do_sync = false;

        bool can_get_rsp_in = false;
        bool can_get_req_in = false;

        sc_uint<L2_SET_BITS+L2_WAY_BITS>  base = 0;

        l2_rsp_in_t rsp_in;
        l2_fwd_in_t fwd_in;
        l2_cpu_req_t cpu_req;

        {
            HLS_DEFINE_PROTOCOL("llc-io-check");

            can_get_rsp_in = l2_rsp_in.nb_can_get();
            can_get_req_in = l2_cpu_req.nb_can_get();

            wait();

			if (l2_sync.nb_can_get()) {
				l2_sync.nb_get(is_sync);
				do_sync = true;
            } else if (l2_flush.nb_can_get() && reqs_cnt == N_REQS) {
                is_flush_all = get_flush();
                //ongoing_flush = true;
                //do_flush = true;
			} else if (can_get_rsp_in) {
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
            } else if ((can_get_req_in || set_conflict) &&
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

        base = line_br.set << L2_WAY_BITS;
        read_set(line_br.set);
	    reqs_lookup(line_br, reqs_hit_i);

	    switch (rsp_in.coh_msg) {

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
                touched_buf[reqs[reqs_hit_i].way][reqs[reqs_hit_i].w_off] = true;
                send_rd_rsp(reqs[reqs_hit_i].line);
                reqs[reqs_hit_i].state = DNV_I;
                reqs_cnt++;
                put_reqs(line_br.set, reqs[reqs_hit_i].way, line_br.tag, reqs[reqs_hit_i].line, reqs[reqs_hit_i].hprot, DNV_V, reqs_hit_i);
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
                                // go to valid state
                                state_buf[way_hit][i] = DNV_V;
                                touched_buf[way_hit][i] = false; // enable flush on next sync
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
                            if ((fwd_in.word_mask & (1 << i)) && state_buf[way_hit][i] != DNV_I) { // if reqv and we have this word in registered
                                ack_mask |= 1 << i;
                            }
                            if ((fwd_in.word_mask & (1 << i)) && state_buf[way_hit][i] == DNV_I) { // if reqo and we do not have this word in registered
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
                                state_buf[way_hit][i] = DNV_V;
                                touched_buf[way_hit][i] = false; // enable flush on next sync
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
                                state_buf[way_hit][i] = DNV_V;
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
                    word_mask_t word_mask = 0;
                    if(tag_hit){
                        for (int i = 0; i < WORDS_PER_LINE; i++){
                            HLS_UNROLL_LOOP(ON, "1");
                            if (fwd_in.word_mask & (1 << i)) {
                                if (state_buf[way_hit][i] == DNV_R) {
                                    line_buf[way_hit].range(BITS_PER_WORD*(i+1)-1,BITS_PER_WORD*i) = fwd_in.line.range(BITS_PER_WORD*(i+1)-1,BITS_PER_WORD*i);
                                } else {
                                    word_mask |= 1 << i;
                                    touched_buf[way_hit][i] = false;
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
                }
                break;
                default:
                break;
            }
        }
	} else if (do_ongoing_flush) {
		self_invalidate();

	} else if (do_cpu_req) { // assuming HPROT cacheable

	    addr_breakdown_t addr_br;
	    sc_uint<REQS_BITS> reqs_empty_i;

	    addr_br.breakdown(cpu_req.addr);

	    reqs_peek_req(addr_br.set, reqs_empty_i);
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

                if (word_hit && (state_buf[way_hit][addr_br.w_off] == DNV_R || (state_buf[way_hit][addr_br.w_off] == DNV_V && touched_buf[way_hit][addr_br.w_off]))) {
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
                HLS_DEFINE_PROTOCOL("send wt");
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
                        HLS_DEFINE_PROTOCOL("spandex_dual_req");
                        send_req_out(REQ_WB, hprot_buf[evict_way], line_addr_evict, line_buf[evict_way], word_mask);
                        wait();
                        fill_reqs(0, addr_br, 0, 0, 0, DNV_RI, 0, 0, line_buf[evict_way], word_mask, reqs_empty_i);
                        reqs[reqs_empty_i].tag = tag_buf[evict_way];
                    }

                    for (int i = 0; i < WORDS_PER_LINE; i ++)
                    {
                        HLS_UNROLL_LOOP(ON, "4");
                        state_buf[evict_way][i] = DNV_I;
                        touched_buf[evict_way][i] = false;
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
                                    HLS_DEFINE_PROTOCOL("req_wtfwd out");
                                    if(cpu_req.use_owner_pred){
                                        send_fwd_out(FWD_WTfwd, cpu_req.pred_cid, 1, addr_br.line_addr, line_buf[way_write], 1 << addr_br.w_off);
                                    }else{
                                        send_req_out(REQ_WTfwd, cpu_req.hprot, addr_br.line_addr, line_buf[way_write], 1 << addr_br.w_off);
                                    }
                                    wait();
                                    state_buf[way_write][addr_br.w_off] = DNV_V;
                                    touched_buf[way_write][addr_br.w_off] = false;
                                }
                            }
                            break;
                            default:
                            break;
                        }
                }else{
                    if ((!word_hit) || (state_buf[way_write][addr_br.w_off] != DNV_R)) { // if no hit or not in registered
                        HLS_DEFINE_PROTOCOL("spandex_dual_req");
                        send_req_out(REQ_O, cpu_req.hprot, addr_br.line_addr, line_buf[way_write], 1 << addr_br.w_off); // send registration
                        wait();
                        state_buf[way_write][addr_br.w_off] = DNV_R; // directly go to registered
                    }
                }

            }
            // else if read
            // assuming line granularity read MESI style
			else if (tag_hit) {
                word_mask_t word_mask = 0;
                for (int i = 0; i < WORDS_PER_LINE; i ++)
                {
                    HLS_UNROLL_LOOP(ON, "2");
                    if (state_buf[way_hit][i] == DNV_I)
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
                    touched_buf[way_hit][addr_br.w_off] = true;
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

                {
                    HLS_DEFINE_PROTOCOL("spandex_dual_req");
                    if (word_mask)
                    {
                        send_req_out(REQ_WB, hprot_buf[evict_way], line_addr_evict, line_buf[evict_way], word_mask);
                        fill_reqs(0, addr_br, 0, 0, 0, DNV_RI, 0, 0, line_buf[evict_way], word_mask, reqs_empty_i);
                        reqs[reqs_empty_i].tag = tag_buf[evict_way];
                        //wait();
                    }
                }   


                for (int i = 0; i < WORDS_PER_LINE; i ++)
                {
                    HLS_UNROLL_LOOP(ON, "4");
                    // Update it now b/c set_conflict will be true
                    // state_buf[evict_way][i] = DNV_I;
                    // touched_buf[evict_way][i] = false;
                    states[base + evict_way][i] = DNV_I;
                    touched[base + evict_way][i] = false;
                }

                send_inval(line_addr_evict);
                
                // make it set_conflict so that next ctrl loop there is an empty way
                set_conflict = 1;
                cpu_req_conflict = cpu_req;
			}
	    }
	}
    if ((do_cpu_req && !set_conflict) || (do_fwd && !fwd_stall) || do_rsp){
        for (int i = 0; i < L2_WAYS; i++) {
            HLS_UNROLL_LOOP(ON, "5");
            for (int j = 0; j < WORDS_PER_LINE; j++) {
                HLS_UNROLL_LOOP(ON, "6");
                states[base + i][j] = state_buf[i][j];
                touched[base + i][j] = touched_buf[i][j];
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
        for (int j = 0; j < WORDS_PER_LINE; j++) {
	        HLS_UNROLL_LOOP(ON, "5");
            state_buf_dbg[i][j] = state_buf[i][j];
        }
	}

    for (int i = 0; i < L2_LINES; i++) {
        HLS_UNROLL_LOOP(ON, "states_dbg_i");
        for (int j = 0; j < WORDS_PER_LINE; j++) {
            HLS_UNROLL_LOOP(ON, "states_dbg_j");
            states_dbg[i][j] = states[i][j];

        }

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
        for (int j = 0; j < WORDS_PER_LINE; j++) {
            HLS_UNROLL_LOOP(ON, "6");
            states[i][j] = DNV_I;
            touched[i][j] = false;
        }
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
        for (int j = 0; j < WORDS_PER_LINE; j++) {
            HLS_UNROLL_LOOP(ON, "7");
            states[i][j] = DNV_I;
        }
	}

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

void l2_denovo::put_reqs(l2_set_t set, l2_way_t way, l2_tag_t tag, line_t line, hprot_t hprot, state_t state,
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

    tag_buf[0] = tags.port2[0][base + 0];
    for (int i = 0; i < WORDS_PER_LINE; i++) {
        HLS_UNROLL_LOOP(ON, "8");
        state_buf[0][i] = states[base + 0][i];
        touched_buf[0][i] = touched[base + 0][i];
    }
    hprot_buf[0] = hprots.port2[0][base + 0];
    line_buf[0] = lines.port2[0][base + 0];

#if (L2_WAYS >= 2)

    tag_buf[1] = tags.port3[0][base + 1];
    for (int i = 0; i < WORDS_PER_LINE; i++) {
        HLS_UNROLL_LOOP(ON, "9");
        state_buf[1][i] = states[base + 1][i];
        touched_buf[1][i] = touched[base + 1][i];
    }
    hprot_buf[1] = hprots.port3[0][base + 1];
    line_buf[1] = lines.port3[0][base + 1];

#if (L2_WAYS >= 4)

    tag_buf[2] = tags.port4[0][base + 2];
    for (int i = 0; i < WORDS_PER_LINE; i++) {
        HLS_UNROLL_LOOP(ON, "10");
        state_buf[2][i] = states[base + 2][i];
        touched_buf[2][i] = touched[base + 2][i];
    }
    hprot_buf[2] = hprots.port4[0][base + 2];
    line_buf[2] = lines.port4[0][base + 2];

    tag_buf[3] = tags.port5[0][base + 3];
    for (int i = 0; i < WORDS_PER_LINE; i++) {
        HLS_UNROLL_LOOP(ON, "11");
        state_buf[3][i] = states[base + 3][i];
        touched_buf[3][i] = touched[base + 3][i];
    }
    hprot_buf[3] = hprots.port5[0][base + 3];
    line_buf[3] = lines.port5[0][base + 3];

#if (L2_WAYS >= 8)

    tag_buf[4] = tags.port6[0][base + 4];
    for (int i = 0; i < WORDS_PER_LINE; i++) {
        HLS_UNROLL_LOOP(ON, "12");
        state_buf[4][i] = states[base + 4][i];
        touched_buf[4][i] = touched[base + 4][i];
    }
    hprot_buf[4] = hprots.port6[0][base + 4];
    line_buf[4] = lines.port6[0][base + 4];

    tag_buf[5] = tags.port7[0][base + 5];
    for (int i = 0; i < WORDS_PER_LINE; i++) {
        HLS_UNROLL_LOOP(ON, "13");
        state_buf[5][i] = states[base + 5][i];
        touched_buf[5][i] = touched[base + 5][i];
    }
    hprot_buf[5] = hprots.port7[0][base + 5];
    line_buf[5] = lines.port7[0][base + 5];

    tag_buf[6] = tags.port8[0][base + 6];
    for (int i = 0; i < WORDS_PER_LINE; i++) {
        HLS_UNROLL_LOOP(ON, "14");
        state_buf[6][i] = states[base + 6][i];
        touched_buf[6][i] = touched[base + 6][i];
    }
    hprot_buf[6] = hprots.port8[0][base + 6];
    line_buf[6] = lines.port8[0][base + 6];

    tag_buf[7] = tags.port9[0][base + 7];
    for (int i = 0; i < WORDS_PER_LINE; i++) {
        HLS_UNROLL_LOOP(ON, "15");
        state_buf[7][i] = states[base + 7][i];
        touched_buf[7][i] = touched[base + 7][i];
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
        if (state_buf[i][j] != DNV_I) {
            line_present = true;
        }
    }

	if (tag_buf[i] == addr_br.tag && line_present) {
	    tag_hit = true;
	    way_hit = i;
	}

	if (tag_buf[i] == addr_br.tag && line_present && state_buf[i][addr_br.w_off] != DNV_I) {
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

void l2_denovo::reqs_peek_req(l2_set_t set, sc_uint<REQS_BITS> &reqs_i)
{
    REQS_PEEK_REQ;

    set_conflict = reqs_cnt == 0; // if no empty reqs left, cannot process CPU request

    for (unsigned int i = 0; i < N_REQS; ++i) {
	REQS_PEEK_REQ_LOOP;

	if (reqs[i].state == DNV_I)
	    reqs_i = i;

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
	for (int i = 0; i < L2_LINES; i++)
	{
        HLS_UNROLL_LOOP(ON, "reset-states");
        for (int j = 0; j < WORDS_PER_LINE; j++)
        {
            HLS_UNROLL_LOOP(ON, "reset-states");
            if (states[i][j] == DNV_V && !touched[i][j])
            {
                states[i][j] = DNV_I;
            }
            touched[i][j] = false;
        }
	}


}

