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

#include <filesystem>
#include <gtest/gtest.h>
#include <sstream>
#include <util/util.h>

class tarxx_lz4_example : public ::testing::TestWithParam<tarxx::tarfile::tar_type> {};


TEST_P(tarxx_lz4_example, from_file_to_file)
{
    const auto tar_type = GetParam();
    const auto [dir, test_files] = util::create_multiple_test_files_with_sub_folders(tar_type);
    EXPECT_EQ(test_files.size(), 2);

    const auto tar_filename = (std::filesystem::temp_directory_path() / "test.tar").string();
    const auto lz4_filename = tar_filename + ".lz4";
    util::remove_file_if_exists(tar_filename);
    util::remove_file_if_exists(lz4_filename);

    const auto type_str = std::to_string(static_cast<int>(tar_type));
    util::execute(TARXX_EXAMPLE_BINARY_PATH,
                  const_cast<char*>("-ckt"),
                  const_cast<char*>(type_str.c_str()),
                  const_cast<char*>("-f"),
                  const_cast<char*>(lz4_filename.c_str()),
                  const_cast<char*>(test_files.at(0).path.c_str()),
                  const_cast<char*>(test_files.at(1).path.c_str()));

    util::decompress_lz4(lz4_filename, tar_filename);
    util::expect_files_in_tar(tar_filename, test_files, tar_type);
    std::filesystem::remove_all(dir);
}

TEST_P(tarxx_lz4_example, from_stream_to_file)
{
    const auto tar_type = GetParam();
    const auto test_file = util::create_test_file(tar_type);

    const auto tar_filename = std::filesystem::temp_directory_path() / "test.tar";
    const auto lz4_filename = tar_filename.string() + ".lz4";
    util::remove_file_if_exists(tar_filename);
    util::remove_file_if_exists(lz4_filename);

    const auto type_str = std::to_string(static_cast<int>(tar_type));
    util::execute(const_cast<char*>(TARXX_EXAMPLE_BINARY_PATH),
                  const_cast<char*>("-ckt"),
                  const_cast<char*>(type_str.c_str()),
                  const_cast<char*>("-f"),
                  const_cast<char*>(lz4_filename.c_str()),
                  const_cast<char*>(test_file.path.c_str()));

    util::decompress_lz4(lz4_filename, tar_filename);
    util::expect_files_in_tar(tar_filename, {test_file}, tar_type);
    std::filesystem::remove(test_file.path);
}

TEST_P(tarxx_lz4_example, from_file_to_stream)
{
    const auto tar_type = GetParam();
    const auto [dir, test_files] = util::create_multiple_test_files_with_sub_folders(tar_type);
    const auto test_files_str = util::test_files_as_str(test_files);

    const auto tar_filename = std::filesystem::temp_directory_path() / "test.tar";
    const auto lz4_filename = tar_filename.string() + ".lz4";
    util::remove_file_if_exists(tar_filename);
    util::remove_file_if_exists(lz4_filename);

    std::stringstream cmd;
    cmd << TARXX_EXAMPLE_BINARY_PATH << " -t " << static_cast<int>(tar_type) << " -kc " << test_files_str << " > " << lz4_filename;
    util::execute("bash", const_cast<char*>("-c"), const_cast<char*>(cmd.str().c_str()));

    util::decompress_lz4(lz4_filename, tar_filename);
    util::expect_files_in_tar(tar_filename, test_files, tar_type);
    std::filesystem::remove_all(dir);
}

TEST_P(tarxx_lz4_example, from_stream_to_stream)
{
    const auto tar_type = GetParam();
    const auto test_file = util::create_test_file(tar_type);
    const auto test_file_str = util::test_files_as_str({test_file});

    const auto tar_filename = std::filesystem::temp_directory_path() / "test.tar";
    const auto lz4_filename = tar_filename.string() + ".lz4";
    util::remove_file_if_exists(tar_filename);
    util::remove_file_if_exists(lz4_filename);

    std::stringstream cmd;
    cmd
            << "cat " << test_file_str << "|"
            << TARXX_EXAMPLE_BINARY_PATH
            << " -t " << static_cast<int>(tar_type)
            << " -kc "
            << " > " << lz4_filename;
    util::execute("bash", const_cast<char*>("-c"), const_cast<char*>(cmd.str().c_str()));

    util::decompress_lz4(lz4_filename, tar_filename);
    util::expect_files_in_tar(tar_filename, {}, tar_type);
    std::filesystem::remove(test_file.path);
}

INSTANTIATE_TEST_SUITE_P(tar_type_dependent, tarxx_lz4_example, ::testing::Values(tarxx::tarfile::tar_type::unix_v7, tarxx::tarfile::tar_type::ustar));
