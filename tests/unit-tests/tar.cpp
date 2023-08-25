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


#include <gtest/gtest.h>
#include <iostream>
#include <tarxx.h>
#include <util/util.h>

#if defined(__linux)
#    include <sys/socket.h>
#    include <sys/un.h>
#    include <thread>

#endif

using std::string_literals::operator""s;

class tar_tests : public ::testing::TestWithParam<tarxx::tarfile::tar_type> {};

TEST_P(tar_tests, add_file_streaming_stream_output_throws)
{
    const auto tar_type = GetParam();
    tarxx::tarfile f([](const tarxx::block_t&, size_t size) {
    },
                     tar_type);
    EXPECT_THROW(f.add_file_streaming(), std::logic_error);
}

TEST_P(tar_tests, is_open_returns_fall_stream_mode_callback_nullptr)
{
    const auto tar_type = GetParam();
    tarxx::tarfile f(static_cast<tarxx::tarfile::callback_t>(nullptr), tar_type);
    EXPECT_FALSE(f.is_open());
}

TEST_P(tar_tests, add_non_existing_file)
{
    const auto tar_type = GetParam();
    tarxx::tarfile f(util::tar_file_name(), tar_type);
    EXPECT_THROW(f.add_from_filesystem("this-file-does-not-exist"), std::invalid_argument);
}

TEST_P(tar_tests, add_from_filesystem_when_file_is_not_open_throws)
{
    const auto tar_type = GetParam();
    tarxx::tarfile f("", tar_type);
    EXPECT_THROW(f.add_from_filesystem("bar"), std::logic_error);
}

TEST_P(tar_tests, add_file_streaming_when_file_is_not_open_throws)
{
    const auto tar_type = GetParam();
    tarxx::tarfile f("", tar_type);
    EXPECT_THROW(f.add_from_filesystem("bar"), std::logic_error);
}

TEST_P(tar_tests, stream_file_finish_when_file_is_not_open_throws)
{
    const auto tar_type = GetParam();
    tarxx::tarfile f("", tar_type);
    EXPECT_THROW(f.stream_file_complete("", 666, 0, 0, 0, 0), std::logic_error);
}

TEST_P(tar_tests, add_file_streaming_data_when_file_is_not_open_throws)
{
    const auto tar_type = GetParam();
    tarxx::tarfile f("", tar_type);
    EXPECT_THROW(f.add_file_streaming_data("a", 1), std::logic_error);
}

TEST_P(tar_tests, add_file_streaming_data_before_adding_file)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    tarxx::tarfile f(tar_filename, tar_type);
    EXPECT_THROW(f.add_file_streaming_data("a", 1), std::logic_error);
}

TEST_P(tar_tests, add_directory_while_file_streaming_in_progress)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    tarxx::tarfile f(tar_filename, tar_type);
    f.add_file_streaming();
    EXPECT_THROW(f.add_directory("foobar", 0, 0, 0, 0), std::logic_error);
}

TEST_P(tar_tests, add_link_while_file_streaming_in_progress)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    tarxx::tarfile f(tar_filename, tar_type);
    f.add_file_streaming();
    EXPECT_THROW(f.add_symlink("foobar", "link", 0, 0, 0), std::logic_error);
}

TEST(tar_tests, add_block_device_while_file_streaming_in_progress)
{
    const auto tar_type = tarxx::tarfile::tar_type::ustar;
    const auto tar_filename = util::tar_file_name();
    tarxx::tarfile f(tar_filename, tar_type);
    f.add_file_streaming();
    EXPECT_THROW(f.add_block_special_file("foobar", 0, 0, 0, 0, 0, 0, 0), std::logic_error);
}

TEST(tar_tests, add_char_device_while_file_streaming_in_progress)
{
    const auto tar_type = tarxx::tarfile::tar_type::ustar;
    const auto tar_filename = util::tar_file_name();
    tarxx::tarfile f(tar_filename, tar_type);
    f.add_file_streaming();
    EXPECT_THROW(f.add_block_special_file("foobar", 0, 0, 0, 0, 0, 0, 0), std::logic_error);
}

TEST(tar_tests, add_fifo_while_file_streaming_in_progress)
{
    const auto tar_type = tarxx::tarfile::tar_type::ustar;
    const auto tar_filename = util::tar_file_name();
    tarxx::tarfile f(tar_filename, tar_type);
    f.add_file_streaming();
    EXPECT_THROW(f.add_fifo("foobar", 0, 0, 0, 0), std::logic_error);
}

TEST_P(tar_tests, double_close)
{
    const auto tar_type = GetParam();
    tarxx::tarfile f("", tar_type);
    f.close();
    f.close();
}

TEST_P(tar_tests, close_file)
{
    const auto tar_type = GetParam();
    tarxx::tarfile f(util::tar_file_name(), tar_type);
    EXPECT_TRUE(f.is_open());
    f.close();
    EXPECT_FALSE(f.is_open());
}

