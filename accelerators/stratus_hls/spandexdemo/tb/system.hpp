// Copyright (c) 2011-2019 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#ifndef __SYSTEM_HPP__
#define __SYSTEM_HPP__

#include "spandexdemo_conf_info.hpp"
#include "spandexdemo_debug_info.hpp"
#include "spandexdemo.hpp"
#include "spandexdemo_directives.hpp"

#include "esp_templates.hpp"

const size_t MEM_SIZE = 8388608 / (DMA_WIDTH/8);

#include "core/systems/esp_system.hpp"

#ifdef CADENCE
#include "spandexdemo_wrap.h"
#endif

class system_t : public esp_system<DMA_WIDTH, MEM_SIZE>
{
public:

    // ACC instance
#ifdef CADENCE
    spandexdemo_wrapper *acc;
#else
    spandexdemo *acc;
#endif

    // Constructor
    SC_HAS_PROCESS(system_t);
    system_t(sc_module_name name)
        : esp_system<DMA_WIDTH, MEM_SIZE>(name)
    {
        // ACC
#ifdef CADENCE
        acc = new spandexdemo_wrapper("spandexdemo_wrapper");
#else
        acc = new spandexdemo("spandexdemo_wrapper");
#endif
        // Binding ACC
        acc->clk(clk);
        acc->rst(acc_rst);
        acc->dma_read_ctrl(dma_read_ctrl);
        acc->dma_write_ctrl(dma_write_ctrl);
        acc->dma_read_chnl(dma_read_chnl);
        acc->dma_write_chnl(dma_write_chnl);
        acc->conf_info(conf_info);
        acc->conf_done(conf_done);
        acc->acc_done(acc_done);
        acc->debug(debug);

        /* <<--params-default-->> */
        base_addr = 0;
        owner = 0;
        owner_pred = 0;
        stride_size = 0;
        coh_msg = 0;
        array_length = 1;
        req_type = 0;
        element_size = 1;
    }

    // Processes

    // Configure accelerator
    void config_proc();

    // Load internal memory
    void load_memory();

    // Dump internal memory
    void dump_memory();

    // Validate accelerator results
    int validate();

    // Accelerator-specific data
    /* <<--params-->> */
    int32_t base_addr;
    int32_t owner;
    int32_t owner_pred;
    int32_t stride_size;
    int32_t coh_msg;
    int32_t array_length;
    int32_t req_type;
    int32_t element_size;

    uint32_t in_words_adj;
    uint32_t out_words_adj;
    uint32_t in_size;
    uint32_t out_size;
    int32_t *in;
    int32_t *out;
    int32_t *gold;

    // Other Functions
};

#endif // __SYSTEM_HPP__
