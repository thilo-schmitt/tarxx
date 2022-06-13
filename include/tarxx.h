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
#include <lz4frame_static.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

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

        enum class compression_mode : unsigned {
            none,
            lz4,
        };


        using callback_t = std::function<void(const block_t&, size_t size)>;

        tarfile(const std::string& filename, compression_mode compression, tar_type type = tar_type::unix_v7)
            : file_(filename, std::ios::out | std::ios::binary),
              callback_(nullptr), mode_(output_mode::file_output),
              compression_(compression),
              type_(type), stream_file_header_pos_(-1), stream_block_used_(0)
        {
            init_lz4();
        }

        tarfile(callback_t&& callback, compression_mode compression, tar_type type = tar_type::unix_v7)
            : file_(), callback_(std::move(callback)), mode_(output_mode::stream_output), compression_(compression), type_(type), stream_file_header_pos_(-1), stream_block_used_(0)
        {
            init_lz4();
        }


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
            try {
                finish();
                if (!is_open()) file_.close();
            } catch (const std::exception& ex) {
                // ignore exception in destructor, as they cannot be caught.
            }
        }

        void add_file(const std::string& filename)
        {
            if (stream_file_header_pos_ >= 0) throw std::logic_error("Can't add new file while adding streaming data isn't completed");
            // flush is necessary to get the correct position of the header
            if (compression_ == compression_mode::lz4) {
                lz4_flush();
            }

            block_t block;
            std::fstream infile(filename, std::ios::in | std::ios::binary);
            if (!infile.is_open()) throw std::runtime_error("Can't find input file " + filename);
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

            // flush is necessary to get the correct position of the header
            if (compression_ == compression_mode::lz4) {
                lz4_flush();
            }

            // write emtpy header
            stream_file_header_pos_ = file_.tellg();
            block_t header {};
            write(header, true);
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
            std::copy_n(stream_block_.data(), stream_block_used_, block.data());
            stream_block_used_ = 0;
            write(block);

            // flush is necessary so seek to the correct positions
            if (compression_ == compression_mode::lz4) {
                lz4_flush();
            }

            // seek to header
            const auto stream_pos = file_.tellp();
            file_.seekp(stream_file_header_pos_);
            write_header(filename, mode, uid, gid, size, mod_time);
            stream_file_header_pos_ = -1;

            file_.seekp(stream_pos);
        }
