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

    if (tar_type == tarxx::tarfile::tar_type::ustar) {
        util::append_folders_from_test_files(test_files, tar_type);
    }

    util::expect_files_in_tar(tar_filename, test_files, tar_type);
    std::filesystem::remove_all(dir);
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

TEST_P(tar_tests, add_link_from_filesystem)
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
            .path = link_location};
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

TEST_P(tar_tests, add_link_via_streaming)
{
    if (util::tar_version() != util::tar_version::gnu) {
        // Only GNU tar shows links in tar -tvf
        return;
    }

    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();

    const tarxx::Platform platform;
    tarxx::tarfile tar_file(tar_filename, tar_type);
    const auto file_name = "file";
    const auto link_name = "link";
    const auto user = platform.user_id();
    const auto group = platform.group_id();
    tar_file.add_symlink(file_name, link_name, user, group, 0);
    util::file_info fake_link {
            .permissions = "lrwxrwxrwx",
            .owner = tar_type == tarxx::tarfile::tar_type::ustar ? platform.user_name(user) : std::to_string(group),
            .group = tar_type == tarxx::tarfile::tar_type::ustar ? platform.group_name(group) : std::to_string(group),
            .size = 0U,
            .date = "1970-01-01",
            .time = "00:00",
            .path = file_name,
            .link_name = link_name,
            .mtime = {},
            .mode = static_cast<tarxx::mode_t>(tarxx::permission_t::all_all)};

    tar_file.close();
    util::expect_files_in_tar(tar_filename, {fake_link}, tar_type);
}

// this test is only for ustar
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

TEST(tar_tests, add_directory_from_filesystem)
{
    const auto tar_type = tarxx::tarfile::tar_type::ustar;
    const auto tar_filename = util::tar_file_name();
    const auto test_dir = util::create_test_directory(tar_type);

    tarxx::tarfile f(tar_filename, tar_type);

    f.add_from_filesystem(test_dir.path);
    f.close();

    const auto files = util::files_in_tar_archive(tar_filename);
    EXPECT_EQ(files.size(), 1);
    util::file_from_tar_matches_original_file(test_dir, files.at(0), tar_type);
}

TEST(tar_tests, add_directory_via_streaming)
{
    const auto tar_type = tarxx::tarfile::tar_type::ustar;
    const auto tar_filename = util::tar_file_name();
    const tarxx::Platform platform;

    tarxx::tarfile f(tar_filename, tar_type);

    std::time_t time = std::time(nullptr);
    const auto user = platform.user_id();
    const auto group = platform.group_id();
    f.add_directory("test_dir", 0755, user, group, time);
    f.close();

    const auto files = util::files_in_tar_archive(tar_filename);
    EXPECT_EQ(files.size(), 1);
    const auto& file = files.at(0);
    EXPECT_EQ(file.owner, platform.user_name(user));
    EXPECT_EQ(file.group, platform.group_name(group));
    EXPECT_EQ(file.size, 0);
    EXPECT_EQ(file.permissions, "drwxr-xr-x");
}

TEST_P(tar_tests, add_directory_twice_via_streaming)
{
    const auto tar_type = tarxx::tarfile::tar_type::ustar;
    const auto tar_filename = util::tar_file_name();
    const tarxx::Platform platform;

    tarxx::tarfile f(tar_filename, tar_type);

    std::time_t time = std::time(nullptr);
    const auto user = platform.user_id();
    const auto group = platform.group_id();
    f.add_directory("test_dir", 0755, user, group, time);
    EXPECT_THROW(f.add_directory("test_dir", 0755, user, group, time), std::logic_error);
}

TEST_P(tar_tests, add_directory_twice_via_filesystem)
{
    const auto tar_type = tarxx::tarfile::tar_type::ustar;
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

    const auto files = util::files_in_tar_archive(tar_filename);
    util::file_from_tar_matches_original_file(test_dir, files.at(0), tar_type);
}

TEST_P(tar_tests, add_hard_link_via_streaming)
{
    if (util::tar_version() != util::tar_version::gnu) {
        // Only GNU tar shows links in tar -tvf
        return;
    }

    const auto tar_type = GetParam();
    const auto tar_filename = util::tar_file_name();

    const tarxx::Platform platform;
    tarxx::tarfile tar_file(tar_filename, tar_type);
    const auto file_name = "file";
    const auto link_name = "link";
    const auto user = platform.user_id();
    const auto group = platform.group_id();
    tar_file.add_hardlink(file_name, link_name, user, group, 0);
    util::file_info fake_link {
            .permissions = "hrwxrwxrwx",
            .owner = tar_type == tarxx::tarfile::tar_type::ustar ? platform.user_name(user) : std::to_string(group),
            .group = tar_type == tarxx::tarfile::tar_type::ustar ? platform.group_name(group) : std::to_string(group),
            .size = 0U,
            .date = "1970-01-01",
            .time = "00:00",
            .path = file_name,
            .link_name = link_name,
            .mtime = {},
            .mode = static_cast<tarxx::mode_t>(tarxx::permission_t::all_all)};

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
    const auto link_location = std::filesystem::temp_directory_path() / "hardlink_to_file";

    util::remove_if_exists(tar_filename);
    util::remove_if_exists(link_location);

    std::filesystem::create_hard_link(test_file.path, link_location);
    util::file_info link_test_file {.path = test_file.path};
    util::file_info_set_stat(link_test_file, tar_type);
    link_test_file.link_name = link_location;
    link_test_file.permissions[0] = 'h';

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

#if defined(__linux)
TEST(tar_tests, add_char_special_device_from_filesystem)
{
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

#endif

INSTANTIATE_TEST_SUITE_P(tar_type_dependent, tar_tests, ::testing::Values(tarxx::tarfile::tar_type::unix_v7, tarxx::tarfile::tar_type::ustar));
