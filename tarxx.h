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


#ifndef TARXX_TARXX_H_F498949DFCF643A3B77C60CF3AA29F36
#define TARXX_TARXX_H_F498949DFCF643A3B77C60CF3AA29F36


#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>

#if __linux
#    include <cerrno>
#    include <fcntl.h>
#    include <sys/stat.h>
#    include <sys/sysmacros.h>
#    include <sys/types.h>
#else
#    error "no support for targeted platform"
#endif


namespace tarxx {

#ifdef __linux
    struct errno_exception : std::system_error {
        errno_exception() : std::system_error(std::error_code(errno, std::generic_category())) {}
        using std::system_error::system_error;
    };
#endif

    using block_t = std::array<char, 512>;

    struct tarfile {

        enum class tar_type {
            unix_v7
            //TODO support for utar, star, etc.
        };

        using callback_t = std::function<void(const block_t&)>;

        explicit tarfile(const std::string& filename, tar_type type = tar_type::unix_v7)
            : file_(filename, std::ios::out | std::ios::binary), callback_(nullptr), mode_(output_mode::file_output), type_(type) {}

        explicit tarfile(callback_t callback, tar_type type = tar_type::unix_v7)
            : file_(), callback_(std::move(callback)), mode_(output_mode::stream_output), type_(type) {}

        ~tarfile()
        {
            close();
        }

        bool is_open()
        {
            switch (mode_) {
                case output_mode::file_output:
                    return file_.is_open();
                case output_mode::stream_output:
                    return callback_ != nullptr;
            }
            throw std::logic_error("unsupported output mode");
        }

        void close()
        {
            finish();
            if (!is_open()) file_.close();
        }

        void add_file(const std::string& filename)
        {
            block_t block;
            std::fstream infile(filename, std::ios::in | std::ios::binary);
            if (!infile.is_open()) return;
            write_header(filename);
            while (infile.good()) {
                infile.read(block.data(), block.size());
                auto read = infile.gcount();
                if (read < block.size()) std::fill_n(block.begin() + read, block.size() - read, 0);
                write(block);
            }
        }

    private:
        void write(const block_t& data)
        {
            if (!is_open()) return;
            switch (mode_) {
                case output_mode::stream_output:
                    callback_(data);
                    break;
                case output_mode::file_output:
                    file_.write(data.data(), data.size());
                    break;
            }
        }

        void finish()
        {
            block_t zeroes {};
            write(zeroes);
            write(zeroes);
        }

        void write_header(const std::string& filename)
        {
            if (type_ != tar_type::unix_v7) throw std::logic_error("unsupported tar format");

            block_t header {};

            write_into_block(header, filename, 0, 100);

#ifdef __linux
            struct ::stat buffer {};
            const auto stat_result = ::stat(filename.c_str(), &buffer);
            if (stat_result != 0) {
                throw errno_exception();
            }
            write_into_block(header, buffer.st_mode & ALLPERMS, 100, 7);
            write_into_block(header, buffer.st_uid, 108, 7);
            write_into_block(header, buffer.st_gid, 116, 7);
            write_into_block(header, buffer.st_size, 124, 11);
            write_into_block(header, buffer.st_mtim.tv_sec, 136, 11);

            //TODO link indicator (file type)
            //TODO name of linked file
#endif

            calc_and_write_checksum(header);

            write(header);
        }

        static void write_into_block(block_t& block, const unsigned long long value, const unsigned pos, const unsigned len)
        {
            return write_into_block(block, to_octal_ascii(value, len), pos, len);
        }

        static void write_into_block(block_t& block, const std::string& str, const unsigned pos, const unsigned len)
        {
            const auto copylen = str.size() < len ? str.size() : len;
            std::copy_n(str.c_str(), copylen, block.data() + pos);
        }

        static void calc_and_write_checksum(block_t& block)
        {
            std::fill_n(block.data() + 148, 8, ' ');
            unsigned chksum = 0;
            for (unsigned char c : block) chksum += (c & 0xFF);
            write_into_block(block, chksum, 148, 6);
            block[154] = 0;
        }

        static std::string to_octal_ascii(unsigned long long value, unsigned width)
        {
            std::stringstream sstr;
            sstr << std::oct << std::ios::right << std::setfill('0') << std::setw(width) << value;
            auto str = sstr.str();
            if (str.size() > width) str = str.substr(str.size() - width);
            return str;
        }

        enum class output_mode : unsigned {
            file_output,
            stream_output
        };

        tar_type type_;
        output_mode mode_;
        std::fstream file_;
        callback_t callback_;
    };

} // namespace tarxx


#endif //TARXX_TARXX_H_F498949DFCF643A3B77C60CF3AA29F36