TEST_P(tar_tests, close_stream)
{
    const auto tar_type = GetParam();
    tarxx::tarfile f([](const tarxx::block_t&, size_t size) {}, tar_type);
    EXPECT_TRUE(f.is_open());
    f.close();
    EXPECT_FALSE(f.is_open());
}


TEST_P(tar_tests, close_on_destruct)
{
    const auto tar_type = GetParam();
    bool write_called = false;
    {
        tarxx::tarfile f([&](const tarxx::block_t&, size_t size) {
            write_called = true;
        },
                         tar_type);
    }

    EXPECT_TRUE(write_called);
}

TEST_P(tar_tests, finish_on_close)
{
    const auto tar_type = GetParam();
    const auto expected_writes = 2;
    auto writes = 0;
    auto expected_size = 2 * tarxx::BLOCK_SIZE;
    auto written_size = 0UL;
    tarxx::tarfile f([&](const tarxx::block_t&, size_t size) {
        ++writes;
        written_size += size;
    },
                     tar_type);

    f.close();
    EXPECT_EQ(expected_size, written_size);
    EXPECT_EQ(expected_writes, writes);
}

TEST_P(tar_tests, add_from_filesystem_with_stream_file_throws)
{
    const auto tar_type = GetParam();
    tarxx::tarfile f(util::tar_file_name(), tar_type);
    f.add_file_streaming();
    EXPECT_THROW(f.add_from_filesystem("foobar");, std::logic_error);
}

TEST_P(tar_tests, add_file_streaming_twice_throws)
{
    const auto tar_type = GetParam();
    tarxx::tarfile f(util::tar_file_name(), tar_type);
    f.add_file_streaming();
    EXPECT_THROW(f.add_file_streaming();, std::logic_error);
}

TEST_P(tar_tests, add_file_streaming_with_stream_output_throws)
{
    const auto tar_type = GetParam();
    tarxx::tarfile f([&](const tarxx::block_t&, size_t size) {
    },
                     tar_type);
    EXPECT_THROW(f.add_file_streaming();, std::logic_error);
}

TEST_P(tar_tests, add_from_filesystem_success)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    const auto test_file = util::create_test_file(tar_type);
    util::remove_if_exists(tar_filename);

    tarxx::tarfile f(tar_filename, tar_type);
    f.add_from_filesystem(test_file.path);
    f.close();

    const auto files = util::files_in_tar_archive(tar_filename);
    EXPECT_EQ(files.size(), 1);
    util::file_from_tar_matches_original_file(test_file, files.at(0), tar_type);
}

TEST_P(tar_tests, add_multiple_files_recursive_success)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    auto [dir, test_files] = util::create_multiple_test_files_with_sub_folders(tar_type);
    util::remove_if_exists(tar_filename);

    tarxx::tarfile tar_file(tar_filename, tar_type);
    tar_file.add_from_filesystem_recursive(dir);
    tar_file.close();

    util::append_folders_from_test_files(test_files, tar_type);

    util::expect_files_in_tar(tar_filename, test_files, tar_type);
    std::filesystem::remove_all(dir);
}

TEST_P(tar_tests, add_multiple_files_recursive_new_name)
{
    std::vector<std::string> new_names = {
            "new_root",
            "new_root/",
            "/new_root/",
            "/new_root",
            "new_root/with_subfolder",
            "new_root/with_subfolder/"};

    for (auto& new_name : new_names) {
        const auto tar_type = GetParam();
        const auto tar_filename = util::tar_file_name();
        auto [dir, test_files] = util::create_multiple_test_files_with_sub_folders(tar_type);
        util::remove_if_exists(tar_filename);

        tarxx::tarfile tar_file(tar_filename, tar_type);
        tar_file.add_from_filesystem_recursive(dir, new_name);
        tar_file.close();

        util::append_folders_from_test_files(test_files, tar_type);

        for (auto& file : test_files) {
            if (new_name.rfind("/") == new_name.size() - 1) {
                new_name = new_name.substr(0, new_name.size() - 1);
            }
            file.path.replace(file.path.begin(), file.path.begin() + dir.string().size(), new_name);
        }
        util::expect_files_in_tar(tar_filename, test_files, tar_type);
        std::filesystem::remove_all(dir);
    }
}

void tar_validate_streaming_data(const unsigned int size, const tarxx::tarfile::tar_type& tar_type)
{
    const auto tar_filename = util::tar_file_name();
    const auto input_data = util::create_input_data(size);
    const auto test_file = util::create_test_file(tar_type, std::filesystem::temp_directory_path() / "test_file", input_data);
    util::remove_if_exists(tar_filename);

    tarxx::tarfile tar_file(tar_filename, tar_type);
    util::add_streaming_data(input_data, test_file, tar_file);
    tar_file.close();

    util::tar_has_one_file_and_matches(tar_filename, test_file, tar_type);
}

