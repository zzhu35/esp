// Copyright (c) 2011-2019 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#ifndef __CACHES_CONSTS_HPP__
#define __CACHES_CONSTS_HPP__

#include "log2.h"
#include <algorithm>

#ifdef RTL_CACHE
#include "cache_cfg.hpp"
#endif

/*
 * System
 */

// System configuration
#define MAX_N_L2 16
#define MAX_N_L2_BITS ilog2(MAX_N_L2)

#define MAX_N_LLC 64
#define MAX_N_LLC_BITS ilog2(MAX_N_LLC)

/*
 * Caches
 */

//
// Common
//

#ifndef ADDR_BITS
#define ADDR_BITS	32 // defined in l2,llc/stratus/project.tcl
#endif
#ifndef BYTE_BITS
#define BYTE_BITS	3 // defined in l2,llc/stratus/project.tcl
#endif
#ifndef WORD_BITS
#define WORD_BITS	1 // defined in l2,llc/stratus/project.tcl
#endif

#define OFFSET_BITS	(BYTE_BITS + WORD_BITS)

#define LINE_RANGE_HI	(ADDR_BITS - 1)
#define LINE_RANGE_LO	OFFSET_BITS
#define TAG_RANGE_HI	LINE_RANGE_HI
#define SET_RANGE_LO	LINE_RANGE_LO
#define OFF_RANGE_HI	(OFFSET_BITS - 1)
#define OFF_RANGE_LO	0
#define W_OFF_RANGE_HI	OFF_RANGE_HI
#define W_OFF_RANGE_LO	(OFFSET_BITS - WORD_BITS)
#define B_OFF_RANGE_HI	(W_OFF_RANGE_LO - 1)
#define B_OFF_RANGE_LO	OFF_RANGE_LO

#define SET_OFFSET	(1 << OFFSET_BITS)
#define WORD_OFFSET	(1 << BYTE_BITS)
#define LINE_ADDR_BITS  (ADDR_BITS - SET_RANGE_LO)

// Cache sizes
#define BYTES_PER_WORD		(1 << BYTE_BITS)
#define BITS_PER_WORD		(BYTES_PER_WORD << 3)
#define BITS_PER_HALFWORD	(BITS_PER_WORD >> 1)
#define BITS_PER_DOUBLEWORD (BITS_PER_WORD << 1)
#define BITS_PER_LINE		(BITS_PER_WORD * WORDS_PER_LINE)
#define WORDS_PER_LINE		(1 << WORD_BITS)
#define WORD_MASK_ALL       ((1 << WORDS_PER_LINE) - 1)

// Cache data types width
#define CPU_MSG_TYPE_WIDTH	2
#define COH_MSG_TYPE_WIDTH	4
#define DMA_MSG_TYPE_WIDTH      1
#define MIX_MSG_TYPE_WIDTH	(COH_MSG_TYPE_WIDTH + DMA_MSG_TYPE_WIDTH)
#define HSIZE_WIDTH		3
#define DCS_WIDTH               2
// TODO: HPROT_WIDTH should be 1, but that triggers a bug of the memory generator
#define HPROT_WIDTH		2
#define INVACK_CNT_WIDTH	MAX_N_L2_BITS
#define INVACK_CNT_CALC_WIDTH   (INVACK_CNT_WIDTH + 1)
#define CACHE_ID_WIDTH          MAX_N_L2_BITS
#define LLC_COH_DEV_ID_WIDTH    MAX_N_LLC_BITS

//
// L2
//

#ifndef L2_WAYS
#define L2_WAYS      8 // defined in l2/stratus/project.tcl
#endif

#ifndef L2_SETS
#define L2_SETS      256  // defined in l2/stratus/project.tcl
#endif

#define L2_WAY_BITS	ilog2(L2_WAYS)
#define L2_SET_BITS	ilog2(L2_SETS)
#define L2_LINES	(L2_SETS * L2_WAYS)
#define L2_ADDR_BITS    (L2_SET_BITS+L2_WAY_BITS)
#define L2_TAG_BITS	(ADDR_BITS - OFFSET_BITS - L2_SET_BITS)
#define L2_TAG_RANGE_LO	(ADDR_BITS - L2_TAG_BITS)
#define L2_SET_RANGE_HI	(L2_TAG_RANGE_LO - 1)
#define L2_TAG_OFFSET	(1 << L2_TAG_RANGE_LO)


// write buffer
#define N_WB 4
#define WB_BITS 2
#define WB_BITS_P1 3
// Ongoing transaction buffers
#define N_REQS		4	// affects REQS_BITS
#define REQS_BITS	2	// depends on N_REQS
#define REQS_BITS_P1	3	// depends on N_REQS + 1

