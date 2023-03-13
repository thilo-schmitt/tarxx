// tarxx - modern C++ tar library
// Copyright (c) 2022-2023, Thilo Schmitt
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
#include <iostream>
#include <string>
#include <vector>
#if defined(__linux)
#    include <getopt.h>
#    include <unistd.h>
#else
#    error "no support for targeted platform"
#endif

static int tar_files_in(
        tarxx::tarfile& tar,
        const std::vector<std::string>& input_files)
{
    if (!tar.is_open()) return 2;
    for (const auto& file : input_files) {
        tar.add_from_filesystem_recursive(file);
    }

    return 0;
}

#ifdef WITH_COMPRESSION
static int tar_files_in_stream_out(std::ostream& os, const std::vector<std::string>& input_files, const tarxx::tarfile::compression_mode& compression_mode, const tarxx::tarfile::tar_type& tar_type)
#else
static int tar_files_in_stream_out(std::ostream& os, const std::vector<std::string>& input_files, const tarxx::tarfile::tar_type& tar_type)
#endif

{
    tarxx::tarfile::callback_t cb = [&](const tarxx::block_t& block, const size_t size) {
        os.write(block.data(), size);
    };

#ifdef WITH_COMPRESSION
    tarxx::tarfile tar(std::move(cb), compression_mode, tar_type);
#else
    tarxx::tarfile tar(std::move(cb), tar_type);
#endif

    return tar_files_in(tar, input_files);
}

#ifdef WITH_COMPRESSION
static int tar_files_in_file_out(const std::string& output_file, const std::vector<std::string>& input_files, const tarxx::tarfile::compression_mode& compression_mode, const tarxx::tarfile::tar_type& tar_type)
#else
static int tar_files_in_file_out(const std::string& output_file, const std::vector<std::string>& input_files, const tarxx::tarfile::tar_type& tar_type)
#endif
{

#ifdef WITH_COMPRESSION
    tarxx::tarfile tar(output_file, compression_mode, tar_type);
#else
    tarxx::tarfile tar(output_file, tar_type);
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
static int tar_stream_in_file_out(const std::string& output_file, const tarxx::tarfile::compression_mode& compression_mode, const tarxx::tarfile::tar_type& tar_type)
#else
static int tar_stream_in_file_out(const std::string& output_file, const tarxx::tarfile::tar_type& tar_type)
#endif
{

#ifdef WITH_COMPRESSION
    tarxx::tarfile tar(output_file, compression_mode, tar_type);
#else
    tarxx::tarfile tar(output_file, tar_type);
#endif

    return tar_stream_in(tar);
}


#ifdef WITH_COMPRESSION
static int tar_stream_in_stream_out(std::ostream& os, const tarxx::tarfile::compression_mode& compression_mode, const tarxx::tarfile::tar_type& tar_type)
#else
static int tar_stream_in_stream_out(std::ostream& os, const tarxx::tarfile::tar_type& tar_type)
#endif
{
    tarxx::tarfile::callback_t cb = [&](const tarxx::block_t& block, const size_t size) {
        os.write(block.data(), size);
    };

#ifdef WITH_COMPRESSION
    tarxx::tarfile tar(std::move(cb), compression_mode, tar_type);
#else
    tarxx::tarfile tar(std::move(cb), tar_type);
#endif
    return tar_stream_in(tar);
}

static bool std_out_redirected()
{
#if defined(__linux)
    return isatty(fileno(stdout)) == 0;
#endif
}

static bool std_in_redirected()
{
#if defined(__linux)
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
    tarxx::tarfile::tar_type tar_type = tarxx::tarfile::tar_type::unix_v7;

    std::string shortOpts = "t:f:c";
#ifdef WITH_LZ4
    shortOpts += 'k';
#endif
#if defined(__linux)
    while ((opt = getopt(argc, argv, shortOpts.c_str())) != -1) {
        switch (opt) {
            case 'c':
                create = true;
                break;
#    ifdef WITH_LZ4
            case 'k':
                compress = true;
                break;
#    endif
            case 'f':
                filename = optarg;
                break;
            case 't':
                tar_type = static_cast<tarxx::tarfile::tar_type>(std::stoi(optarg));
                break;
            default:
                std::cout << "Usage: " << argv[0]
                          << " [OPTION]... [-f OUTPUT] [INPUT>...]\n"
                          << "-t          tar archive type\n"
                          << "            0: unix v7 (default)\n"
                          << "            1: ustar \n"
                          << "-c          create a tar archive\n"
                          << "-k          enable lz4 compression\n"
                          << "-f <FILES>  create tar archive from file instead of "
                             "standard input\n";
                return EXIT_FAILURE;
        }
    }
#endif

    if (!create) {
        std::cerr << "Unpacking archives is not supported yet\n";
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
#ifdef WITH_COMPRESSION
#    ifdef WITH_LZ4
        const auto compression_mode = compress
                                              ? tarxx::tarfile::compression_mode::lz4
                                              : tarxx::tarfile::compression_mode::none;
#    else
        const auto compression_mode = tarxx::tarfile::compression_mode::none;
#    endif
#endif

        if (std_in_redirected()) {
            if (std_out_redirected()) {
#ifdef WITH_COMPRESSION
                return tar_stream_in_stream_out(std::cout, compression_mode, tar_type);
#else
                return tar_stream_in_stream_out(std::cout, tar_type);
#endif
            }

#ifdef WITH_COMPRESSION
            return tar_stream_in_file_out(filename, compression_mode, tar_type);
#else
            return tar_stream_in_file_out(filename, tar_type);
#endif
        }

        std::vector<std::string> input_files;
        for (int i = 1; i < argc; ++i) {
            if (std::string(argv[i]).rfind('-', 0) == 0) {
                continue;
            }

            if (i > 0) {
                const auto argv_last_str = std::string(argv[i - 1]);
                if (argv_last_str.rfind('-', 0) == 0 && argv_last_str.find('t') != std::string::npos) {
                    continue;
                }
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
            return tar_files_in_stream_out(std::cout, input_files, compression_mode, tar_type);
#else
            return tar_files_in_stream_out(std::cout, input_files, tar_type);
#endif
        }

#ifdef WITH_COMPRESSION
        return tar_files_in_file_out(filename, input_files, compression_mode, tar_type);
#else
        return tar_files_in_file_out(filename, input_files, tar_type);
#endif

    } catch (std::exception& ex) {
        std::cerr << "Failed to create tar archive: " << ex.what() << "\n";
    }
}