#endif

    private:
        template<typename F, typename... Args>
        auto lz4_check_error(F&& func, Args&&... args)
        {
            const auto lz4_result = func(std::forward<Args>(args)...);
            if (LZ4F_isError(lz4_result) != 0) {
                throw std::runtime_error("lz4 function failed: error "s + LZ4F_getErrorName(lz4_result));
            }
            return lz4_result;
        }

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

        void lz4_flush()
        {
            lz4_out_buf_pos_ += lz4_check_error(LZ4F_flush, lz4_ctx_->get(), lz4_out_buf_.data(), lz4_out_buf_.capacity(), nullptr);
            const auto lz4_result = lz4_check_error(LZ4F_flush, lz4_ctx_->get(), lz4_out_buf_.data(), lz4_out_buf_.capacity(), nullptr);
            lz4_out_buf_pos_ += lz4_result;
            write_lz4_data();
        }

        void write(const block_t& data, bool is_header = false)
        {
            if (!is_open()) return;
            if (compression_ == compression_mode::lz4) {
                if (is_header) {
                    const auto lz4_result = lz4_check_error(LZ4F_uncompressedUpdate, lz4_ctx_->get(), lz4_out_buf_.data(), lz4_out_buf_.capacity(),
                                                            data.data(), data.size(), nullptr);

                    lz4_out_buf_pos_ += lz4_result;
                    // flush is necessary to keep lz4_out_buf_pos_ consistent
                    lz4_flush();
                } else {
                    const auto lz4_result = lz4_check_error(LZ4F_compressUpdate, lz4_ctx_->get(), lz4_out_buf_.data(), lz4_out_buf_.capacity(),
                                                            data.data(), data.size(), nullptr);
                    lz4_out_buf_pos_ += lz4_result;
                }
                write_lz4_data();
            } else {
                switch (mode_) {
                    case output_mode::stream_output:
                        callback_(data, data.size());
                        break;
                    case output_mode::file_output:
                        file_.write(data.data(), data.size());
                        file_.flush(); // todo remove this
                        break;
                }
            }
        }

        void finish()
        {
            block_t zeroes {};
            write(zeroes);
            write(zeroes);

            if (compression_ == compression_mode::lz4 && lz4_ctx_ != nullptr) {
                const auto lz4_result = lz4_check_error(LZ4F_compressEnd,
                                                        lz4_ctx_->get(), lz4_out_buf_.data(), lz4_out_buf_.capacity(), nullptr);

                lz4_out_buf_pos_ += lz4_result;
                write_lz4_data();
            }
        }

        void write_header(const std::string& filename)
        {
#ifdef __linux
            struct ::stat buffer {};
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

            write_into_block(header, mode, HEADER_POS_MODE, HEADER_LEN_MODE);
            write_into_block(header, uid, HEADER_POS_UID, HEADER_LEN_UID);
            write_into_block(header, gid, HEADER_POS_GID, HEADER_LEN_GID);
            write_into_block(header, size, HEADER_POS_SIZE, HEADER_LEN_SIZE);
            write_into_block(header, time, HEADER_POS_MTIM, HEADER_LEN_MTIM);

            //TODO link indicator (file type)
            //TODO name of linked file
#endif

            calc_and_write_checksum(header);

            write(header, true);
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

        void init_lz4()
        {
            if (compression_ != compression_mode::lz4) {
                return;
            }

            lz4_ctx_ = std::make_unique<lz4_ctx>();
            const auto outbuf_size = lz4_check_error(LZ4F_compressBound, 16 * 1024, &lz4_prefs_);

            lz4_out_buf_.reserve(outbuf_size);
            const auto headerSize = lz4_check_error(LZ4F_compressBegin, lz4_ctx_->get(), lz4_out_buf_.data(), lz4_out_buf_.capacity(), &lz4_prefs_);
            lz4_out_buf_pos_ += headerSize;
            write_lz4_data();
        }

        void write_lz4_data()
        {
            unsigned long long offset = 0;
            switch (mode_) {
                // guarding that input is not stream is done in add_file_streaming
                // using a file as input is fine, as we have all data necessary
                // for compression w/o seeking
                case output_mode::stream_output:
                    while (lz4_out_buf_pos_ > 0) {
                        block_t block {};
                        const auto copy_size = std::min(lz4_out_buf_pos_, block.size());
                        std::copy_n(lz4_out_buf_.begin() + offset, copy_size, block.data());
                        callback_(block, copy_size);
                        lz4_out_buf_pos_ -= copy_size;
                        offset += copy_size;
                    }

                    break;
                case output_mode::file_output:
                    file_.write(lz4_out_buf_.data(), lz4_out_buf_pos_);
                    lz4_out_buf_pos_ = 0;
                    break;
            }
        }

        enum class output_mode : unsigned {
            file_output,
            stream_output
        };

        class lz4_ctx {
        public:
            lz4_ctx()
            {
                LZ4F_createCompressionContext(&ctx_, LZ4F_VERSION);
            }
            ~lz4_ctx()
            {
                free(ctx_);
            }

            LZ4F_cctx* get()
            {
                return ctx_;
            }

        private:
            LZ4F_compressionContext_t ctx_ = nullptr;
        };

        tar_type type_;
        output_mode mode_;
        compression_mode compression_;
        std::fstream file_;
        callback_t callback_;
        long stream_file_header_pos_;
        block_t stream_block_;
        size_t stream_block_used_;

        std::unique_ptr<lz4_ctx> lz4_ctx_;
        std::vector<char> lz4_out_buf_;
        size_t lz4_out_buf_pos_ = 0;

        static inline constexpr LZ4F_preferences_t lz4_prefs_ = {
                {LZ4F_max256KB, LZ4F_blockIndependent, LZ4F_noContentChecksum,
                 LZ4F_frame, 0 /* unknown content size */, 0 /* no dictID */,
                 LZ4F_noBlockChecksum},
                0,         /* compression level; 0 == default */
                0,         /* autoflush */
                0,         /* favor decompression speed */
                {0, 0, 0}, /* reserved, must be set to 0 */
        };
    };

} // namespace tarxx


#endif //TARXX_TARXX_H_F498949DFCF643A3B77C60CF3AA29F36
