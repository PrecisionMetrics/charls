// Copyright (c) Team CharLS.
// SPDX-License-Identifier: BSD-3-Clause

#include "util.h"

#include "portable_anymap_file.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

using std::cout;
using std::error_code;
using std::ifstream;
using std::milli;
using std::setprecision;
using std::setw;
using std::swap;
using std::vector;
using std::chrono::duration;
using std::chrono::steady_clock;
using std::ios;
using namespace charls;
using namespace charls_test;


namespace {

MSVC_WARNING_SUPPRESS(26497) // cannot be marked constexpr, check must be executed at runtime.

bool is_machine_little_endian() noexcept
{
    constexpr int a = 0xFF000001;  // NOLINT(bugprone-narrowing-conversions, cppcoreguidelines-narrowing-conversions)
    const auto* chars = reinterpret_cast<const char*>(&a);
    return chars[0] == 0x01;
}

MSVC_WARNING_UNSUPPRESS()


} // namespace


void fix_endian(vector<uint8_t>* buffer, const bool little_endian_data) noexcept
{
    if (little_endian_data == is_machine_little_endian())
        return;

    for (size_t i = 0; i < buffer->size() - 1; i += 2)
    {
        swap((*buffer)[i], (*buffer)[i + 1]);
    }
}


vector<uint8_t> read_file(const char* filename, long offset, size_t bytes)
{
    ifstream input;
    input.exceptions(ios::eofbit | ios::failbit | ios::badbit);
    input.open(filename, ios::in | ios::binary);

    input.seekg(0, ios::end);
    const auto byte_count_file = static_cast<int>(input.tellg());
    input.seekg(offset, ios::beg);

    if (offset < 0)
    {
        assert::is_true(bytes != 0);
        offset = static_cast<long>(byte_count_file - bytes);
    }
    if (bytes == 0)
    {
        bytes = static_cast<size_t>(byte_count_file) - offset;
    }

    vector<uint8_t> buffer(bytes);
    input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());

    return buffer;
}


void test_round_trip(const char* name, const vector<uint8_t>& decoded_buffer, const rect_size size, const int bits_per_sample, const int component_count, const int loop_count)
{
    JlsParameters params = JlsParameters();
    params.components = component_count;
    params.bitsPerSample = bits_per_sample;
    params.height = static_cast<int>(size.cy);
    params.width = static_cast<int>(size.cx);

    test_round_trip(name, decoded_buffer, params, loop_count);
}


void test_round_trip(const char* name, const vector<uint8_t>& original_buffer, JlsParameters& params, const int loop_count)
{
    vector<uint8_t> encoded_buffer(params.height * params.width * params.components * params.bitsPerSample / 4);

    vector<uint8_t> decoded_buffer(static_cast<size_t>(params.height) * params.width * ((params.bitsPerSample + 7) / 8) * params.components);

    if (params.components == 4)
    {
        params.interleaveMode = interleave_mode::line;
    }
    else if (params.components == 3)
    {
        params.interleaveMode = interleave_mode::line;
        params.colorTransformation = color_transformation::hp1;
    }

    size_t encoded_actual_size{};
    auto start = steady_clock::now();
    for (int i = 0; i < loop_count; ++i)
    {
        const error_code error = JpegLsEncode(encoded_buffer.data(), encoded_buffer.size(), &encoded_actual_size,
                                              original_buffer.data(), original_buffer.size(), &params, nullptr);
        assert::is_true(!error);
    }

    const auto total_encode_duration = steady_clock::now() - start;

    start = steady_clock::now();
    for (int i = 0; i < loop_count; ++i)
    {
        const error_code error = JpegLsDecode(decoded_buffer.data(), decoded_buffer.size(), encoded_buffer.data(), encoded_actual_size, nullptr, nullptr);
        assert::is_true(!error);
    }

    const auto total_decode_duration = steady_clock::now() - start;

    const double bits_per_sample = 1.0 * static_cast<double>(encoded_actual_size) * 8. / (static_cast<double>(params.components) * params.height * params.width);
    cout << "RoundTrip test for: " << name << "\n\r";
    const double encode_time = duration<double, milli>(total_encode_duration).count() / loop_count;
    const double decode_time = duration<double, milli>(total_decode_duration).count() / loop_count;
    const double symbol_rate = (static_cast<double>(params.components) * params.height * params.width) / (1000.0 * decode_time);

    cout << "Size:" << setw(10) << params.width << "x" << params.height << setw(7) << setprecision(2) << ", Encode time:" << encode_time << " ms, Decode time:" << decode_time << " ms, Bits per sample:" << bits_per_sample << ", Decode rate:" << symbol_rate << " M/s\n";

    const uint8_t* byte_out = decoded_buffer.data();
    for (size_t i = 0; i < decoded_buffer.size(); ++i)
    {
        if (original_buffer[i] != byte_out[i])
        {
            assert::is_true(false);
            break;
        }
    }
}


void test_file(const char* filename, const int offset, const rect_size size2, const int bits_per_sample, const int component_count, const bool little_endian_file, const int loop_count)
{
    const size_t byte_count = size2.cx * size2.cy * component_count * ((bits_per_sample + 7) / 8);
    vector<uint8_t> uncompressed_buffer = read_file(filename, offset, byte_count);

    if (bits_per_sample > 8)
    {
        fix_endian(&uncompressed_buffer, little_endian_file);
    }

    test_round_trip(filename, uncompressed_buffer, size2, bits_per_sample, component_count, loop_count);
}


void test_portable_anymap_file(const char* filename, const int loop_count)
{
    portable_anymap_file anymap_file(filename);

    test_round_trip(filename, anymap_file.image_data(), rect_size(anymap_file.width(), anymap_file.height()),
                  anymap_file.bits_per_sample(), anymap_file.component_count(), loop_count);
}
