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


#include <gtest/gtest.h>
#include <iostream>
#include <tarxx.h>
#include <util/util.h>

using std::string_literals::operator""s;

TEST(tar_tests, add_file_streaming_stream_output_throws)
{
    tarxx::tarfile f([](const tarxx::block_t&, size_t size) {
    });
    EXPECT_THROW(f.add_file_streaming(), std::logic_error);
}

TEST(tar_tests, is_open_returns_fall_stream_mode_callback_nullptr)
{
    tarxx::tarfile f(static_cast<tarxx::tarfile::callback_t>(nullptr));
    EXPECT_FALSE(f.is_open());
}

TEST(tar_tests, add_non_existing_file)
{
    tarxx::tarfile f(std::filesystem::temp_directory_path() / "test.tar");
    EXPECT_THROW(f.add_file("this-file-does-not-exist"), std::runtime_error);
}

TEST(tar_tests, add_file_when_file_is_not_open_throws)
{
    tarxx::tarfile f("");
    EXPECT_THROW(f.add_file("bar"), std::logic_error);
}

TEST(tar_tests, add_file_streaming_when_file_is_not_open_throws)
{
    tarxx::tarfile f("");
    EXPECT_THROW(f.add_file("bar"), std::logic_error);
}

TEST(tar_tests, stream_file_finish_when_file_is_not_open_throws)
{
    tarxx::tarfile f("");
    EXPECT_THROW(f.stream_file_complete("", 666, 0, 0, 0, 0), std::logic_error);
}

TEST(tar_tests, add_file_streaming_data_when_file_is_not_open_throws)
{
    tarxx::tarfile f("");
    EXPECT_THROW(f.add_file_streaming_data("a", 1), std::logic_error);
}

TEST(tar_tests, double_close)
{
    tarxx::tarfile f("");
    f.close();
    f.close();
}

TEST(tar_tests, close_file)
{
    tarxx::tarfile f(std::filesystem::temp_directory_path() / "test.tar");
    EXPECT_TRUE(f.is_open());
    f.close();
    EXPECT_FALSE(f.is_open());
}

TEST(tar_tests, close_stream)
{
    tarxx::tarfile f([](const tarxx::block_t&, size_t size) {});
    EXPECT_TRUE(f.is_open());
    f.close();
    EXPECT_FALSE(f.is_open());
}


TEST(tar_tests, close_on_destruct)
{
    bool write_called = false;
    {
        tarxx::tarfile f([&](const tarxx::block_t&, size_t size) {
            write_called = true;
        });
    }

    EXPECT_TRUE(write_called);
}

TEST(tar_tests, finish_on_close)
{
    const auto expected_writes = 2;
    auto writes = 0;
    auto expected_size = 2 * tarxx::BLOCK_SIZE;
    auto written_size = 0UL;
    tarxx::tarfile f([&](const tarxx::block_t&, size_t size) {
        ++writes;
        written_size += size;
    });

    f.close();
    EXPECT_EQ(expected_size, written_size);
    EXPECT_EQ(expected_writes, writes);
}

TEST(tar_tests, add_file_with_stream_file_throws)
{
    tarxx::tarfile f(std::filesystem::temp_directory_path() / "test.tar");
    f.add_file_streaming();
    EXPECT_THROW(f.add_file("foobar");, std::logic_error);
}

TEST(tar_tests, add_file_streaming_twice_throws)
{
    tarxx::tarfile f(std::filesystem::temp_directory_path() / "test.tar");
    f.add_file_streaming();
    EXPECT_THROW(f.add_file_streaming();, std::logic_error);
}

TEST(tar_tests, add_file_streaming_with_stream_output_throws)
{
    tarxx::tarfile f([&](const tarxx::block_t&, size_t size) {
    });
    EXPECT_THROW(f.add_file_streaming();, std::logic_error);
}

TEST(tar_tests, add_file_success)
{
    const auto tar_filename = std::filesystem::temp_directory_path() / "test.tar";
    const auto test_file = util::create_test_file();
    util::remove_file_if_exists(tar_filename);

    tarxx::tarfile f(tar_filename);
    f.add_file(test_file.path);
    f.close();

    const auto files = util::files_in_tar_archive(tar_filename);
    EXPECT_EQ(files.size(), 1);
    util::file_from_tar_matches_original_file(test_file, files.at(0));
}

TEST(tar_tests, add_multiple_files_recursive_success)
{
    const auto tar_filename = std::filesystem::temp_directory_path() / "test.tar";
    const auto [dir, test_files] = util::create_multiple_test_files_with_sub_folders();
    util::remove_file_if_exists(tar_filename);

    tarxx::tarfile tar_file(tar_filename);
    tar_file.add_files_recursive(dir);
    tar_file.close();

    util::expect_files_in_tar(tar_filename, test_files);

    std::filesystem::remove_all(dir);
}

void tar_validate_streaming_data(const unsigned int size)
{
    const auto tar_filename = std::filesystem::temp_directory_path() / "test.tar"s;
    const auto input_data = util::create_input_data(size);
    const auto test_file = util::create_test_file(std::filesystem::temp_directory_path() / "test_file", input_data);
    util::remove_file_if_exists(tar_filename);

    tarxx::tarfile tar_file(tar_filename);
    util::add_streaming_data(input_data, test_file, tar_file);
    tar_file.close();

    util::tar_has_one_file_and_matches(tar_filename, test_file);
}

TEST(tar_tests, add_file_stream_data_smaller_than_block_size)
{
    tar_validate_streaming_data(tarxx::BLOCK_SIZE / 2);
}

TEST(tar_tests, add_file_stream_data_two_block_sizes)
{
    tar_validate_streaming_data(tarxx::BLOCK_SIZE * 2);
}

TEST(tar_tests, add_file_stream_data_multi_block)
{
    tar_validate_streaming_data(tarxx::BLOCK_SIZE * 1.42);
}
