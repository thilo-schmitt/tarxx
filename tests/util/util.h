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


#ifndef TARXX_SYSTEM_TAR_H
#define TARXX_SYSTEM_TAR_H

#include "tarxx.h"
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace util {

    struct file_info {
        std::string permissions;
        std::string owner;
        std::string group;
        tarxx::size_t size = 0;
        std::string date;
        std::string time;
        std::string path;
        std::string link_name;
        struct timespec mtime;
        tarxx::mode_t mode;
        std::string device_type;
    };

    inline std::vector<std::string> split_string(const std::string& str, const char& delim)
    {
        std::stringstream ss(str);
        std::string token;
        std::vector<std::string> res;
        while (std::getline(ss, token, delim)) {
            res.push_back(token);
        }
        return res;
    }

    template<typename CheckT, typename... Pack>
    constexpr inline bool is_homogeneous_type_v = (std::is_same_v<CheckT, Pack> && ...);

    template<typename... T, typename = std::enable_if<is_homogeneous_type_v<char*, T...>>>
    inline int execute(const std::string& cmd, T*... args)
    {
#if defined(__linux)
        char* args_array[] = {const_cast<char*>(cmd.c_str()), args..., nullptr};
        pid_t pid = fork();
        if (pid == 0) {
            if (execvp(const_cast<char*>(cmd.c_str()), args_array) == -1)
                throw std::runtime_error(std::string("execvp failed launching ") + cmd + ", error=" + std::strerror(errno));
        }

        auto status = -1;
        if (pid <= 0) throw std::runtime_error(std::string("can't wait for process to finish, what=") + std::strerror(errno));
        if (waitpid(pid, &status, 0) == -1) throw std::runtime_error(std::string("waitpid failed, what=") + std::strerror(errno));
        return WIFEXITED(status);
#else
#    error "no support for targeted platform"
#endif
    }

    inline int execute_with_output(const std::string& cmd, std::string& std_out)
    {
#if defined(__linux)
        constexpr const auto buf_size = 256;
        std::array<char, buf_size> buffer {};

        auto pipe = popen(cmd.c_str(), "r");
        if (!pipe) throw std::runtime_error(std::string("popen() failed, what=") + std::strerror(errno));

        size_t count;
        while ((count = fread(buffer.data(), 1, buf_size, pipe)) > 0) {
            std_out.insert(std_out.end(), std::begin(buffer), std::next(std::begin(buffer), count));
        }

        return pclose(pipe);
#else
#    error "no support for targeted platform"
#endif
    }

    enum class tar_version {
        gnu,
        bsd
    };

    inline tar_version tar_version()
    {
        std::string tar_output;
        const auto tar_result = execute_with_output("tar --version ", tar_output);
        if (tar_result != 0) throw tarxx::errno_exception();
        if (tar_output.find("GNU tar") != std::string::npos) return tar_version::gnu;
        if (tar_output.find("bsdtar") != std::string::npos) return tar_version::bsd;
        throw std::runtime_error("unsupported tar version: " + tar_output);
    }

    inline std::vector<file_info> files_in_tar_archive(const std::string& filename)
    {
        std::string tar_output;
        const auto tar_result = execute_with_output("tar -tvf " + filename, tar_output);
        if (tar_result != 0) {
            throw std::runtime_error("Failed to list files in tar");
        }

        const auto tar_output_lines = split_string(tar_output, 0xa);
        const std::regex expr("(\\w|-|\\./|/|:)+");
        std::vector<file_info> infos;
        const auto shell_tar = tar_version();
        for (const auto& line : tar_output_lines) {
            std::vector<std::string> tokens;
            for (auto i = std::sregex_iterator(line.begin(), line.end(), expr); i != std::sregex_iterator(); ++i) {
                tokens.push_back(i->str());
            }

            // gnu tar output
            // line from tar might look like this
            // "-rw-r--r-- 1422250/880257   12 2022-09-16 12:26 /tmp/test"
            if (shell_tar == tar_version::gnu) {
                const auto permissions = tokens.at(0);
                const auto file_type = permissions.at(0);
                const auto owner_group = split_string(tokens.at(1), '/');
                std::string name;
                std::string link_name;
                tarxx::size_t size = std::stoul(tokens.at(2));
                std::string date = tokens.at(3);
                std::string time = tokens.at(4);
                std::string device_type;
                if (file_type == 'l') {
                    link_name = tokens.at(5);
                    name = tokens.at(7);
                } else if (file_type == 'c' || file_type == 'b') {
                    name = tokens.at(6);
                    date = tokens.at(4);
                    time = tokens.at(5);
                    device_type = tokens.at(2) + "," + tokens.at(3);
                    size = 0;
                } else if (file_type == 'h') {
                    link_name = tokens.at(5);
                    name = tokens.at(8);
                } else {
                    name = tokens.at(5);
                }

                infos.emplace_back(file_info {
                        .permissions = tokens.at(0),
                        .owner = owner_group.at(0),
                        .group = owner_group.at(1),
                        .size = size,
                        .date = date,
                        .time = time,
                        // paths with spaces are not supported
                        // as this is used for tests only it should be okay
                        .path = name,
                        .link_name = link_name,
                        .device_type = device_type});
            } else if (shell_tar == tar_version::bsd) {
                // just assume that the files where from this year
                const auto year = []() {
                    const auto now = std::chrono::system_clock::now();
                    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
                    std::tm* now_tm = std::localtime(&now_time_t);
                    return now_tm->tm_year + 1900;
                }();

                const auto month = [&]() {
                    std::tm time = {};
                    std::stringstream ss(tokens.at(5));
                    ss >> std::get_time(&time, "%b");
                    return time.tm_mon + 1;
                }();

                std::stringstream date;
                date << year << "-" << std::setw(2) << std::setfill('0') << month << "-" << tokens.at(6);
                infos.emplace_back(file_info {
                        tokens.at(0),
                        tokens.at(2),
                        tokens.at(3),
                        std::stoul(tokens.at(4)),
                        date.str(),
                        tokens.at(7),
                        // paths with spaces are not supported
                        // as this is used for tests only it should be okay
                        tokens.at(8),
                });
            } else {
                throw std::runtime_error("tar version is not supported");
            }
        }

        return infos;
    }

    inline void remove_if_exists(const std::string& filename)
    {
        if (std::filesystem::exists(filename))
            std::filesystem::remove_all(filename);
    }

    inline void file_info_set_stat(file_info& file, const tarxx::tarfile::tar_type& tar_type)
    {
        const tarxx::Platform platform;
        const auto owner = platform.file_owner(file.path);
        const auto group = platform.file_group(file.path);
        switch (tar_type) {
            case tarxx::tarfile::tar_type::unix_v7:
                file.owner = std::to_string(owner);
                file.group = std::to_string(group);
                break;
            case tarxx::tarfile::tar_type::ustar:
                file.owner = platform.user_name(owner);
                file.group = platform.group_name(group);
                break;
        }

        std::array<char, 20> buf {};
        const auto mtime = platform.mod_time(file.path);
        const auto mtime_time_t = static_cast<time_t>(mtime);
        // mod time does not have usec
        file.mtime = {.tv_sec = mtime_time_t};

        std::strftime(buf.data(), buf.size(), "%Y-%m-%d", std::localtime(&mtime_time_t));
        file.date = std::string(buf.data());
        memset(buf.data(), 0, buf.size());

        strftime(buf.data(), buf.size(), "%H:%M", std::localtime(&mtime_time_t));
        file.time = std::string(buf.data());

        // set defaults, might be overwritten by special file types
        file.permissions = platform.permissions_str(file.path);
        file.mode = platform.mode(file.path);

        if (std::filesystem::is_symlink(file.path)) {
            file.link_name = file.path;
            file.path = platform.read_symlink(file.link_name);
            file.permissions = "lrwxrwxrwx";
            file.mode = static_cast<tarxx::mode_t>(tarxx::permission_t::all_all);
            file.size = 0U;
        } else if (std::filesystem::is_character_file(file.path) || std::filesystem::is_block_file(file.path)) {
            file.size = 0U;
            tarxx::major_t major;
            tarxx::minor_t minor;
            platform.major_minor(file.path, major, minor);
            std::stringstream ss;
            ss << major << "," << minor;
            file.device_type = ss.str();
        } else if (std::filesystem::is_directory(file.path)) {
            if (file.path.at(file.path.size() - 1) != '/') {
                file.path += '/';
            }
        }
    }

    inline file_info create_test_file(
            const tarxx::tarfile::tar_type& tar_type,
            const std::filesystem::path& test_file_path = std::filesystem::temp_directory_path() / "test_file",
            const std::string& file_content = "test content")
    {
        file_info file;
        file.path = test_file_path;

        util::remove_if_exists(file.path);
        std::filesystem::create_directories(test_file_path.parent_path());

        std::ofstream os(file.path);
        os << file_content << std::endl;
        os.close();
        file.size = std::filesystem::file_size(file.path);

        file_info_set_stat(file, tar_type);
        return file;
    }

    inline file_info create_test_directory(
            const tarxx::tarfile::tar_type& tar_type,
            const std::filesystem::path& test_file_path = std::filesystem::temp_directory_path() / "test_dir")
    {
        file_info file;
        file.path = test_file_path;

        util::remove_if_exists(file.path);
        std::filesystem::create_directories(test_file_path);
        file_info_set_stat(file, tar_type);

        return file;
    }

    inline std::tuple<std::filesystem::path, std::vector<util::file_info>> create_multiple_test_files_with_sub_folders(const tarxx::tarfile::tar_type& tar_type)
    {
        std::filesystem::path dir(std::filesystem::temp_directory_path() / "test");
        remove_if_exists(dir);
        std::filesystem::create_directories(dir);

        std::vector<std::string> test_files = {
                dir / "test_file_1",
                dir / "sub_folder" / "test_file_2",
        };

        std::vector<util::file_info> expected_files;
        for (const auto& file : test_files) {
            expected_files.emplace_back(util::create_test_file(tar_type, file));
        }

        return {dir, expected_files};
    }

    inline void append_folders_from_test_files(std::vector<file_info>& files, const tarxx::tarfile::tar_type& tar_type)
    {
        std::set<std::string> folders;
        for (const auto& f : files) {
            std::filesystem::path p(f.path);
            folders.emplace(p.parent_path().string());
        }
        for (const auto& folder : folders) {
            util::file_info fi;
            fi.path = folder;
            file_info_set_stat(fi, tar_type);
            files.push_back(fi);
        }
    }

    inline std::string test_files_as_str(const std::vector<file_info>& test_files)
    {
        std::stringstream test_files_str;
        for (const auto& file : test_files)
            test_files_str << " " << file.path;
        return test_files_str.str();
    }

    inline void file_from_tar_matches_original_file(const util::file_info& test_file, const util::file_info& file_in_tar, const tarxx::tarfile::tar_type& tar_type)
    {
        const tarxx::Platform platform;
        const auto path = platform.relative_path(test_file.path);
        EXPECT_EQ(path, file_in_tar.path);
        EXPECT_EQ(test_file.size, file_in_tar.size);
        EXPECT_EQ(test_file.date, file_in_tar.date);
        EXPECT_EQ(test_file.permissions, file_in_tar.permissions);
        EXPECT_EQ(file_in_tar.owner, test_file.owner);
        EXPECT_EQ(file_in_tar.group, test_file.group);
        EXPECT_EQ(file_in_tar.device_type, test_file.device_type);
    }

    inline void tar_has_one_file_and_matches(const std::string& tar_filename, const file_info& reference_file, const tarxx::tarfile::tar_type& tar_type)
    {
        const auto files = util::files_in_tar_archive(tar_filename);
        EXPECT_EQ(files.size(), 1);
        util::file_from_tar_matches_original_file(reference_file, files.at(0), tar_type);
    }

    inline void tar_first_files_matches_original(const std::string& tar_filename, const file_info& original, const tarxx::tarfile::tar_type& tar_type)
    {
        const auto files = util::files_in_tar_archive(tar_filename);
        EXPECT_EQ(files.size(), 1);
        util::file_from_tar_matches_original_file(original, files.at(0), tar_type);
    }

    inline void expect_files_in_tar(const std::string& tar_filename, const std::vector<util::file_info>& expected_files, const tarxx::tarfile::tar_type& tar_type)
    {
        const tarxx::Platform platform;
        const auto files_in_tar = util::files_in_tar_archive(tar_filename);
        EXPECT_EQ(files_in_tar.size(), expected_files.size());
        for (const auto& found_file : files_in_tar) {

            auto expected_file_found = false;
            for (const auto& expected_file : expected_files) {
                const auto expected_path = platform.relative_path(expected_file.path);
                const auto expected_link_name = platform.relative_path(expected_file.link_name);
                if (found_file.path != expected_path) continue;
                if (found_file.link_name != expected_link_name) continue;

                expected_file_found = true;
                util::file_from_tar_matches_original_file(expected_file, found_file, tar_type);
            }
            if (!expected_file_found) {
                std::cerr << "Missing " << found_file.path << " in tar archive\n";
            }
            EXPECT_TRUE(expected_file_found);
        }
    }

    inline void add_streaming_data(const std::string& reference_data, const util::file_info& reference_file, tarxx::tarfile& tar_file)
    {
        const tarxx::Platform platform;
        tar_file.add_file_streaming();
        tar_file.add_file_streaming_data(reference_data.data(), reference_data.size());
        tar_file.stream_file_complete(reference_file.path, reference_file.mode, platform.user_id(), platform.group_id(), reference_file.size, reference_file.mtime.tv_sec);
    }

    inline std::string create_input_data(unsigned long size)
    {
        std::string reference_data;
        for (auto i = 0U; i < size; ++i) {
            reference_data.push_back('a');
        }

        return reference_data;
    }

    inline std::string tar_file_name()
    {
        return std::filesystem::temp_directory_path() / "test.tar";
    }

#ifdef WITH_LZ4

    inline void decompress_lz4(const std::string& lz4_in, const std::string& tar_out)
    {
        std::string lz4_output;
        std::stringstream cmd;
        cmd << "lz4 -cdf " << lz4_in << ">" << tar_out;
        if (execute_with_output(cmd.str(), lz4_output) != 0) {
            throw std::runtime_error("Failed to decompress lz4 file: " + lz4_in + ", error=" + lz4_output);
        }
    }

#endif


} // namespace util


#endif //TARXX_SYSTEM_TAR_H