TEST_P(tar_tests, add_from_filesystem_different_name)
{
    const std::vector<std::string> new_names = {
            "this-is-a-new-name",
            "tmp/new-name"};

    for (const auto& new_name : new_names) {
        const auto tar_type = GetParam();
        const auto tar_filename = util::tar_file_name();
        const auto test_file = util::create_test_file(tar_type);
        util::remove_if_exists(tar_filename);

        tarxx::tarfile f(tar_filename, tar_type);
        f.add_from_filesystem(test_file.path, new_name);
        f.close();

        const auto files = util::files_in_tar_archive(tar_filename);
        EXPECT_EQ(files.size(), 1);
        auto expected_file = files.at(0);
        expected_file.path = new_name;
        util::file_from_tar_matches_original_file(expected_file, files.at(0), tar_type);
    }
}

#if defined(__linux)
TEST_P(tar_tests, add_from_filesystem_access_denied)
{
    const tarxx::Platform platform;
    if (platform.user_id() == 0) {
        return;
    }

    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    const auto test_file0 = util::create_test_file(tar_type, std::filesystem::temp_directory_path() / "test2");
    const auto test_file1 = util::create_test_file(tar_type);
    ASSERT_EQ(std::system(("chmod a-r " + test_file0.path).c_str()), 0);

    util::remove_if_exists(tar_filename);

    tarxx::tarfile f(tar_filename, tar_type);
    EXPECT_THROW(f.add_from_filesystem(test_file0.path), std::exception);

    f.add_from_filesystem(test_file1.path);
    f.close();

    const auto files = util::files_in_tar_archive(tar_filename);
    EXPECT_EQ(files.size(), 1);
    util::file_from_tar_matches_original_file(test_file1, files.at(0), tar_type);

    ASSERT_EQ(std::system(("chmod a+r " + test_file0.path).c_str()), 0);
    util::remove_if_exists(tar_filename);
    util::remove_if_exists(test_file0.path);
    util::remove_if_exists(test_file1.path);
}

TEST_P(tar_tests, add_from_filesystem_access_denied_stream_output)
{
    tarxx::Platform platform;
    if (platform.user_id() == 0) {
        return;
    }
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    const auto test_file0 = util::create_test_file(tar_type, std::filesystem::temp_directory_path() / "test2");
    const auto test_file1 = util::create_test_file(tar_type);
    ASSERT_EQ(std::system(("chmod a-r " + test_file0.path).c_str()), 0);

    util::remove_if_exists(tar_filename);

    std::ofstream ofs(tar_filename, std::ios::binary);
    tarxx::tarfile f([&ofs](const tarxx::block_t& block, const size_t size) {
        ofs.write(block.data(), size);
    },
                     tar_type);

    EXPECT_THROW(f.add_from_filesystem(test_file0.path), std::exception);
    f.add_from_filesystem(test_file1.path);
    f.close();
    ofs.close();

    const auto files = util::files_in_tar_archive(tar_filename);
    EXPECT_EQ(files.size(), 1);
    util::file_from_tar_matches_original_file(test_file1, files.at(0), tar_type);

    ASSERT_EQ(std::system(("chmod a+r " + test_file0.path).c_str()), 0);
    util::remove_if_exists(tar_filename);
    util::remove_if_exists(test_file0.path);
    util::remove_if_exists(test_file1.path);
}
#endif

TEST_P(tar_tests, add_from_filesystem_different_invalid_name)
{
    const std::vector<std::string> new_names = {
            "..",
            "not/../good",
            "",
            "also/not/good/..",
            "tmp/false-directory/"};

    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    const auto test_file = util::create_test_file(tar_type);
    util::remove_if_exists(tar_filename);

    tarxx::tarfile f(tar_filename, tar_type);
    for (const auto& new_name : new_names) {
        EXPECT_THROW(f.add_from_filesystem(test_file.path, new_name), std::invalid_argument);
    }
}

TEST_P(tar_tests, add_empty_block_via_streaming)
{
    const auto tar_type = GetParam();
    const auto size = 42;
    const auto tar_filename = util::tar_file_name();
    const auto input_data = util::create_input_data(size);
    auto test_file = util::create_test_file(tar_type, std::filesystem::temp_directory_path() / "test_file0", input_data);
    const tarxx::Platform platform;
    util::remove_if_exists(tar_filename);

    tarxx::tarfile tar_file(tar_filename, tar_type);

    tar_file.add_file_streaming();
    tar_file.add_file_streaming_data("", 0);
    tar_file.stream_file_complete(test_file.path, test_file.mode, platform.user_id(), platform.group_id(), 0, test_file.mtime.tv_sec);

    test_file.size = 0;
    util::tar_has_one_file_and_matches(tar_filename, test_file, tar_type);

    tar_file.close();
}

TEST_P(tar_tests, add_from_filesystem_stream_data_smaller_than_block_size)
{
    const auto tar_type = GetParam();
    tar_validate_streaming_data(tarxx::BLOCK_SIZE / 2, tar_type);
}

TEST_P(tar_tests, add_from_filesystem_stream_data_two_block_sizes)
{
    const auto tar_type = GetParam();
    tar_validate_streaming_data(tarxx::BLOCK_SIZE * 2, tar_type);
}

