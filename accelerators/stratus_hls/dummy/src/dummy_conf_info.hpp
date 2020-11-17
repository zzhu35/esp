// Copyright (c) 2011-2019 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#ifndef __DUMMY_CONF_INFO_HPP__
#define __DUMMY_CONF_INFO_HPP__

#include <systemc.h>

//
// Configuration parameters for the accelerator.
//
class conf_info_t
{
public:

    //
    // constructors
    //
    conf_info_t()
    {
        /* <<--ctor-->> */
        this->base_addr = 0;
        this->owner = 0;
        this->owner_pred = 0;
        this->stride_size = 0;
        this->coh_msg = 0;
        this->array_length = 1;
        this->req_type = 0;
        this->element_size = 1;
    }

    conf_info_t(
        /* <<--ctor-args-->> */
        int32_t base_addr, 
        int32_t owner, 
        int32_t owner_pred, 
        int32_t stride_size, 
        int32_t coh_msg, 
        int32_t array_length, 
        int32_t req_type, 
        int32_t element_size
        )
    {
        /* <<--ctor-custom-->> */
        this->base_addr = base_addr;
        this->owner = owner;
        this->owner_pred = owner_pred;
        this->stride_size = stride_size;
        this->coh_msg = coh_msg;
        this->array_length = array_length;
        this->req_type = req_type;
        this->element_size = element_size;
    }

    // equals operator
    inline bool operator==(const conf_info_t &rhs) const
    {
        /* <<--eq-->> */
        if (base_addr != rhs.base_addr) return false;
        if (owner != rhs.owner) return false;
        if (owner_pred != rhs.owner_pred) return false;
        if (stride_size != rhs.stride_size) return false;
        if (coh_msg != rhs.coh_msg) return false;
        if (array_length != rhs.array_length) return false;
        if (req_type != rhs.req_type) return false;
        if (element_size != rhs.element_size) return false;
        return true;
    }

    // assignment operator
    inline conf_info_t& operator=(const conf_info_t& other)
    {
        /* <<--assign-->> */
        base_addr = other.base_addr;
        owner = other.owner;
        owner_pred = other.owner_pred;
        stride_size = other.stride_size;
        coh_msg = other.coh_msg;
        array_length = other.array_length;
        req_type = other.req_type;
        element_size = other.element_size;
        return *this;
    }

    // VCD dumping function
    friend void sc_trace(sc_trace_file *tf, const conf_info_t &v, const std::string &NAME)
    {}

    // redirection operator
    friend ostream& operator << (ostream& os, conf_info_t const &conf_info)
    {
        os << "{";
        /* <<--print-->> */
        os << "base_addr = " << conf_info.base_addr << ", ";
        os << "owner = " << conf_info.owner << ", ";
        os << "owner_pred = " << conf_info.owner_pred << ", ";
        os << "stride_size = " << conf_info.stride_size << ", ";
        os << "coh_msg = " << conf_info.coh_msg << ", ";
        os << "array_length = " << conf_info.array_length << ", ";
        os << "req_type = " << conf_info.req_type << ", ";
        os << "element_size = " << conf_info.element_size << "";
        os << "}";
        return os;
    }

        /* <<--params-->> */
        int32_t base_addr;
        int32_t owner;
        int32_t owner_pred;
        int32_t stride_size;
        int32_t coh_msg;
        int32_t array_length;
        int32_t req_type;
        int32_t element_size;
};

#endif // __DUMMY_CONF_INFO_HPP__
