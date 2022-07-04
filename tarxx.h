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

    using std::string_literals::operator""s;

#ifdef __linux
    struct errno_exception : std::system_error {
        errno_exception() : std::system_error(std::error_code(errno, std::generic_category())) {}
        using std::system_error::system_error;
    };
#endif

    static constexpr int BLOCK_SIZE = 512;
    using block_t = std::array<char, BLOCK_SIZE>;

    struct tarfile {

        enum class tar_type {
            unix_v7
            //TODO support for utar, star, etc.
        };

        using callback_t = std::function<void(const block_t&)>;

        explicit tarfile(const std::string& filename, tar_type type = tar_type::unix_v7)
            : file_(filename, std::ios::out | std::ios::binary), callback_(nullptr), mode_(output_mode::file_output), type_(type), stream_file_header_pos_(-1), stream_block_used_(0)
        {}

        explicit tarfile(callback_t callback, tar_type type = tar_type::unix_v7)
            : file_(), callback_(std::move(callback)), mode_(output_mode::stream_output), type_(type), stream_file_header_pos_(-1), stream_block_used_(0)
        {}

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
            if (stream_file_header_pos_ >= 0) throw std::logic_error("Can't add new file while adding streaming data isn't completed");

            block_t block;
            std::fstream infile(filename, std::ios::in | std::ios::binary);
            if (!infile.is_open()) return;
            write_header(filename);
            while (infile.good()) {
                infile.read(block.data(), block.size());
                const auto read = infile.gcount();
                if (read < block.size()) std::fill_n(block.begin() + read, block.size() - read, 0);
                write(block);
            }
        }

        void add_file_streaming()
        {
            if (stream_file_header_pos_ >= 0) throw std::logic_error("Can't add new file while adding streaming data isn't completed");
            if (mode_ != output_mode::file_output) throw std::logic_error(__func__ + " only supports output mode file"s);

            // write empty header
            stream_file_header_pos_ = file_.tellg();
            block_t header {};
            write(header);
        }

        void add_file_streaming_data(const char* const data, std::streamsize size)
        {
            unsigned long pos = 0;
            block_t block;

            // fill a new block with old and new data if enough data
            // is available to fill a whole block
            if ((stream_block_used_ + size) >= BLOCK_SIZE) {
                // copy saved data
                std::copy_n(stream_block_.data(), stream_block_used_, block.data());

                // copy from new data
                const auto copy_from_new_data = BLOCK_SIZE - stream_block_used_;
                std::copy_n(data, copy_from_new_data, block.data() + stream_block_used_);

                // write new complete block
                write(block);

                pos += copy_from_new_data;
                size -= copy_from_new_data;
                stream_block_used_ = 0;
            }

            // write new block as long as we have enough data to fill a whole block
            while (size >= BLOCK_SIZE) {
                std::copy_n(data + pos, BLOCK_SIZE, block.data());
                pos += BLOCK_SIZE;
                size -= BLOCK_SIZE;
                write(block);
            }

            // store remaining data for next call or stream_file_complete
            std::copy_n(data + pos, size, stream_block_.data() + stream_block_used_);
            stream_block_used_ += size;
        }

#ifdef __linux
        void stream_file_complete(const std::string& filename, __mode_t mode, __uid_t uid, __gid_t gid, __off_t size, __time_t mod_time)
        {
            // create last block, 0 init to ensure correctness if size < block size
            block_t block {};
            std::copy_n(stream_block_.data(), block.size(), block.data());
            stream_block_used_ = 0;
            write(block);

            const auto stream_pos = file_.tellp();

            // seek to header
            file_.seekp(stream_file_header_pos_);
            write_header(filename, mode, uid, gid, size, mod_time);
            stream_file_header_pos_ = -1;

            file_.seekp(stream_pos);
        }
#endif

    private:
        static constexpr unsigned int HEADER_POS_MODE = 100U;
        static constexpr unsigned int HEADER_POS_UID = 108U;
        static constexpr unsigned int HEADER_POS_GID = 116U;
        static constexpr unsigned int HEADER_POS_SIZE = 124U;
        static constexpr unsigned int HEADER_POS_MTIM = 136U;
        static constexpr unsigned int HEADER_LEN_MODE = 7U;
        static constexpr unsigned int HEADER_LEN_UID = 7U;
        static constexpr unsigned int HEADER_LEN_GID = 7U;
        static constexpr unsigned int HEADER_LEN_SIZE = 11U;
        static constexpr unsigned int HEADER_LEN_MTIM = 11U;

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
#ifdef __linux
            // clang-format off
            struct ::stat buffer {};
            // clang-format on
            const auto stat_result = ::stat(filename.c_str(), &buffer);
            if (stat_result != 0) {
                throw errno_exception();
            }

            write_header(filename, buffer.st_mode & ALLPERMS, buffer.st_uid, buffer.st_gid, buffer.st_size, buffer.st_mtim.tv_sec);
#endif
        }

#ifdef __linux
        void write_header(const std::string& filename, __mode_t mode, __uid_t uid, __gid_t gid, __off_t size, __time_t time)
        {
            if (type_ != tar_type::unix_v7) throw std::logic_error("unsupported tar format");

            block_t header {};

            write_into_block(header, filename, 0, 100);

            // clang-format off
            struct ::stat buffer {};
            // clang-format on
            const auto stat_result = ::stat(filename.c_str(), &buffer);
            if (stat_result != 0) {
                throw errno_exception();
            }
            write_into_block(header, mode, HEADER_POS_MODE, HEADER_LEN_MODE);
            write_into_block(header, uid, HEADER_POS_UID, HEADER_LEN_UID);
            write_into_block(header, gid, HEADER_POS_GID, HEADER_LEN_GID);
            write_into_block(header, size, HEADER_POS_SIZE, HEADER_LEN_SIZE);
            write_into_block(header, time, HEADER_POS_MTIM, HEADER_LEN_MTIM);

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
        long stream_file_header_pos_;
        block_t stream_block_;
        size_t stream_block_used_;
    };

} // namespace tarxx


#endif //TARXX_TARXX_H_F498949DFCF643A3B77C60CF3AA29F36