//
// LLC
//

#ifndef LLC_WAYS
#define LLC_WAYS      16 // defined in l2/stratus/project.tcl
#endif

#ifndef LLC_SETS
#define LLC_SETS      512  // defined in l2/stratus/project.tcl
#endif

#define LLC_WAY_BITS		ilog2(LLC_WAYS)
#define LLC_SET_BITS		ilog2(LLC_SETS)
#define LLC_LINES		(LLC_SETS * LLC_WAYS)
#define LLC_ADDR_BITS           (LLC_SET_BITS+LLC_WAY_BITS)
#define LLC_TAG_BITS		(ADDR_BITS - OFFSET_BITS - LLC_SET_BITS)
#define LLC_TAG_RANGE_LO	(ADDR_BITS - LLC_TAG_BITS)
#define LLC_SET_RANGE_HI	(LLC_TAG_RANGE_LO - 1)
#define LLC_TAG_OFFSET		(1 << LLC_TAG_RANGE_LO)
#define LLC_LOOKUP_WAYS         16


// Ongoing transaction buffers
#define LLC_N_REQS		4	// affects LLC_REQS_BITS
#define LLC_REQS_BITS   	(ilog2(LLC_N_REQS))	// depends on LLC_N_REQS
#define LLC_REQS_BITS_P1	(LLC_REQS_BITS + 1)	// depends on LLC_N_REQS + 1


/*
 * Testbench
 */

// L2 operation behavior
#define HIT		0
#define MISS		1
#define MISS_EVICT	2

// Invalidation acknowledges order
#define DATA_FIRST	0
#define DATA_HALFWAY	1
#define DATA_LAST	2

// Fwd testing case
#define FWD_NONE	0
#define FWD_NOSTALL	1
#define FWD_STALL	2
#define FWD_STALL_XMW	3
#define FWD_STALL_EVICT	4

// DMA operations testbench
#define NO_DIRTY	0
#define DIRTY		1
#define NO_EVICT	0
#define EVICT		1

/*
 * Coherence
 */

/* Protocol states */

// N bits to indicate the state
#define DNV_NUM_STATE       8
#define DNV_STABLE_STATE_BITS   ilog2(DNV_NUM_STATE)
#define STABLE_STATE_BITS	2	// depends on # of stable states
#define LLC_STABLE_STATE_BITS	2
#define UNSTABLE_STATE_BITS	4	// depends on # of unstable states
#define LLC_UNSTABLE_STATE_BITS	4	// depends on # of unstable states

// L2 Stable states
#define INVALID			0
#define SHARED			1
#define EXCLUSIVE		2
#define MODIFIED		3


// #define MID merged with EID

// L2 Unstable states
#define ISD			1
#define IMAD			2
#define IMADW			3
#define IMA			4
#define IMAW			5
#define SMAD			6
#define SMADW			7
#define SMA			8
#define SMAW			9
#define XMW			10
#define IIA			11
#define SIA			12
#define MIA			13

// LLC states
#define LLC_I       0
#define LLC_V       1
#define LLC_S       2

// LLC unstable states
#define LLC_IV      1
#define LLC_IS      2
#define LLC_IO      3
#define LLC_SO      4
#define LLC_SV      5
#define LLC_OS      6
#define LLC_OV      7
#define LLC_SWB     8
#define LLC_OWB     9
#define LLC_SI      10
#define LLC_WB      11


// RTL watch dogs
#define WDOG_1      1

// DeNovo states
#define DNV_I       0
#define DNV_MAX_V   (DNV_S - 1)
#define DNV_S       (DNV_NUM_STATE - 2)
#define DNV_R       (DNV_NUM_STATE - 1)

// DeNovo Transient state
#define DNV_IV      1
#define DNV_II      2
#define DNV_RI      3
#define DNV_AMO     4
#define DNV_IV_DCS  5
#define DNV_XR      6
#define DNV_XRV     7
#define DNV_IS      8

// GPU states
#define GPU_I       0
#define GPU_V       1

// DeNovo Transient state
#define GPU_IV      1
#define GPU_II      2
#define GPU_AMO     3

#define MAX_RETRY 4
#define MAX_RETRY_BITS ilog2(MAX_RETRY)

/*
 * Protocol messages
 */

// CPU requests (L1 to L2)
#define READ		0
#define READ_ATOMIC	1
#define WRITE		2
#define WRITE_ATOMIC	3

// CPU DCS
#define DCS_ReqWTfwd    0

// LLC requests (LLC to mem)
#define LLC_READ  0
#define LLC_WRITE 1

// Coherence planes
#define REQ_PLANE 0
#define FWD_PLANE 1
#define RSP_PLANE 2

