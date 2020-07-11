// Copyright (c) Team CharLS.
// SPDX-License-Identifier: BSD-3-Clause

#include "pch.h"

#include "../src/decoder_strategy.h"

#include "encoder_strategy_tester.h"

#include <array>

using Microsoft::VisualStudio::CppUnitTestFramework::Assert;
using std::array;
using std::unique_ptr;

namespace {

class decoder_strategy_tester final : public charls::decoder_strategy
{
public:
    decoder_strategy_tester(const charls::frame_info& frame_info, const charls::coding_parameters& parameters, uint8_t* const destination, const size_t count) : // NOLINT
        decoder_strategy(frame_info, parameters)
    {
        byte_stream_info stream{nullptr, destination, count};
        initialize(stream);
    }

    void set_presets(const charls::jpegls_pc_parameters& /*preset_coding_parameters*/) noexcept(false) override
    {
    }

    unique_ptr<charls::process_line> create_process(byte_stream_info /*rawStreamInfo*/, uint32_t /*stride*/) noexcept(false) override
    {
        return nullptr;
    }

    void decode_scan(unique_ptr<charls::process_line> /*outputData*/, const JlsRect& /*size*/, byte_stream_info& /*compressedData*/) noexcept(false) override
    {
    }

    int32_t read(const int32_t length)
    {
        return read_long_value(length);
    }
};

} // namespace


namespace charls {
namespace test {

// clang-format off

TEST_CLASS(decoder_strategy_test)
{
public:
    TEST_METHOD(decode_encoded_ff_pattern) // NOLINT
    {
        struct data_t final
        {
            int32_t value;
            int bits;
        };

        const array<data_t, 5> inData{{{0x00, 24}, {0xFF, 8}, {0xFFFF, 16 }, {0xFFFF, 16 }, {0x12345678, 31}}};

        array<uint8_t, 100> encBuf{};
        const charls::frame_info frame_info{};
        const charls::coding_parameters parameters{};

        encoder_strategy_tester encoder(frame_info, parameters);

        byte_stream_info stream{nullptr, encBuf.data(), encBuf.size()};
        encoder.initialize_forward(stream);

        for (const auto& data : inData)
        {
            encoder.append_to_bit_stream_forward(data.value, data.bits);
        }

        encoder.end_scan_forward();
        // Note: Correct encoding is tested in encoder_strategy_test::append_to_bit_stream_ff_pattern.

        const auto length = encoder.get_length_forward();
        decoder_strategy_tester dec(frame_info, parameters, encBuf.data(), length);
        for (auto i = 0U; i < sizeof(inData) / sizeof(inData[0]); ++i)
        {
            const auto actual = dec.read(inData[i].bits);
            Assert::AreEqual(inData[i].value, actual);
        }
    }
};

} // namespace test
} // namespace charls