TEST_P(tar_tests, add_from_filesystem_stream_data_multi_block)
{
    const auto tar_type = GetParam();
    tar_validate_streaming_data(tarxx::BLOCK_SIZE * 1.42, tar_type);
}

TEST_P(tar_tests, add_from_filesystem_success_long_name)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    auto file_name = "this_file_name_has_100_chars_which_is_the_limit_of_tar_v7"s;
    while ((std::filesystem::temp_directory_path() / file_name).string().length() < 99) {
        file_name += "x";
    }
    file_name += "y";
    ASSERT_EQ((std::filesystem::temp_directory_path() / file_name).string().length(), 100);

    const auto test_file = util::create_test_file(tar_type, std::filesystem::temp_directory_path() / file_name);
    util::remove_if_exists(tar_filename);

    tarxx::tarfile f(tar_filename, tar_type);
    f.add_from_filesystem(test_file.path);
    f.close();

    util::expect_files_in_tar(tar_filename, {test_file}, tar_type);
}

TEST_P(tar_tests, add_from_filesystems_relative_path)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();

    auto [dir, test_files] = util::create_multiple_test_files_with_sub_folders(tar_type);
    std::filesystem::current_path(dir);
    util::remove_if_exists(tar_filename);

    tarxx::tarfile tar_file(tar_filename, tar_type);

    const auto path = dir.string() + "/";
    for (auto& f : test_files) {
        auto name_without_root_path = f.path;
        name_without_root_path.replace(name_without_root_path.find(path), path.size(), "");

        const auto name = name_without_root_path;
        tar_file.add_from_filesystem(name);
        f.path = name_without_root_path;
    }

    tar_file.close();

    util::expect_files_in_tar(tar_filename, test_files, tar_type);

    std::filesystem::remove_all(dir);
}

TEST_P(tar_tests, add_symlink_from_filesystem)
{
    if (util::tar_version() != util::tar_version::gnu) {
        // Only GNU tar shows links in tar -tvf
        return;
    }

    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();

    const auto file_name = "test_file";
    const auto test_file = util::create_test_file(tar_type, std::filesystem::temp_directory_path() / file_name);
    const auto link_location = std::filesystem::temp_directory_path() / "symlink_to_file";

    util::remove_if_exists(tar_filename);
    util::remove_if_exists(link_location);

    std::filesystem::create_symlink(test_file.path, link_location);
    util::file_info link_test_file {
            .path = link_location,
            .is_symlink = true};
    util::file_info_set_stat(link_test_file, tar_type);

    tarxx::tarfile tar_file(tar_filename, tar_type);
    tar_file.add_from_filesystem(test_file.path);
    tar_file.add_from_filesystem(link_location);

    tar_file.close();

    const auto files_in_tar = util::files_in_tar_archive(tar_filename);
    const std::vector<util::file_info> expected_files = {
            link_test_file,
            test_file,
    };

    util::expect_files_in_tar(tar_filename, expected_files, tar_type);
}

TEST_P(tar_tests, add_from_filesystem_and_resolve_symlink)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();

    const auto file_name = "test_file";
    auto test_file = util::create_test_file(tar_type, std::filesystem::temp_directory_path() / file_name);
    const auto link_location = std::filesystem::temp_directory_path() / "symlink_to_file";

    util::remove_if_exists(tar_filename);
    util::remove_if_exists(link_location);

    std::filesystem::create_symlink(test_file.path, link_location);
    tarxx::tarfile tar_file(tar_filename, tar_type);
    tar_file.add_from_filesystem(link_location, true);

    tar_file.close();

    const auto files_in_tar = util::files_in_tar_archive(tar_filename);

    // we expect the link name but as regular file
    test_file.path = link_location;
    const std::vector<util::file_info> expected_files = {
            test_file,
    };

    util::expect_files_in_tar(tar_filename, expected_files, tar_type);
}

util::file_info create_fake_link(const tarxx::tarfile::tar_type& tar_type, tarxx::Platform& platform, const char* file_name,
                                 const char* link_name, const uid_t user, const gid_t group)
{
    return {
            .permissions = "lrwxrwxrwx",
            .owner = tar_type == tarxx::tarfile::tar_type::ustar ? platform.user_name(user) : std::to_string(user),
            .group = tar_type == tarxx::tarfile::tar_type::ustar ? platform.group_name(group) : std::to_string(group),
            .size = 0U,
            .date = "1970-01-01",
            .time = "00:00",
            .path = file_name,
            .link_name = link_name,
            .mtime = {},
            .mode = static_cast<tarxx::mode_t>(tarxx::permission_t::all_all)};
}