/******************
original ESP messages
*****************/

// requests (L2 to L3)
#define REQ_GETS		0
#define REQ_GETM		1
#define REQ_PUTS		2
#define REQ_PUTM		3
#define REQ_DMA_READ		4
#define REQ_DMA_WRITE		5
#define REQ_DMA_READ_BURST	0x1E
#define REQ_DMA_WRITE_BURST	0x1F

#define DMA_BURST_LENGTH_BITS 32

// forwards (L3 to L2)
#define FWD_GETS	0
#define FWD_GETM	1
#define FWD_INV		2
#define FWD_PUTACK	3
#define FWD_GETM_LLC    4
#define FWD_INV_LLC     5

// response (L2 to L2, L2 to L3, L3 to L2)
#define RSP_DATA	0
#define RSP_EDATA	1
#define RSP_INVACK	2
#define RSP_DATA_DMA    3

/******************
end original ESP messages
*****************/

#define ARIANE_AMO_BITS 6


// requests (L2/TU to L3)
#define REQ_S          0 // same as gets
#define REQ_Odata      1 // same as getm
#define REQ_WT         2
#define REQ_WB         3 // same as putm
#define REQ_O          4
#define REQ_V          5
#define REQ_WTdata     6
#define REQ_AMO_SWAP    REQ_WTdata
#define REQ_AMO_ADD     7 // ADD
#define REQ_AMO_AND     8 // CLR
#define REQ_AMO_OR      9 // SET
#define REQ_AMO_XOR     10 // EOR
#define REQ_AMO_MAX     11 // SMAX
#define REQ_AMO_MAXU    12 // UMAX
#define REQ_AMO_MIN     13 // SMIN
#define REQ_AMO_MINU    14 // UMIN
/* DMA currently not used by Spandex */
#define REQ_WTfwd       15


// forwards (L3 to L2/TU)
#define FWD_REQ_S      0 // same as fwd_gets
#define FWD_REQ_Odata  1 // same as fwd_getm
#define FWD_INV_SPDX   2 // same as fwd_inv
#define FWD_WB_ACK     3 // same as fwd_putack
#define FWD_RVK_O      4 // same as getm_llc
#define FWD_REQ_V      7 // non existent in ESP
#define FWD_REQ_O      6
#define FWD_WTfwd      5

// response (L2/TU to L2/TU, L2/TU to L3, L3 to L2/TU)
#define RSP_S          0
#define RSP_Odata      1 // same as fwd_req_odata
#define RSP_INV_ACK_SPDX    2 // same as fwd_inv_spdx
#define RSP_NACK       3
#define RSP_RVK_O      4 // same as fwd_rvk_o
#define RSP_V          5
#define RSP_O          6 // same as fwd_req_o
#define RSP_WT         7
#define RSP_WTdata     8


// AMOS
#define AMO_SWAP    48      // 0b110000 // SWAP
#define AMO_ADD     32      // 0b100000 // ADD
#define AMO_AND     33      // 0b100001 // CLR
#define AMO_OR      35      // 0b100011 // SET
#define AMO_XOR     34      // 0b100010 // EOR
#define AMO_MAX     36      // 0b100100 // SMAX
#define AMO_MAXU    38      // 0b100110 // UMAX
#define AMO_MIN     37      // 0b100101 // SMIN
#define AMO_MINU    39      // 0b100111 // UMIN

// DMA burst not currently supported in Spandex

/*
 * AMBA Bus
 */

// hsize
#define BYTE		0
#define HALFWORD	1
#define WORD_32		2
#define WORD_64     3
#define WORDS_128   4
#define WORDS_256   5

//used for LLC transactions to memory
//byte bits matches hsize encoding for a word
//for both LEON(2) and ARIANE(3)
#define WORD BYTE_BITS
// hprot
#define INSTR 0
#define DATA  1

/*
 * Debug and report (currently not in use)
 */

// #define L2_DEBUG 1
// #define LLC_DEBUG 1

#define STATS_ENABLE 1

// Decide whether to send to LLC regular DMA transaction or to send the unrolled
// DMA transaction one cache line at a time

#define INTERNAL 0
#define EXTERNAL 1
#define DMA_UNROLL INTERNAL

#if (DMA_UNROLL == EXTERNAL)

#define DMA_READ REQ_DMA_READ
#define DMA_WRITE REQ_DMA_WRITE

#elif (DMA_UNROLL == INTERNAL)

#define DMA_READ REQ_DMA_READ_BURST
#define DMA_WRITE REQ_DMA_WRITE_BURST

#endif

//
// L2
//

#define ASSERT_WIDTH	19
#define BOOKMARK_WIDTH	32

