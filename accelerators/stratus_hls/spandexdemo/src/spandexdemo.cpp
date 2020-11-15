// Copyright (c) 2011-2019 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#include "spandexdemo.hpp"
#include "spandexdemo_directives.hpp"

// Functions

#include "spandexdemo_functions.hpp"

// Processes

void spandexdemo::load_input()
{

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



void spandexdemo::store_output()
{
    // Reset
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

void spandexdemo::compute_kernel()
{
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


// void spandexdemo::compute_kernel()
// {
//     // Reset
//     {
//         HLS_PROTO("compute-reset");

//         this->reset_compute_kernel();

//         // explicit PLM ports reset if any

//         // User-defined reset code

//         wait();
//     }

//     // Config
//     /* <<--params-->> */
//     int32_t base_addr;
//     int32_t owner;
//     int32_t owner_pred;
//     int32_t stride_size;
//     int32_t coh_msg;
//     int32_t array_length;
//     int32_t req_type;
//     int32_t element_size;
//     {
//         HLS_PROTO("compute-config");

//         cfg.wait_for_config(); // config process
//         conf_info_t config = this->conf_info.read();

//         // User-defined config code
//         /* <<--local-params-->> */
//         base_addr = config.base_addr;
//         owner = config.owner;
//         owner_pred = config.owner_pred;
//         stride_size = config.stride_size;
//         coh_msg = config.coh_msg;
//         array_length = config.array_length;
//         req_type = config.req_type;
//         element_size = config.element_size;
//     }


//     // Compute
//     bool ping = true;
//     {
//         for (uint16_t b = 0; b < 1; b++)
//         {
//             uint32_t in_length = element_size;
//             uint32_t out_length = element_size;
//             int out_rem = out_length;

//             for (int in_rem = in_length; in_rem > 0; in_rem -= PLM_IN_WORD)
//             {

//                 uint32_t in_len  = in_rem  > PLM_IN_WORD  ? PLM_IN_WORD  : in_rem;
//                 uint32_t out_len = out_rem > PLM_OUT_WORD ? PLM_OUT_WORD : out_rem;

//                 this->compute_load_handshake();

//                 // Computing phase implementation
//                 for (int i = 0; i < in_len; i++) {
//                     if (ping)
//                         plm_out_ping[i] = plm_in_ping[i];
//                     else
//                         plm_out_pong[i] = plm_in_pong[i];
//                 }

//                 out_rem -= PLM_OUT_WORD;
//                 this->compute_store_handshake();
//                 ping = !ping;
//             }
//         }

//         // Conclude
//         {
//             this->process_done();
//         }
//     }
// }

