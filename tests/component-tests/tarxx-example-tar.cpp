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

TEST(tarxx_example, no_arguments)
{
    EXPECT_NE(util::execute(TARXX_EXAMPLE_BINARY_PATH), 0);
}

TEST(tarxx_example, unknown_arguments)
{
    std::string out;
    std::stringstream cmd;
    EXPECT_NE(util::execute(const_cast<char*>(TARXX_EXAMPLE_BINARY_PATH), const_cast<char*>("-q")), 0);
}

TEST(tarxx_example, create_argument_only)
{
    std::string out;
    std::stringstream cmd;
    EXPECT_NE(util::execute(const_cast<char*>(TARXX_EXAMPLE_BINARY_PATH), const_cast<char*>("-c")), 0);
}

TEST(tarxx_example, from_file_to_file)
{
    const auto [dir, test_files] = util::create_multiple_test_files_with_sub_folders();
    EXPECT_EQ(test_files.size(), 2);

    const auto tar_filename = (std::filesystem::temp_directory_path() / "test.tar").string();
    util::remove_file_if_exists(tar_filename);

    util::execute(TARXX_EXAMPLE_BINARY_PATH,
                  const_cast<char*>("-cf"),
                  const_cast<char*>(tar_filename.c_str()),
                  const_cast<char*>(test_files.at(0).path.c_str()),
                  const_cast<char*>(test_files.at(1).path.c_str()));

    util::expect_files_in_tar(tar_filename, test_files);
    std::filesystem::remove_all(dir);
}

TEST(tarxx_example, from_stream_to_file)
{
    const auto test_file = util::create_test_file();

    const auto tar_filename = std::filesystem::temp_directory_path() / "test.tar";
    util::remove_file_if_exists(tar_filename);


    util::execute(const_cast<char*>(TARXX_EXAMPLE_BINARY_PATH),
                  const_cast<char*>("-cf"),
                  const_cast<char*>(tar_filename.c_str()),
                  const_cast<char*>(test_file.path.c_str()));

    util::expect_files_in_tar(tar_filename, {test_file});
    std::filesystem::remove(test_file.path);
}

TEST(tarxx_example, from_file_to_stream)
{
    const auto [dir, test_files] = util::create_multiple_test_files_with_sub_folders();
    const auto test_files_str = util::test_files_as_str(test_files);

    const auto tar_filename = std::filesystem::temp_directory_path() / "test.tar";
    util::remove_file_if_exists(tar_filename);

    std::stringstream cmd;
    cmd << TARXX_EXAMPLE_BINARY_PATH<< " -c " << test_files_str << " > " << tar_filename.string();
    util::execute("bash",
                  const_cast<char*>("-c"),
                  const_cast<char*>(cmd.str().c_str()));

    util::expect_files_in_tar(tar_filename, test_files);
    std::filesystem::remove_all(dir);
}

TEST(tarxx_example, from_stream_to_stream)
{
    const auto test_file = util::create_test_file();
    const auto test_file_str = util::test_files_as_str({test_file});

    const auto tar_filename = std::filesystem::temp_directory_path() / "test.tar";
    util::remove_file_if_exists(tar_filename);

    std::stringstream cmd;
    cmd << "cat " << test_file_str << "|"
        << TARXX_EXAMPLE_BINARY_PATH << " -c "
        << " > " << tar_filename.string();
    util::execute("bash",
                  const_cast<char*>("-c"),
                  const_cast<char*>(cmd.str().c_str()));

    util::expect_files_in_tar(tar_filename, {});
    std::filesystem::remove(test_file.path);
}