#define AS_REQS_LOOKUP   		(1 << 0)
#define AS_RSP_DATA_XMAD		(1 << 1)
#define AS_RSP_DATA_XMADW		(1 << 2)
#define AS_RSP_DATA_DEFAULT		(1 << 3)
#define AS_RSP_INVACK_DEFAULT		(1 << 4)
#define AS_FWD_HIT_DEFAULT              (1 << 5)
#define AS_FWD_NOHIT_DEFAULT            (1 << 6)
#define AS_PUTACK_DEFAULT		(1 << 7)
#define AS_HIT_SMADX			(1 << 8)
#define AS_HIT_EMIA			(1 << 9)
#define AS_HIT_READ_ATOMIC_DEFAULT	(1 << 10)
#define AS_HIT_WRITE_DEFAULT		(1 << 11)
#define AS_HIT_DEFAULT			(1 << 12)
#define AS_MISS_DEFAULT			(1 << 13)
#define AS_EVICT_DEFAULT		(1 << 14)
#define AS_FLUSH_CHECK			(1 << 15)
#define AS_FLUSH_NOPUTACK		(1 << 16)
#define AS_RSP_WHILE_FLUSHING           (1 << 17)
#define AS_RSP_DEFAULT                  (1 << 18)

#define BM_GET_CPU_REQ			(1 << 0)
#define BM_GET_FWD_IN			(1 << 1)
#define BM_GET_RSP_IN			(1 << 2)
#define BM_GET_FLUSH			(1 << 3)
#define BM_SEND_RD_RSP			(1 << 4)
#define BM_SEND_INVAL			(1 << 5)
#define BM_SEND_REQ_OUT			(1 << 6)
#define BM_SEND_RSP_OUT			(1 << 7)
#define BM_FILL_REQS			(1 << 8)
#define BM_PUT_REQS			(1 << 9)
#define BM_TAG_LOOKUP			(1 << 10)
#define BM_REQS_LOOKUP			(1 << 11)
#define BM_REQS_PEEK_REQ		(1 << 12)
#define BM_REQS_PEEK_FWD		(1 << 13)
#define BM_RSP_EDATA_ISD		(1 << 14)
#define BM_RSP_DATA_ISD			(1 << 15)
#define BM_RSP_DATA_XMAD		(1 << 16)
#define BM_RSP_DATA_XMADW		(1 << 17)
#define BM_RSP_INVACK			(1 << 18)
#define BM_FWD_PUTACK			(1 << 19)
#define BM_FWD_STALL_BEGIN		(1 << 20)
#define BM_FWD_HIT_SMADX		(1 << 21)
#define BM_FWD_HIT_EMIA			(1 << 22)
#define BM_FWD_HIT_SIA			(1 << 23)
#define BM_FWD_NOHIT_GETS		(1 << 24)
#define BM_FWD_NOHIT_GETM		(1 << 25)
#define BM_FWD_NOHIT_INV		(1 << 26)
#define BM_FWD_NOHIT_DEFAULT		(1 << 27)
#define BM_SET_CONFLICT			(1 << 28)
#define BM_ATOMIC_OVERRIDE		(1 << 29)
#define BM_ATOMIC_CONTINUE		(1 << 30)
#define BM_HIT_READ			(1 << 31)
#define BM_HIT_READ_ATOMIC_S		(1 << 0)
#define BM_HIT_READ_ATOMIC_EM		(1 << 1)
#define BM_HIT_WRITE_S			(1 << 2)
#define BM_HIT_WRITE_EM			(1 << 3)
#define BM_MISS_READ			(1 << 4)
#define BM_MISS_READ_ATOMIC		(1 << 5)
#define BM_MISS_WRITE			(1 << 6)
#define BM_FLUSH_READ_SET		(1 << 7)

//
// LLC
//
// @TODO add LLC debug bitmasks
//#define LLC_ASSERT_WIDTH
//#define LLC_BOOKMARK_WIDTH
//#define BM_LLC_


#define AS_GENERIC		(1 << 0)
#define AS_GETS_S_NOSHARE	(1 << 1)
#define AS_GETS_S_ALREADYSHARE	(1 << 2)
#define AS_GETS_EM_ALREADYOWN	(1 << 3)
#define AS_GETS_SD_ALREADYSHARE (1 << 4)
#define AS_GETM_EM_ALREADYOWN	(1 << 5)

//
// Reporting
//

#define RPT_OFF 0
#define RPT_ON  1
#define RPT_TB  RPT_ON
#define RPT_RTL RPT_ON
#define RPT_BM  RPT_OFF
#define RPT_CU  RPT_OFF

#endif // __CACHES_CONSTS_HPP__