TEST_P(tar_tests, add_link_via_streaming)
{
    if (util::tar_version() != util::tar_version::gnu) {
        // Only GNU tar shows links in tar -tvf
        return;
    }

    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();

    tarxx::Platform platform;
    tarxx::tarfile tar_file(tar_filename, tar_type);
    const auto file_name = "file";
    const auto link_name = "link";
    const auto user = platform.user_id();
    const auto group = platform.group_id();
    tar_file.add_symlink(file_name, link_name, user, group, 0);
    const auto fake_link = create_fake_link(tar_type, platform, file_name, link_name, user, group);
    tar_file.close();
    util::expect_files_in_tar(tar_filename, {fake_link}, tar_type);
}

TEST_P(tar_tests, add_hard_link_via_streaming)
{
    if (util::tar_version() != util::tar_version::gnu) {
        // Only GNU tar shows links in tar -tvf
        return;
    }

    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();

    tarxx::Platform platform;
    tarxx::tarfile tar_file(tar_filename, tar_type);
    const auto file_name = "file";
    const auto link_name = "link";
    const auto user = platform.user_id();
    const auto group = platform.group_id();
    tar_file.add_hardlink(file_name, link_name, user, group, 0);
    auto fake_link = create_fake_link(tar_type, platform, file_name, link_name, user, group);
    fake_link.permissions = "hrwxrwxrwx";

    tar_file.close();
    util::expect_files_in_tar(tar_filename, {fake_link}, tar_type);
}

TEST_P(tar_tests, add_hard_link_from_filesystem)
{
    if (util::tar_version() != util::tar_version::gnu) {
        // Only GNU tar shows links in tar -tvf
        return;
    }

    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();

    const auto file_name = "test_file";
    const auto test_file = util::create_test_file(tar_type, std::filesystem::temp_directory_path() / file_name);
    auto test_file_hard_link = test_file;
    test_file_hard_link.permissions[0] = 'h';
    test_file_hard_link.link_name = test_file.path;
    test_file_hard_link.size = 0;
    const auto link_location = std::filesystem::temp_directory_path() / "hardlink_to_file";

    util::remove_if_exists(tar_filename);
    util::remove_if_exists(link_location);

    std::filesystem::create_hard_link(test_file.path, link_location);
    util::file_info link_test_file {.path = test_file.path};
    util::file_info_set_stat(link_test_file, tar_type);
    link_test_file.link_name = link_location;
    link_test_file.permissions[0] = 'h';

    tarxx::tarfile tar_file(tar_filename, tar_type);

    // add file twice, as it should be a hard link the second time.
    tar_file.add_from_filesystem(test_file.path);
    tar_file.add_from_filesystem(test_file.path);
    tar_file.add_from_filesystem(link_location);

    tar_file.close();

    const auto files_in_tar = util::files_in_tar_archive(tar_filename);
    const std::vector<util::file_info> expected_files = {
            test_file,
            test_file_hard_link,
            link_test_file,
    };

    util::expect_files_in_tar(tar_filename, expected_files, tar_type);
}

TEST_P(tar_tests, add_relative_directory_from_filesystem)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    auto [dir, test_files] = util::create_multiple_test_files_with_sub_folders(tar_type);
    std::filesystem::current_path(dir);

    tarxx::tarfile f(tar_filename, tar_type);

    f.add_from_filesystem_recursive(".");
    f.close();

    util::file_info root_dir;
    root_dir.path = dir;
    util::file_info_set_stat(root_dir, tar_type);

    util::file_info sub_dir;
    sub_dir.path = std::filesystem::path(test_files.at(1).path).parent_path().string();
    util::file_info_set_stat(sub_dir, tar_type);

    test_files.push_back(root_dir);
    test_files.push_back(sub_dir);

    for (auto& info : test_files) {
        const auto rel = std::filesystem::relative(info.path, dir).string();
        if (rel == ".") {
            info.path = ".";
        } else {
            info.path = "./" + rel;
        }

        if (info.permissions.at(0) == 'd') {
            info.path += "/";
        }
    }

    util::expect_files_in_tar(tar_filename, test_files, tar_type);
}

TEST_P(tar_tests, add_parent_parent_directory)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    auto [dir, test_files] = util::create_multiple_test_files_with_sub_folders(tar_type);
    const auto sub = dir / "sub_folder";
    const auto sub_sub = sub / "sub_sub";
    std::filesystem::create_directories(sub_sub);
    std::filesystem::current_path(sub_sub);

    tarxx::tarfile f(tar_filename, tar_type);

    f.add_from_filesystem_recursive("../..");
    f.close();

    util::file_info root_dir;
    root_dir.path = dir;
    util::file_info_set_stat(root_dir, tar_type);

    util::file_info sub_dir;
    sub_dir.path = std::filesystem::path(test_files.at(1).path).parent_path().string();
    util::file_info_set_stat(sub_dir, tar_type);

    util::file_info sub_sub_dir;
    sub_sub_dir.path = std::filesystem::path(sub_dir.path) / "sub_sub";
    util::file_info_set_stat(sub_sub_dir, tar_type);

    test_files.push_back(root_dir);
    test_files.push_back(sub_dir);
    test_files.push_back(sub_sub_dir);

    for (auto& info : test_files) {
        const auto rel = std::filesystem::relative(info.path, dir).string();
        info.path = rel;

        if (info.permissions.at(0) == 'd') {
            info.path += "/";
        }
    }

    util::expect_files_in_tar(tar_filename, test_files, tar_type);
}

