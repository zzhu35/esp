// Copyright (c) 2011-2019 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#ifndef __L2_DENOVO_HPP__
#define __L2_DENOVO_HPP__

#define USE_WB 1


#include "cache_utils.hpp"
#include "l2_denovo_directives.hpp"

#include EXP_MEM_INCLUDE_STRING(l2_denovo, tags, L2_SETS, L2_WAYS)
#include EXP_MEM_INCLUDE_STRING(l2_denovo, lines, L2_SETS, L2_WAYS)
#include EXP_MEM_INCLUDE_STRING(l2_denovo, hprots, L2_SETS, L2_WAYS)
#include EXP_MEM_INCLUDE_STRING(l2_denovo, evict_ways, L2_SETS, L2_WAYS)

class l2_denovo : public sc_module
{

public:

    // Clock signal
    sc_in<bool> clk;

    // Reset signal
    sc_in<bool> rst;

#ifdef L2_DEBUG
    // Debug signals
    sc_signal< sc_bv<ASSERT_WIDTH> >   asserts;
    sc_signal< sc_bv<BOOKMARK_WIDTH> > bookmark;

    sc_signal< sc_uint<REQS_BITS_P1> > reqs_cnt_dbg;
    sc_signal< sc_uint<WB_BITS_P1> > wbs_cnt_dbg;
    sc_signal< bool > set_conflict_dbg;
    sc_signal< l2_cpu_req_t > cpu_req_conflict_dbg;
    sc_signal< bool > evict_stall_dbg;
    sc_signal< bool > fwd_stall_dbg;
    sc_signal< bool > fwd_stall_ended_dbg;
    sc_signal< l2_fwd_in_t > fwd_in_stalled_dbg;
    sc_signal< sc_uint<REQS_BITS> > reqs_fwd_stall_i_dbg;
    sc_signal< bool > ongoing_atomic_dbg;
    sc_signal< line_addr_t > atomic_line_addr_dbg;
    sc_signal< sc_uint<REQS_BITS> > reqs_atomic_i_dbg;
    sc_signal< bool > ongoing_flush_dbg;
    sc_signal< uint32_t > flush_way_dbg;
    sc_signal< uint32_t > flush_set_dbg;

    sc_signal<bool> tag_hit_req_dbg;
    sc_signal<l2_way_t> way_hit_req_dbg;
    sc_signal<bool> empty_found_req_dbg;
    sc_signal<l2_way_t> empty_way_req_dbg;
    sc_signal<bool> reqs_hit_req_dbg;
    sc_signal<sc_uint<REQS_BITS> > reqs_hit_i_req_dbg;
    sc_signal<l2_way_t> way_hit_fwd_dbg;
    sc_signal<l2_way_t> peek_reqs_i_dbg;
    sc_signal<l2_way_t> peek_reqs_i_flush_dbg;
    sc_signal<bool> peek_reqs_hit_fwd_dbg;

    sc_signal<reqs_buf_t> reqs_dbg[N_REQS];
    sc_signal<wb_t> wbs_dbg[N_WB];
    sc_signal<l2_tag_t> tag_buf_dbg[L2_WAYS];
    sc_signal<state_t> state_buf_dbg[L2_WAYS][WORDS_PER_LINE];
    sc_signal<state_t> states_dbg[L2_LINES][WORDS_PER_LINE];
    sc_signal<l2_way_t>	evict_way_dbg;
    sc_signal< sc_uint<3> > watch_dog;
#endif

    // Other signals
    sc_out<bool> flush_done;

    // Input ports
    nb_get_initiator<l2_cpu_req_t>	l2_cpu_req;
    nb_get_initiator<l2_fwd_in_t>	l2_fwd_in;
    nb_get_initiator<l2_rsp_in_t>	l2_rsp_in;
    nb_get_initiator<bool>		    l2_flush;
    nb_get_initiator<bool>		    l2_sync;

