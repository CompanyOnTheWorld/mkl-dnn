/*******************************************************************************
* Copyright 2016 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#ifndef MKLDNN_TEST_COMMON_HPP
#define MKLDNN_TEST_COMMON_HPP

#include <numeric>
#include <vector>
#include <cmath>

#include "gtest/gtest.h"

#include "mkldnn.hpp"

template <typename data_t> struct data_traits { };
template <> struct data_traits<float> {
    using precision = mkldnn::memory::precision;
    static const precision prec = precision::f32;
};

template <typename T> inline void assert_eq(T a, T b);
template <> inline void assert_eq<float>(float a, float b) {
    ASSERT_FLOAT_EQ(a, b);
}

inline size_t map_index(const mkldnn::memory::desc &md, size_t index) {
    const int ndims = md.data.tensor_desc.ndims;
    const int *dims = md.data.tensor_desc.dims;
    const int *pdims = md.data.layout_desc.blocking.padding_dims;
    const int *optd = md.data.layout_desc.blocking.offset_padding_to_data;

    const int *strides_block = md.data.layout_desc.blocking.strides[0];
    const int *strides_within_block = md.data.layout_desc.blocking.strides[1];

    size_t ph_index = 0;

    for (int rd = 0; rd < ndims; ++rd) {
        int d = ndims - rd - 1;

        EXPECT_LE(dims[d], pdims[d]);

        int cur_dim = dims[d];
        int cur_block = md.data.layout_desc.blocking.block_dims[d];

        int cur_pos = optd[d] + (index % cur_dim);
        int cur_pos_block = cur_pos / cur_block;
        int cur_pos_within_block = cur_pos % cur_block;

        ph_index += cur_pos_block*strides_block[d];
        ph_index += cur_pos_within_block*strides_within_block[d];

        index /= cur_dim;
    }

    ph_index += md.data.layout_desc.blocking.offset_padding;

    return ph_index;
}

inline mkldnn::memory::desc create_md(mkldnn::tensor::dims dims,
        mkldnn::memory::precision prec, mkldnn::memory::format fmt) {
    using f = mkldnn::memory::format;
    size_t ndims = 0;

    switch (fmt) {
    case f::x:
        ndims = 1; break;
    case f::nc:
    case f::oi:
        ndims = 2; break;
    case f::nchw:
    case f::nhwc:
    case f::nChw8c:
    case f::oihw:
    case f::OIhw8i8o:
    case f::Ohwi8o:
        ndims = 4; break;
    case f::goihw:
    case f::gOIhw8i8o:
        ndims = 5; break;
    case f::format_undef:
        ndims = 0; break;
    case f::any:
        return mkldnn::memory::desc({dims}, prec, fmt);
    default: EXPECT_TRUE(false) << "test does not support format: " << int(fmt);
    }

    EXPECT_EQ(dims.size(), ndims) << "dims and format are inconsistent";

    return mkldnn::memory::desc({dims}, prec, fmt);
}

template <typename data_t>
static inline data_t set_value(size_t index, data_t mean, data_t deviation,
        double sparsity)
{
    if (data_traits<data_t>::prec == mkldnn::memory::precision::f32) {
        const size_t group_size = (size_t)(1. / sparsity);
        const size_t group = index / group_size;
        const size_t in_group = index % group_size;
        const bool fill = in_group == ((group % 1637) % group_size);
        return fill ? mean + deviation * sin(data_t(index % 37)) : 0;
    } else {
        return data_t(0);
    }
}

template <typename data_t>
static void fill_data(const int size, data_t *data, data_t mean,
        data_t deviation, double sparsity = 1.)
{
#   pragma omp parallel for
    for (int n = 0; n < size; n++) {
        data[n] = set_value<data_t>(n, mean, deviation, sparsity);
    }
}

template <typename data_t>
static void fill_data(const int size, data_t *data, double sparsity = 1.)
{
#   pragma omp parallel for
    for (int n = 0; n < size; n++) {
        data[n] = set_value<data_t>(n, data_t(1), data_t(2e-1), sparsity);
    }
}

template <typename data_t>
static void compare_data(mkldnn::memory& ref, mkldnn::memory& dst)
{
    size_t num = ref.get_primitive_desc().get_number_of_elements();
    data_t *ref_data = (data_t *)ref.get_data_handle();
    data_t *dst_data = (data_t *)dst.get_data_handle();
#   pragma omp parallel for
    for (size_t i = 0; i < num; ++i) {
        data_t ref = ref_data[i];
        data_t got = dst_data[i];
        data_t diff = got - ref;
        data_t e = std::abs(ref) > 1e-4 ? diff / ref : diff;
        EXPECT_NEAR(e, 0.0, 1e-4);
    }
}

struct test_convolution_descr_t {
    int mb;
    int ng;
    int ic, ih, iw;
    int oc, oh, ow;
    int kh, kw;
    int padh, padw;
    int strh, strw;
};

#endif