TEST_P(tar_tests, add_from_filesystem_recursive_tar_will_be_part_of_itself)
{
    const auto tar_type = GetParam();
    auto [dir, test_files] = util::create_multiple_test_files_with_sub_folders(tar_type);

    tarxx::tarfile f(dir / "test.tar", tar_type);
    EXPECT_THROW(f.add_from_filesystem_recursive(dir), std::invalid_argument);
}

TEST_P(tar_tests, add_from_recursive_tar_will_be_part_of_itself)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();

    tarxx::tarfile f(tar_filename, tar_type);
    EXPECT_THROW(f.add_from_filesystem(tar_filename), std::invalid_argument);
}

TEST_P(tar_tests, add_directory_from_filesystem)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    const auto test_dir = util::create_test_directory(tar_type);

    tarxx::tarfile f(tar_filename, tar_type);

    f.add_from_filesystem(test_dir.path);
    f.close();

    const auto files = util::files_in_tar_archive(tar_filename);
    EXPECT_EQ(files.size(), 1);

    util::expect_files_in_tar(tar_filename, {test_dir}, tar_type);
}

TEST_P(tar_tests, add_from_filesystem_file_grows_while_reading)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    util::remove_if_exists(tar_filename);

    tarxx::tarfile tar_file(tar_filename, tar_type);
    const auto test_file = util::grow_source_file_during_tar_creation(tar_file, tar_type);
    expect_disk_file_ge_file_in_tar_and_tar_valid(tar_filename, test_file, tar_type);
}

TEST_P(tar_tests, add_from_filesystem_file_grows_while_reading_streaming_output)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    auto file_name = "appending_file"s;

    auto test_file = util::create_test_file(tar_type, std::filesystem::temp_directory_path() / file_name);
    util::remove_if_exists(tar_filename);

    auto abort_appending = false;
    auto append_thread_running = false;
    auto append_thread = append_to_file_in_thread(test_file, append_thread_running, abort_appending);

    std::ofstream ofs(tar_filename);
    const auto write_callback = [&](const tarxx::block_t& data, size_t size) {
        ofs.write(data.data(), size);
    };

    tarxx::tarfile f(std::move(write_callback), tar_type);
    f.add_from_filesystem(test_file.path);
    f.close();
    abort_appending = true;
    append_thread.join();

    ofs.close();

    expect_disk_file_ge_file_in_tar_and_tar_valid(tar_filename, test_file, tar_type);
}

TEST_P(tar_tests, add_from_filesystem_file_shrinks_while_reading)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    util::remove_if_exists(tar_filename);

    tarxx::tarfile tar_file(tar_filename, tar_type);
    const auto test_file = util::shrink_source_file_during_tar_creation(tar_file, tar_type);
    expect_disk_file_le_file_in_tar_and_tar_valid(tar_filename, test_file, tar_type);
}

TEST_P(tar_tests, add_from_filesystem_file_shrinks_while_reading_streaming_output)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    auto file_name = "shrinking_file"s;

    auto test_file = util::create_test_file_with_size(tar_type, 250 * 1024 * 1024, std::filesystem::temp_directory_path() / file_name);

    util::remove_if_exists(tar_filename);
    auto abort_removing = false;
    auto remove_thread_running = false;
    auto remove_thread = remove_from_file_in_thread(test_file, remove_thread_running, abort_removing);

    std::ofstream ofs(tar_filename);
    const auto write_callback = [&](const tarxx::block_t& data, size_t size) {
        ofs.write(data.data(), size);
    };

    tarxx::tarfile f(std::move(write_callback), tar_type);
    f.add_from_filesystem(test_file.path);
    f.close();
    ofs.flush();
    ofs.close();

    abort_removing = true;
    remove_thread.join();

    expect_disk_file_le_file_in_tar_and_tar_valid(tar_filename, test_file, tar_type);
}

#ifdef __linux
TEST_P(tar_tests, add_from_filesystem_procinfo)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    util::remove_if_exists(tar_filename);

    const auto out_dir = std::filesystem::temp_directory_path() / "add_from_filesystem_procinfo";
    util::remove_if_exists(out_dir.string());
    std::filesystem::create_directories(out_dir.string());

    tarxx::tarfile f(tar_filename, tar_type);
    const auto proc_cpuinfo = "/proc/cpuinfo";
    f.add_from_filesystem(proc_cpuinfo);
    f.close();


    util::extract_tar(tar_filename, out_dir);
    tarxx::Platform platform;
    std::stringstream cpu_info_tar;
    std::stringstream cpu_info_os;
    {
        std::ifstream ifs(out_dir / "proc/cpuinfo");
        cpu_info_tar << ifs.rdbuf();
    }

    {
        std::ifstream ifs(proc_cpuinfo);
        cpu_info_os << ifs.rdbuf();
    }

    const auto tar_cpu_info = util::split_string(cpu_info_tar.str(), '\n');
    const auto os_cpu_info = util::split_string(cpu_info_os.str(), '\n');

    ASSERT_EQ(tar_cpu_info.size(), os_cpu_info.size());
    for (auto i = 0; i < tar_cpu_info.size(); ++i) {
        const auto& tar_cpu_line = tar_cpu_info.at(i);
        const auto& os_cpu_line = os_cpu_info.at(i);

        // we have to ignore the frequency as the kernel dynamically changes
        // this based on the current demand
        if (tar_cpu_line.find("MHz") != std::string::npos) {
            continue;
        }

        EXPECT_EQ(tar_cpu_line, os_cpu_line);
    }
}