    // Output ports
    put_initiator<l2_rd_rsp_t>	l2_rd_rsp;
    put_initiator<l2_inval_t>	l2_inval;
    nb_put_initiator<l2_req_out_t> l2_req_out;
    nb_put_initiator<l2_fwd_out_t> l2_fwd_out;
    nb_put_initiator<l2_rsp_out_t> l2_rsp_out;

#ifdef STATS_ENABLE
    put_initiator<bool> l2_stats;
#endif

    // Local memory
    EXP_MEM_TYPE_STRING(l2_denovo, tags, L2_SETS, L2_WAYS)<l2_tag_t, L2_LINES> tags;
    state_t states[L2_LINES][WORDS_PER_LINE]; // fast flush
    bool touched[L2_LINES][WORDS_PER_LINE];
    EXP_MEM_TYPE_STRING(l2_denovo, lines, L2_SETS, L2_WAYS)<line_t, L2_LINES> lines;
    EXP_MEM_TYPE_STRING(l2_denovo, hprots, L2_SETS, L2_WAYS)<hprot_t, L2_LINES> hprots;
    EXP_MEM_TYPE_STRING(l2_denovo, evict_ways, L2_SETS, L2_WAYS)<l2_way_t, L2_SETS> evict_ways;

    // Local registers
    reqs_buf_t	 reqs[N_REQS];
    wb_t wbs[N_WB];

    word_mask_t reqs_word_mask_in[N_REQS];


    l2_tag_t	 tag_buf[L2_WAYS];
    state_t	 state_buf[L2_WAYS][WORDS_PER_LINE];
    bool	 touched_buf[L2_WAYS][WORDS_PER_LINE];
    hprot_t	 hprot_buf[L2_WAYS];
    line_t	 line_buf[L2_WAYS];
    l2_way_t	 evict_way;


    // Constructor
    SC_CTOR(l2_denovo)
	: clk("clk")
	, rst("rst")
#ifdef L2_DEBUG
	, asserts("asserts")
	, bookmark("bookmark")
#endif
	, flush_done("flush_done")
	, l2_cpu_req("l2_cpu_req")
	, l2_fwd_in("l2_fwd_in")
	, l2_fwd_out("l2_fwd_out")
	, l2_rsp_in("l2_rsp_in")
	, l2_flush("l2_flush")
	, l2_sync("l2_sync")
	, l2_rd_rsp("l2_rd_rsp")
	, l2_inval("l2_inval")
	, l2_req_out("l2_req_out")
	, l2_rsp_out("l2_rsp_out")
#ifdef STATS_ENABLE
	, l2_stats("l2_stats")  
#endif
	{
	    // Cache controller process
	    SC_CTHREAD(ctrl, clk.pos());
	    reset_signal_is(rst, false);
	    // set_stack_size(0x400000);

	    // Assign clock and reset to put_get ports
	    l2_cpu_req.clk_rst (clk, rst);
	    l2_fwd_in.clk_rst (clk, rst);
	    l2_fwd_out.clk_rst (clk, rst);
	    l2_rsp_in.clk_rst (clk, rst);
	    l2_flush.clk_rst (clk, rst);
	    l2_sync.clk_rst (clk, rst);
	    l2_rd_rsp.clk_rst(clk, rst);
	    l2_inval.clk_rst(clk, rst);
	    l2_req_out.clk_rst(clk, rst);
	    l2_rsp_out.clk_rst(clk, rst);
#ifdef STATS_ENABLE
	    l2_stats.clk_rst(clk, rst);
#endif

	    // Flatten arrays
	    L2_DENOVO_FLATTEN_REGS;

	    // Preserve signals
	    PRESERVE_SIGNALS;

	    // Clock binding for memories
	    tags.clk(this->clk);
	    hprots.clk(this->clk);
	    lines.clk(this->clk);
	    evict_ways.clk(this->clk);
	}

    // Processes
    void ctrl(); // cache controller

    /* Functions for the reset phase */
    inline void reset_io();

