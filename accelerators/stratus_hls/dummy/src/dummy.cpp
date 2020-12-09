// Copyright (c) 2011-2019 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#include "dummy.hpp"
#include "dummy_directives.hpp"

// Functions

#include "dummy_functions.hpp"

// Processes

void dummy::load_input()
{

//     // Reset
//     {
//         HLS_PROTO("load-reset");

//         this->reset_load_input();

//         wait();
//     }

//     // Config
//     uint32_t tokens;
//     uint32_t batch;
//     {
//         HLS_PROTO("load-config");

//         cfg.wait_for_config(); // config process
//         conf_info_t config = this->conf_info.read();

//         tokens = config.base_addr;
//         batch = config.owner;
//     }

//     // Load
//     bool ping = true;
//     uint32_t offset = 0;
//     for (int n = 0; n < batch; n++)
//         for (int b = tokens; b > 0; b -= PLM_SIZE)
//         {
//             HLS_PROTO("load-dma");

//             uint32_t len = b > PLM_SIZE ? PLM_SIZE : b;
//             dma_info_t dma_info(offset * DMA_BEAT_PER_WORD, len * DMA_BEAT_PER_WORD, DMA_SIZE);
//             offset += len;

//             this->dma_read_ctrl.put(dma_info);

//             for (uint16_t i = 0; i < len; i++) {
//                 uint64_t data;
//                 sc_dt::sc_bv<64> data_bv;

// #if (DMA_WIDTH == 64)
//                 data_bv = this->dma_read_chnl.get();
// #elif (DMA_WIDTH == 32)
//                 data_bv.range(31, 0) = this->dma_read_chnl.get();
//                 wait();
//                 data_bv.range(63, 32) = this->dma_read_chnl.get();
// #endif
//                 wait();
//                 data = data_bv.to_uint64();

//                 if (ping)
//                     plm0[i] = data;
//                 else
//                     plm1[i] = data;
//             }
//             this->load_compute_handshake();
//             ping = !ping;
//         }

//     // Conclude
//     {
//         this->process_done();
//     }

// Reset
    {
        HLS_PROTO("load-reset");

        this->reset_load_input();

        // explicit PLM ports reset if any

        // User-defined reset code

        wait();
    }

    // Config
    /* <<--params-->> */
    int32_t base_addr;
    int32_t owner;
    int32_t owner_pred;
    int32_t stride_size;
    int32_t coh_msg;
    int32_t array_length;
    int32_t req_type;
    int32_t element_size;
    {
        HLS_PROTO("load-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();

        // User-defined config code
        /* <<--local-params-->> */
        base_addr = config.base_addr;
        owner = config.owner;
        owner_pred = config.owner_pred;
        stride_size = config.stride_size;
        coh_msg = config.coh_msg;
        array_length = config.array_length;
        req_type = config.req_type;
        element_size = config.element_size;
    }

    uint32_t offset = 0;
    if(req_type != 0)
        array_length = 0;
    
    for(int i = 0; i < array_length; i++){
        HLS_PROTO("load-dma");
        dma_info_t dma_info(offset, 1, SIZE_DWORD);
        offset += stride_size;
        this->dma_read_ctrl.put(dma_info);
        uint64_t data;
        sc_dt::sc_bv<64> data_bv;
        data_bv = this->dma_read_chnl.get();
        wait();
        data = data_bv.to_uint64();
        tmp = data;
    }


    {
        this->load_compute_handshake();
    }

    // Conclude
    {
        this->process_done();
    }
}



void dummy::store_output()
{

    {
        HLS_PROTO("store-reset");

        this->reset_store_output();

        // explicit PLM ports reset if any

        // User-defined reset code

        wait();
    }

    // Config
    /* <<--params-->> */
    int32_t base_addr;
    int32_t owner;
    int32_t owner_pred;
    int32_t stride_size;
    int32_t coh_msg;
    int32_t array_length;
    int32_t req_type;
    int32_t element_size;
    {
        HLS_PROTO("store-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();

        // User-defined config code
        /* <<--local-params-->> */
        base_addr = config.base_addr;
        owner = config.owner;
        owner_pred = config.owner_pred;
        stride_size = config.stride_size;
        coh_msg = config.coh_msg;
        array_length = config.array_length;
        req_type = config.req_type;
        element_size = config.element_size;
    }

    // Store
    {
        HLS_PROTO("store-dma");
        this->store_compute_handshake();

        uint32_t offset = 0;
        if(req_type != 1)
            array_length = 0;
        
        for(int i = 0; i < array_length; i++){
            dma_info_t dma_info(offset, 1, SIZE_DWORD);
            offset += stride_size;
            this->dma_write_ctrl.put(dma_info);
            uint64_t data = 0x0ecebbeefc0ffee0;
            sc_dt::sc_bv<64> data_bv(data);
            this->dma_write_chnl.put(data_bv);
            wait();
        }

    }

    // Conclude
    {
        this->accelerator_done();
        this->process_done();
    }
}


void dummy::compute_kernel()
{
    // // Reset
    // {
    //     HLS_PROTO("compute-reset");

    //     this->reset_compute_kernel();

    //     wait();
    // }

    // // Config
    // {
    //     HLS_PROTO("compute-config");

    //     cfg.wait_for_config(); // config process
    //     conf_info_t config = this->conf_info.read();
    // }


    // // Compute (dummy does nothing)
    // while (true) {
    //     this->compute_load_handshake();
    //     this->compute_store_handshake();
    // }
    // Reset
    {
        HLS_PROTO("compute-reset");

        this->reset_compute_kernel();

        wait();
    }

    // Config
    {
        HLS_PROTO("compute-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();
    }


    // Compute (dummy does nothing)
    {
        this->compute_load_handshake();
        this->compute_store_handshake();
    }

    {
        this->process_done();
    }
}
