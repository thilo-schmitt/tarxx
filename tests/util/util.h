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


#ifndef TARXX_SYSTEM_TAR_H
#define TARXX_SYSTEM_TAR_H

#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#ifdef __linux
#    include "tarxx.h"
#    include <fstream>
#    include <gtest/gtest.h>
#    include <pwd.h>
#    include <unistd.h>
#endif

namespace util {

    struct file_info {
        std::string permissions;
        std::string owner;
        std::string group;
        uint64_t size = 0;
        std::string date;
        std::string time;
        std::string path;
    };

#ifdef __linux
    inline struct passwd* passwd()
    {
        const auto uid = geteuid();
        return getpwuid(uid);
    }

    inline __uid_t uid()
    {
        const auto pw = passwd();
        return pw == nullptr ? std::numeric_limits<decltype(pw->pw_uid)>::max() : pw->pw_uid;
    }

    inline __uid_t gid()
    {
        const auto pw = passwd();
        return pw == nullptr ? std::numeric_limits<decltype(pw->pw_uid)>::max() : pw->pw_gid;
    }
#endif

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
        char* args_array[] = {const_cast<char*>(cmd.c_str()), args..., nullptr};
        pid_t pid = fork();
        if (pid == 0) {
            if (execvp(const_cast<char*>(cmd.c_str()), args_array) == -1)
                throw std::runtime_error(std::string("execvp failed launching ") + cmd + ", error=" + std::strerror(errno));
        }

        auto status = -1;
        if (pid <= 0) throw std::runtime_error(std::string("can't wait for process to finish, what=") + std::strerror(errno));
        if (waitpid(pid, &status, 0) == -1)  throw std::runtime_error(std::string("waitpid failed, what=") + std::strerror(errno));
        return WIFEXITED(status);
    }

    inline int execute_with_output(const std::string& cmd, std::string& std_out)
    {
        constexpr const auto buf_size = 256;
        std::array<char, buf_size> buffer {};

        auto pipe = popen(cmd.c_str(), "r");
        if (!pipe) throw std::runtime_error(std::string("popen() failed, what=") + std::strerror(errno));

        size_t count;
        while ((count = fread(buffer.data(), 1, buf_size, pipe)) > 0) {
            std_out.insert(std_out.end(), std::begin(buffer), std::next(std::begin(buffer), count));
        }

        return pclose(pipe);
    }

    inline std::vector<file_info> files_in_tar_archive(const std::string& filename)
    {
        std::string tar_output;
        const auto tar_result = execute_with_output("tar -tvf " + filename, tar_output);
        if (tar_result != 0) {
            throw std::runtime_error("Failed to list files in tar");
        }

        const auto tar_output_lines = split_string(tar_output, '\n');
        const std::regex expr("(\\w|-|/|:)+");
        std::vector<file_info> infos;
        for (const auto& line : tar_output_lines) {
            // line from tar might look like this
            // "-rw-r--r-- 1422250/880257   12 2022-09-16 12:26 /tmp/test"
            std::vector<std::string> tokens;
            for (auto i = std::sregex_iterator(line.begin(), line.end(), expr); i != std::sregex_iterator(); ++i) {
                tokens.push_back(i->str());
            }
            if (tokens.size() != 6U) {
                continue;
            }

            const auto owner_group = split_string(tokens.at(1), '/');

            infos.emplace_back(file_info {
                    tokens.at(0),
                    owner_group.at(0),
                    owner_group.at(1),
                    std::stoul(tokens.at(2)),
                    tokens.at(3),
                    tokens.at(4),
                    // paths with spaces are not supported
                    // as this is used for tests only it should be okay
                    tokens.at(5),
            });
        }

        return infos;
    }

    inline void remove_file_if_exists(const std::string& filename)
    {
        if (std::filesystem::exists(filename)) {
            std::filesystem::remove(filename);
        }
    }

    inline file_info create_test_file(
            const std::filesystem::path& test_file_path = std::filesystem::temp_directory_path() / "test_file",
            const std::string& file_content = "test content")
    {
        file_info file;
        file.path = test_file_path;
        file.owner = std::to_string(util::uid());
        file.group = std::to_string(util::gid());
        util::remove_file_if_exists(file.path);
        std::filesystem::create_directories(test_file_path.parent_path());

        std::ofstream os(file.path);
        os << file_content << std::endl;
        os.close();
        file.size = std::filesystem::file_size(file.path);
        return file;
    }

    inline std::tuple<std::filesystem::path, std::vector<util::file_info>> create_multiple_test_files_with_sub_folders()
    {
        std::filesystem::path dir(std::filesystem::temp_directory_path() / "add_multiple_files_recursive_success");
        if (!std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }

        std::vector<std::string> test_files = {
                dir / "test_file_1",
                dir / "sub_folder" / "test_file_2",
        };

        std::vector<util::file_info> expected_files;
        for (const auto& file : test_files) {
            expected_files.emplace_back(util::create_test_file(file));
        }

        return {dir, expected_files};
    }

    inline std::string test_files_as_str(const std::vector<file_info>& test_files)
    {
        std::stringstream test_files_str;
        for (const auto& file : test_files)
            test_files_str << " " << file.path;
        return test_files_str.str();
    }

    inline void file_from_tar_matches_original_file(const util::file_info& test_file, const util::file_info& file_in_tar)
    {
        EXPECT_EQ(test_file.path, file_in_tar.path);
        EXPECT_EQ(test_file.size, file_in_tar.size);
#ifdef __linux
        EXPECT_EQ(file_in_tar.owner, std::to_string(util::uid()));
        EXPECT_EQ(file_in_tar.group, std::to_string(util::gid()));
#endif
    }

    inline void tar_has_one_file_and_matches(const std::string& tar_filename, const file_info& reference_file)
    {
        const auto files = util::files_in_tar_archive(tar_filename);
        EXPECT_EQ(files.size(), 1);
        util::file_from_tar_matches_original_file(files.at(0), reference_file);
    }

    inline void tar_first_files_matches_original(const std::string& tar_filename, const file_info& original)
    {
        const auto files = util::files_in_tar_archive(tar_filename);
        EXPECT_EQ(files.size(), 1);
        util::file_from_tar_matches_original_file(original, files.at(0));
    }

    inline void expect_files_in_tar(const std::string& tar_filename, const std::vector<util::file_info>& expected_files)
    {
        const auto files_in_tar = util::files_in_tar_archive(tar_filename);
        EXPECT_EQ(files_in_tar.size(), expected_files.size());
        for (const auto& found_file : files_in_tar) {

            auto expected_file_found = false;
            for (const auto& expected_file : expected_files) {
                if (found_file.path != expected_file.path) continue;

                expected_file_found = true;
                util::file_from_tar_matches_original_file(expected_file, found_file);
            }

            EXPECT_TRUE(expected_file_found);
        }
    }

    inline void add_streaming_data(const std::string& reference_data, const util::file_info& reference_file, tarxx::tarfile& tar_file)
    {
        tar_file.add_file_streaming();
        tar_file.add_file_streaming_data(reference_data.data(), reference_data.size());
        tar_file.stream_file_complete(reference_file.path, 655, util::uid(), util::gid(), reference_file.size, 0);
    }

    inline std::string create_input_data(unsigned long size)
    {
        std::string reference_data;
        for (auto i = 0U; i < size; ++i) {
            reference_data.push_back('a');
        }

        return reference_data;
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