#endif

TEST_P(tar_tests, add_mutliple_of_block_size_from_filesystem)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    auto file_name0 = "file0"s;
    auto file_name1 = "file1"s;

    auto test_file0 = util::create_test_file_with_size(tar_type, 2 * tarxx::BLOCK_SIZE, std::filesystem::temp_directory_path() / file_name0);
    auto test_file1 = util::create_test_file_with_size(tar_type, 2 * tarxx::BLOCK_SIZE, std::filesystem::temp_directory_path() / file_name1);

    util::remove_if_exists(tar_filename);

    tarxx::tarfile f(tar_filename, tar_type);
    f.add_from_filesystem(test_file0.path);
    f.add_from_filesystem(test_file1.path);
    f.close();

    util::expect_files_in_tar(tar_filename, {test_file0, test_file1}, tar_type);
}

TEST_P(tar_tests, add_directory_via_streaming)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    tarxx::Platform platform;

    tarxx::tarfile f(tar_filename, tar_type);

    std::time_t time = std::time(nullptr);
    const auto user = platform.user_id();
    const auto group = platform.group_id();
    f.add_directory("test_dir", 0755, user, group, time);
    f.close();

    const auto files = util::files_in_tar_archive(tar_filename);
    EXPECT_EQ(files.size(), 1);
    const auto& file = files.at(0);
    if (tar_type == tarxx::tarfile::tar_type::ustar) {
        EXPECT_EQ(file.owner, platform.user_name(user));
        EXPECT_EQ(file.group, platform.group_name(group));
    } else {
        EXPECT_EQ(file.owner, std::to_string(user));
        EXPECT_EQ(file.group, std::to_string(group));
    }

    EXPECT_EQ(file.size, 0);
    EXPECT_EQ(file.permissions, "drwxr-xr-x");
}

TEST_P(tar_tests, add_directory_twice_via_filesystem)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    const auto test_dir = util::create_test_directory(tar_type);

    util::file_info link_test_file {.path = test_dir.path};
    util::file_info_set_stat(link_test_file, tar_type);
    link_test_file.link_name = test_dir.path;
    link_test_file.permissions[0] = 'h';

    tarxx::tarfile f(tar_filename, tar_type);

    f.add_from_filesystem(test_dir.path);
    f.add_from_filesystem(test_dir.path);
    f.close();

    util::expect_files_in_tar(tar_filename, {test_dir, test_dir}, tar_type);
}

TEST_P(tar_tests, add_directory_twice_via_streaming)
{
    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();
    tarxx::Platform platform;

    const auto user = platform.user_id();
    const auto group = platform.group_id();

    util::file_info test_dir {
            .permissions = "drwxr-xr-x",
            .owner = tar_type == tarxx::tarfile::tar_type::ustar ? platform.user_name(user) : std::to_string(user),
            .group = tar_type == tarxx::tarfile::tar_type::ustar ? platform.group_name(group) : std::to_string(group),
            .size = 0,
            .date = "1970-01-01",
            .time = "00:00",
            .path = "test_dir/",
            .link_name = "",
            .mtime = {0, 0},
            .mode = 0755,
            .device_type = "",
    };

    tarxx::tarfile f(tar_filename, tar_type);

    f.add_directory("test_dir", 0755, user, group, 0);
    f.add_directory("test_dir", 0755, user, group, 0);
    f.close();

    util::expect_files_in_tar(tar_filename, {test_dir, test_dir}, tar_type);
}

