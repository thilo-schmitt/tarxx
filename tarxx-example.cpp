// tarxx - modern C++ tar library
// Copyright (c) 2022, Thilo Schmitt
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

#include "tarxx.h"
#include <filesystem>
#include <getopt.h>
#include <iostream>
#include <string>
#include <vector>

#if __linux
#    include <thread>
#    include <unistd.h>
#endif

static int tar_files_in(
        tarxx::tarfile& tar,
        const std::vector<std::string>& input_files)
{
    if (!tar.is_open()) return 2;
    for (const auto& file : input_files) {
        std::filesystem::path path(file);
        if (is_directory(path)) {
            for (const auto& dir_entry :
                 std::filesystem::recursive_directory_iterator(path)) {
                if (!is_directory(dir_entry)) {
                    tar.add_file(dir_entry.path());
                }
            }
        } else {
            tar.add_file(path);
        }
    }

    return 0;
}

#ifdef WITH_COMPRESSION
static int tar_files_in_stream_out(std::ostream& os, const std::vector<std::string>& input_files, const tarxx::tarfile::compression_mode& compression_mode)
#else
static int tar_files_in_stream_out(std::ostream& os, const std::vector<std::string>& input_files)
#endif

{

    tarxx::tarfile::callback_t cb = [&](const tarxx::block_t& block, const size_t size) {
        os.write(block.data(), size);
    };

#ifdef WITH_COMPRESSION
    tarxx::tarfile tar(std::move(cb), compression_mode);
#else
    tarxx::tarfile tar(std::move(cb));
#endif

    return tar_files_in(tar, input_files);
}

#ifdef WITH_COMPRESSION
static int tar_files_in_file_out(const std::string& output_file, const std::vector<std::string>& input_files, const tarxx::tarfile::compression_mode& compression_mode)
#else
static int tar_files_in_file_out(const std::string& output_file, const std::vector<std::string>& input_files)
#endif
{

#ifdef WITH_COMPRESSION
    tarxx::tarfile tar(output_file, compression_mode);
#else
    tarxx::tarfile tar(output_file);
#endif

    return tar_files_in(tar, input_files);
}

static int tar_stream_in(tarxx::tarfile& tar)
{
    if (!tar.is_open()) return 2;

    std::array<char, 1> input_buff {};
    long long total_size = 0;
    tar.add_file_streaming();

    while (std::cin.read(input_buff.data(), input_buff.size())) {
        const auto read_bytes = std::cin.gcount();
        total_size += read_bytes;
        tar.add_file_streaming_data(input_buff.data(), read_bytes);
    }

    std::time_t result = std::time(nullptr);

#if __linux
    tar.stream_file_complete("stdin", 0777, getuid(), getgid(), total_size, result);
#endif
    return 0;
}
#ifdef WITH_COMPRESSION
static int tar_stream_in_file_out(const std::string& output_file, const tarxx::tarfile::compression_mode& compression_mode)
#else
static int tar_stream_in_file_out(const std::string& output_file)
#endif
{

#ifdef WITH_COMPRESSION
    tarxx::tarfile tar(output_file, compression_mode);
#else
    tarxx::tarfile tar(output_file);
#endif

    return tar_stream_in(tar);
}


#ifdef WITH_COMPRESSION
static int tar_stream_in_stream_out(std::ostream& os, const tarxx::tarfile::compression_mode& compression_mode)
#else
static int tar_stream_in_stream_out(std::ostream& os)
#endif
{
    tarxx::tarfile::callback_t cb = [&](const tarxx::block_t& block, const size_t size) {
        os.write(block.data(), size);
    };

#ifdef WITH_COMPRESSION
    tarxx::tarfile tar(std::move(cb), compression_mode);
#else
    tarxx::tarfile tar(std::move(cb));
#endif
    return tar_stream_in(tar);
}

static bool std_out_redirected()
{
#ifdef __linux
    return isatty(fileno(stdout)) == 0;
#endif
}

static bool std_in_redirected()
{
#ifdef __linux
    return isatty(fileno(stdin)) == 0;
#endif
}


int main(const int argc, char* const* const argv)
{
    int opt;
#ifdef WITH_COMPRESSION
    auto compress = false;
#endif
    auto create = false;
    std::string filename;

    std::string shortOpts = "f:c";
#ifdef WITH_LZ4
    shortOpts += 'k';
#endif
#ifdef __linux
    while ((opt = getopt(argc, argv, shortOpts.c_str())) != -1) {
        switch (opt) {
            case 'c':
                create = true;
                break;
#ifdef WITH_LZ4
            case 'k':
                compress = true;
#endif
                break;
            case 'f':
                filename = optarg;
                break;
            default:
                std::cout << "Usage: " << argv[0]
                          << " [OPTION]... [-f OUTPUT] [INPUT>...]\n"
                          << "-c          create a tar archive\n"
                          << "-k          enable lz4 compression\n"
                          << "-f <FILES>  create tar archive from file instead of "
                             "standard input\n";
                return EXIT_FAILURE;
        }
    }
#endif

    if (!create) {
        std::cerr << "Unpacking archives is not support yet\n";
        return EXIT_FAILURE;
    }

    if (!std_out_redirected() && filename.empty()) {
        std::cerr << "Refusing to read/write archive content to terminal (missing -f option?)\n";
        return EXIT_FAILURE;
    }

    if (std_out_redirected() && !filename.empty()) {
        std::cerr << "Can't redirect output and use file\n";
        return EXIT_FAILURE;
    }

    try {
#ifdef WITH_LZ4
        // todo support other compression modes here
        const auto compression_mode = compress
                                              ? tarxx::tarfile::compression_mode::lz4
                                              : tarxx::tarfile::compression_mode::none;
#endif

        if (std_in_redirected()) {
            if (std_out_redirected()) {
#ifdef WITH_COMPRESSION
                return tar_stream_in_stream_out(std::cout, compression_mode);
#else
                return tar_stream_in_stream_out(std::cout);
#endif
            }

#ifdef WITH_COMPRESSION
            return tar_stream_in_file_out(filename, compression_mode);
#else
            return tar_stream_in_file_out(filename);
#endif
        }

        std::vector<std::string> input_files;
        for (int i = 1; i < argc; ++i) {
            if (std::string(argv[i]).rfind('-', 0) == 0) {
                continue;
            }

            if (filename == argv[i]) {
                continue;
            }

            input_files.emplace_back(argv[i]);
        }

        if (input_files.empty()) {
            std::cerr << "Courageously refusing to create an empty archive\n";
            return EXIT_FAILURE;
        }

        if (std_out_redirected()) {
#ifdef WITH_COMPRESSION
            return tar_files_in_stream_out(std::cout, input_files, compression_mode);
#else
            return tar_files_in_stream_out(std::cout, input_files);
#endif
        }

#ifdef WITH_COMPRESSION
        return tar_files_in_file_out(filename, input_files, compression_mode);
#else
        return tar_files_in_file_out(filename, input_files);
#endif

    } catch (std::exception& ex) {
        std::cerr << "Failed to create tar archive: " << ex.what() << "\n";
    }
}