    /* Functions to receive input messages */
    void get_cpu_req(l2_cpu_req_t &cpu_req);
    void get_fwd_in(l2_fwd_in_t &fwd_in);
    void get_rsp_in(l2_rsp_in_t &rsp_in);
    bool get_flush();

    /* Functions to send output messages */
    void send_rd_rsp(line_t lines);
    void send_req_out(coh_msg_t coh_msg, hprot_t hprot, line_addr_t line_addr, line_t lines, word_mask_t word_mask);
    void send_rsp_out(coh_msg_t coh_msg, cache_id_t req_id, bool to_req, line_addr_t line_addr, line_t line, word_mask_t word_mask);
    void send_fwd_out(coh_msg_t coh_msg, cache_id_t dst_id, bool to_dst, line_addr_t line_addr, line_t line, word_mask_t word_mask);
    void send_inval(line_addr_t addr_inval);
    /* Functions to move around buffered lines */
    void fill_reqs(cpu_msg_t cpu_msg, addr_breakdown_t addr_br, l2_tag_t tag_estall, l2_way_t way_hit, 
		   hsize_t hsize, unstable_state_t state, hprot_t hprot, word_t word, line_t line, word_mask_t word_mask,
		   sc_uint<REQS_BITS> reqs_i);
    void put_reqs(l2_set_t set, l2_way_t way, l2_tag_t tag, line_t lines, hprot_t hprot, state_t state,
		  sc_uint<REQS_BITS> reqs_i);

    /* Functions to search for cache lines either in memory or buffered */
    void read_set(l2_set_t set);
    void tag_lookup(addr_breakdown_t addr_br, bool &tag_hit, l2_way_t &way_hit, bool &empty_way_found, l2_way_t &empty_way, bool &word_hit);
    void reqs_lookup(line_breakdown_t<l2_tag_t, l2_set_t> line_addr_br,
		     sc_uint<REQS_BITS> &reqs_hit_i);
    void reqs_peek_req(l2_set_t set, sc_uint<REQS_BITS> &reqs_i, sc_uint<REQS_BITS> &reqs_i2);
    void reqs_peek_flush(l2_set_t set, sc_uint<REQS_BITS> &reqs_i);
    void reqs_peek_fwd(addr_breakdown_t addr_br);

    void self_invalidate();


    // write buffer

    void drain_wb();
    void add_wb(bool& success, addr_breakdown_t addr_br, word_t word, l2_way_t way, hprot_t hprot, bool dcs_en, bool use_owner_pred, cache_id_t pred_cid);
    void peek_wb(bool& hit, sc_uint<WB_BITS>& wb_i, addr_breakdown_t addr_br);
    void dispatch_wb(bool& success, sc_uint<WB_BITS> wb_i);


#ifdef STATS_ENABLE
    void send_stats(bool stats);
#endif

    // line_t make_line_of_addr(addr_t addr); // is this needed here? not called by l2.cpp

private:

    /* Variables for debug*/ 
#ifdef L2_DEBUG
    sc_bv<ASSERT_WIDTH>   asserts_tmp;
    sc_bv<BOOKMARK_WIDTH> bookmark_tmp;
#endif

    /* Variables for stalls, conflicts and atomic operations */
    bool is_to_req[2];
    sc_uint<REQS_BITS_P1> reqs_cnt;
    sc_uint<WB_BITS_P1> wbs_cnt;
    sc_uint<WB_BITS> wb_evict;
    bool drain_in_progress;
    bool set_conflict;
    
    l2_cpu_req_t cpu_req_conflict;
    bool evict_stall;
    bool fwd_stall;
    bool fwd_stall_ended;
    l2_fwd_in_t fwd_in_stalled;
    sc_uint<REQS_BITS> reqs_fwd_stall_i;
    bool ongoing_atomic;
    line_addr_t atomic_line_addr;
    sc_uint<REQS_BITS> reqs_atomic_i;
    bool ongoing_flush;
    uint32_t flush_way, flush_set;
};


#endif /* __L2_DENOVO_HPP__ */