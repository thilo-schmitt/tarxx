// tarxx - modern C++ tar library
// Copyright (c) 2022-2023, Thilo Schmitt, Alexander Mohr
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

    util::append_folders_from_test_files(test_files, tar_type);

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

TEST(lz4_tests, add_directory_via_streaming)
{
    const auto tar_type = tarxx::tarfile::tar_type::ustar;
    const auto tar_filename = util::tar_file_name();
    const auto lz4_filename = tar_filename + ".lz4";
    const auto test_file = util::create_test_file(tar_type);
    util::remove_if_exists(tar_filename);
    util::remove_if_exists(lz4_filename);

    const tarxx::Platform platform;
    tarxx::tarfile f(lz4_filename, tarxx::tarfile::compression_mode::lz4, tar_type);

    std::time_t time = std::time(nullptr);
    const auto user = platform.user_id();
    const auto group = platform.group_id();
    f.add_directory("test_dir", 0755, user, group, time);
    f.close();
    util::decompress_lz4(lz4_filename, tar_filename);

    const auto files = util::files_in_tar_archive(tar_filename);
    EXPECT_EQ(files.size(), 1);
    const auto& file = files.at(0);
    EXPECT_EQ(file.owner, platform.user_name(user));
    EXPECT_EQ(file.group, platform.group_name(group));
    EXPECT_EQ(file.size, 0);
    EXPECT_EQ(file.permissions, "drwxr-xr-x");
}

TEST_P(lz4_tests, add_from_filesystem_file_grows_while_reading)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    const auto lz4_filename = tar_filename + ".lz4";
    util::remove_if_exists(lz4_filename);
    util::remove_if_exists(tar_filename);

    tarxx::tarfile tar_file(lz4_filename, tarxx::tarfile::compression_mode::lz4, tar_type);
    const auto test_file = util::grow_source_file_during_tar_creation(tar_file, tar_type);
    util::decompress_lz4(lz4_filename, tar_filename);
    expect_disk_file_ge_file_in_tar_and_tar_valid(tar_filename, test_file, tar_type);
}

TEST_P(lz4_tests, add_from_filesystem_file_shrinks_while_reading)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    const auto lz4_filename = tar_filename + ".lz4";
    util::remove_if_exists(lz4_filename);
    util::remove_if_exists(tar_filename);

    tarxx::tarfile tar_file(lz4_filename, tarxx::tarfile::compression_mode::lz4, tar_type);
    const auto test_file = util::shrink_source_file_during_tar_creation(tar_file, tar_type);
    util::decompress_lz4(lz4_filename, tar_filename);
    expect_disk_file_le_file_in_tar_and_tar_valid(tar_filename, test_file, tar_type);
}

#if defined(__linux)
TEST(lz4_tests, add_char_special_device_via_streaming)
{
    const auto tar_type = tarxx::tarfile::tar_type::ustar;
    const auto tar_filename = util::tar_file_name();
    const auto lz4_filename = tar_filename + ".lz4";
    tarxx::tarfile f(lz4_filename, tarxx::tarfile::compression_mode::lz4, tar_type);
    util::file_info test_file {
            .path = "/dev/random"};
    util::file_info_set_stat(test_file, tar_type);
    std::vector<util::file_info> expected_files = {
            test_file};

    tarxx::Platform platform;
    const auto owner = platform.file_owner(test_file.path);
    const auto group = platform.file_group(test_file.path);
    tarxx::major_t major;
    tarxx::minor_t minor;
    platform.major_minor(test_file.path, major, minor);
    f.add_character_special_file(test_file.path, test_file.mode, owner, group, test_file.size, test_file.mtime.tv_sec, major, minor);
    f.close();
    util::decompress_lz4(lz4_filename, tar_filename);

    util::expect_files_in_tar(tar_filename, expected_files, tar_type);
}

TEST(lz4_tests, add_fifo_via_streaming)
{
    const auto tar_type = tarxx::tarfile::tar_type::ustar;
    const auto tar_filename = util::tar_file_name();
    const auto lz4_filename = tar_filename + ".lz4";
    util::remove_if_exists(tar_filename);
    util::remove_if_exists(lz4_filename);

    tarxx::tarfile f(lz4_filename, tarxx::tarfile::compression_mode::lz4, tar_type);

    util::file_info test_file {.path = std::filesystem::temp_directory_path() / "fifo"};
    util::remove_if_exists(test_file.path);
    mkfifo(test_file.path.c_str(), 0666);
    util::file_info_set_stat(test_file, tar_type);

    tarxx::Platform platform;
    const auto owner = platform.file_owner(test_file.path);
    const auto group = platform.file_group(test_file.path);

    f.add_fifo(test_file.path, test_file.mode, owner, group, test_file.mtime.tv_sec);
    f.close();
    util::decompress_lz4(lz4_filename, tar_filename);

    std::vector<util::file_info> expected_files = {test_file};
    util::expect_files_in_tar(tar_filename, expected_files, tar_type);
}
#endif

INSTANTIATE_TEST_SUITE_P(tar_type_dependent, lz4_tests, ::testing::Values(tarxx::tarfile::tar_type::unix_v7, tarxx::tarfile::tar_type::ustar));