// Below follow tests only valid for ustar
TEST(tar_tests, add_from_filesystem_ustar_prefix_used)
{
    const auto tar_filename = util::tar_file_name();
    const auto tar_type = tarxx::tarfile::tar_type::ustar;
    const auto create_file_name = [&](int len) {
        auto name = "test"s;
        while ((std::filesystem::temp_directory_path() / name).string().length() < len) {
            name += "x";
        }
        return name;
    };

    auto file_name_100_chars = create_file_name(99);
    file_name_100_chars += "y";

    auto file_name_slash_at_index_100 = create_file_name(99);
    file_name_slash_at_index_100 += "/";

    auto file_name_short = create_file_name(42);

    auto file_with_sub_path = util::create_test_file(
            tar_type,
            std::filesystem::temp_directory_path() / "subfolder1" / "subfolder2" / file_name_100_chars);

    auto file_with_name_truncated = util::create_test_file(
            tar_type,
            std::filesystem::temp_directory_path() / (file_name_100_chars + "foobar"));

    auto file_with_slash_at_index_100 = util::create_test_file(
            tar_type,
            std::filesystem::temp_directory_path() / file_name_slash_at_index_100 / "foobar");

    auto file_with_short_name = util::create_test_file(
            tar_type,
            std::filesystem::temp_directory_path() / file_name_short);

    std::vector<util::file_info> test_files = {
            file_with_sub_path,
            file_with_name_truncated,
            file_with_slash_at_index_100,
            file_with_short_name};

    util::remove_if_exists(tar_filename);
    tarxx::tarfile f(tar_filename, tar_type);
    for (const auto& fi : test_files) {
        f.add_from_filesystem(fi.path);
    }
    f.close();

    // remove the last char from the name, because it will be truncated by tar, and we should expect this.
    test_files.at(1).path = file_with_name_truncated.path.substr(0, file_with_name_truncated.path.length() - 1);

    util::expect_files_in_tar(tar_filename, test_files, tar_type);
}

#if defined(__linux)
TEST(tar_tests, add_char_special_device_from_filesystem)
{
    if (util::tar_version() != util::tar_version::gnu) {
        return;
    }
    const auto tar_type = tarxx::tarfile::tar_type::ustar;
    const auto tar_filename = util::tar_file_name();
    tarxx::tarfile f(tar_filename, tar_type);
    util::file_info test_file {
            .path = "/dev/random"};
    util::file_info_set_stat(test_file, tar_type);
    std::vector<util::file_info> expected_files = {
            test_file};

    f.add_from_filesystem(test_file.path);
    f.close();

    util::expect_files_in_tar(tar_filename, expected_files, tar_type);
}

TEST(tar_tests, add_char_special_device_via_streaming)
{
    if (util::tar_version() != util::tar_version::gnu) {
        return;
    }
    const auto tar_type = tarxx::tarfile::tar_type::ustar;
    const auto tar_filename = util::tar_file_name();
    tarxx::tarfile f(tar_filename, tar_type);
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

    util::expect_files_in_tar(tar_filename, expected_files, tar_type);
}

TEST(tar_tests, add_fifo_from_filesystem)
{
    if (util::tar_version() != util::tar_version::gnu) {
        return;
    }
    const auto tar_type = tarxx::tarfile::tar_type::ustar;
    const auto tar_filename = util::tar_file_name();

    util::file_info test_file {.path = std::filesystem::temp_directory_path() / "fifo"};
    util::remove_if_exists(test_file.path);
    mkfifo(test_file.path.c_str(), 0666);
    util::file_info_set_stat(test_file, tar_type);

    tarxx::tarfile f(tar_filename, tar_type);
    f.add_from_filesystem(test_file.path);
    f.close();

    std::vector<util::file_info> expected_files = {test_file};
    util::expect_files_in_tar(tar_filename, expected_files, tar_type);
}

TEST(tar_tests, add_fifo_via_streaming)
{
    if (util::tar_version() != util::tar_version::gnu) {
        return;
    }
    const auto tar_type = tarxx::tarfile::tar_type::ustar;
    const auto tar_filename = util::tar_file_name();

    util::file_info test_file {.path = std::filesystem::temp_directory_path() / "fifo"};
    util::remove_if_exists(test_file.path);
    mkfifo(test_file.path.c_str(), 0666);
    util::file_info_set_stat(test_file, tar_type);

    tarxx::Platform platform;
    const auto owner = platform.file_owner(test_file.path);
    const auto group = platform.file_group(test_file.path);

    tarxx::tarfile f(tar_filename, tar_type);
    f.add_fifo(test_file.path, test_file.mode, owner, group, test_file.mtime.tv_sec);
    f.close();

    std::vector<util::file_info> expected_files = {test_file};
    util::expect_files_in_tar(tar_filename, expected_files, tar_type);
}

TEST_P(tar_tests, add_socket)
{
    const auto sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    EXPECT_NE(sockfd, -1);

    struct sockaddr_un addr {};
    std::memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    const auto sock_path = "/tmp/test.sock";
    std::strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);
    EXPECT_NE(bind(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)), -1);

    const auto tar_type = tarxx::tarfile::tar_type::ustar;
    const auto tar_filename = util::tar_file_name();

    tarxx::tarfile f(tar_filename, tar_type);

    EXPECT_THROW(f.add_from_filesystem(sock_path), std::invalid_argument);

    close(sockfd);
    util::remove_if_exists(sock_path);
}

#endif

INSTANTIATE_TEST_SUITE_P(tar_type_dependent, tar_tests, ::testing::Values(tarxx::tarfile::tar_type::unix_v7, tarxx::tarfile::tar_type::ustar));
