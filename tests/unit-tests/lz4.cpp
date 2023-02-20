// tarxx - modern C++ tar library
// Copyright (c) 2022, Thilo Schmitt, Alexander Mohr
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#include "util/util.h"
#include <gtest/gtest.h>
#include <iostream>
#include <tarxx.h>

using std::string_literals::operator""s;

class lz4_tests : public ::testing::TestWithParam<tarxx::tarfile::tar_type> {};

TEST_P(lz4_tests, add_file_success)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    const auto lz4_filename = tar_filename + ".lz4";
    const auto test_file = util::create_test_file(tar_type);
    util::remove_if_exists(tar_filename);
    util::remove_if_exists(lz4_filename);

    tarxx::tarfile f(lz4_filename, tarxx::tarfile::compression_mode::lz4, tar_type);
    f.add_from_filesystem(test_file.path);
    f.close();

    util::decompress_lz4(lz4_filename, tar_filename);
    util::tar_first_files_matches_original(tar_filename, test_file, tar_type);
}

TEST_P(lz4_tests, add_multiple_files_recursive_success)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    const auto lz4_filename = tar_filename + ".lz4";
    auto [dir, test_files] = util::create_multiple_test_files_with_sub_folders(tar_type);
    util::remove_if_exists(tar_filename);
    util::remove_if_exists(lz4_filename);

    tarxx::tarfile tar_file(lz4_filename, tarxx::tarfile::compression_mode::lz4, tar_type);
    tar_file.add_from_filesystem_recursive(dir);
    tar_file.close();

    if (tar_type == tarxx::tarfile::tar_type::ustar) {
        util::append_folders_from_test_files(test_files, tar_type);
    }

    util::decompress_lz4(lz4_filename, tar_filename);
    util::expect_files_in_tar(tar_filename, test_files, tar_type);
    util::remove_if_exists(dir);
}

void lz4_validate_streaming_data(const unsigned int size, const tarxx::tarfile::tar_type& tar_type)
{
    const auto tar_filename = util::tar_file_name();
    const auto lz4_filename = tar_filename + ".lz4";
    util::remove_if_exists(tar_filename);
    util::remove_if_exists(lz4_filename);

    const auto input_data = util::create_input_data(size);
    const auto reference_file = util::create_test_file(tar_type, std::filesystem::temp_directory_path() / "test_file", input_data);

    tarxx::tarfile tar_file(lz4_filename, tarxx::tarfile::compression_mode::lz4, tar_type);
    util::add_streaming_data(input_data, reference_file, tar_file);
    tar_file.close();

    util::decompress_lz4(lz4_filename, tar_filename);
    util::tar_has_one_file_and_matches(tar_filename, reference_file, tar_type);
}

TEST_P(lz4_tests, add_file_stream_data_smaller_than_block_size)
{
    const auto tar_type = GetParam();
    lz4_validate_streaming_data(tarxx::BLOCK_SIZE / 2, tar_type);
}

TEST_P(lz4_tests, add_file_stream_data_two_block_sizes)
{
    const auto tar_type = GetParam();
    lz4_validate_streaming_data(tarxx::BLOCK_SIZE * 2, tar_type);
}

TEST_P(lz4_tests, add_file_stream_data_multi_block)
{
    const auto tar_type = GetParam();
    lz4_validate_streaming_data(tarxx::BLOCK_SIZE * 1.52, tar_type);
}

INSTANTIATE_TEST_SUITE_P(tar_type_dependent, lz4_tests, ::testing::Values(tarxx::tarfile::tar_type::unix_v7, tarxx::tarfile::tar_type::ustar));
