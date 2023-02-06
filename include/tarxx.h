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


#ifndef TARXX_TARXX_H_F498949DFCF643A3B77C60CF3AA29F36
#define TARXX_TARXX_H_F498949DFCF643A3B77C60CF3AA29F36

#ifdef WITH_LZ4
#    include <lz4frame_static.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#if __linux
#    include <cerrno>
#    include <fcntl.h>
#    include <grp.h>
#    include <pwd.h>
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
            unix_v7,
            ustar,
            //TODO support for utar, star, etc.
        };

        using callback_t = std::function<void(const block_t&, size_t size)>;

#ifdef WITH_COMPRESSION
        enum class compression_mode : unsigned {
            none,
#    ifdef WITH_LZ4
            lz4,
#    endif
        };

        explicit tarfile(const std::string& filename,
                         tar_type type = tar_type::unix_v7)
            : tarfile(filename, compression_mode::none, type)
        {
        }
        explicit tarfile(callback_t&& callback,
                         tar_type type = tar_type::unix_v7)
            : tarfile(std::move(callback), compression_mode::none, type)
        {
        }

        explicit tarfile(const std::string& filename,
                         compression_mode compression = compression_mode::none,
                         tar_type type = tar_type::unix_v7)
            : file_(filename, std::ios::out | std::ios::binary),
              callback_(nullptr), mode_(output_mode::file_output),
              compression_(compression),
              type_(type), stream_file_header_pos_(-1), stream_block_ {0}, stream_block_used_(0)
        {
#    ifdef WITH_LZ4
            init_lz4();
#    endif
        }

        explicit tarfile(callback_t&& callback,
                         compression_mode compression = compression_mode::none,
                         tar_type type = tar_type::unix_v7)
            : file_(), callback_(std::move(callback)), mode_(output_mode::stream_output),
              compression_(compression),
              type_(type), stream_file_header_pos_(-1), stream_block_ {0}, stream_block_used_(0)
        {
#    ifdef WITH_LZ4
            init_lz4();
#    endif
        }
#else
        explicit tarfile(const std::string& filename, tar_type type = tar_type::unix_v7)
            : file_(filename, std::ios::out | std::ios::binary), callback_(nullptr), mode_(output_mode::file_output), type_(type), stream_block_ {0}, stream_file_header_pos_(-1), stream_block_used_(0)
        {
        }

        explicit tarfile(callback_t&& callback,
                         tar_type type = tar_type::unix_v7)
            : file_(), callback_(std::move(callback)), mode_(output_mode::stream_output),
              type_(type), stream_file_header_pos_(-1), stream_block_ {0}, stream_block_used_(0)
        {
        }
#endif

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
                if (is_open()) {
                    finish();
                    file_.close();
                    callback_ = nullptr;
                }
            } catch (const std::exception& ex) {
                // ignore exception in destructor, as they cannot be caught.
            }
        }

#ifdef __cpp_lib_filesystem
        void add_files_recursive(const std::filesystem::path& path)
        {
            if (is_directory(path)) {
                //TODO https://github.com/thilo-schmitt/tarxx/issues/10 support for directories is missing
                for (const auto& f : std::filesystem::recursive_directory_iterator(path)) {
                    if (f.is_regular_file())
                        add_file(f.path());
                }
            } else if (is_regular_file(path)) {
                add_file(path);
            } else {
                throw std::invalid_argument(path.string() + " is neither a regular file, nor a directory");
            }
        }
#endif
        // TODO add support for adding directories
        void add_file(const std::string& filename)
        {
            check_state();

#ifdef WITH_LZ4
            if (compression_ == compression_mode::lz4) {
                lz4_flush();
            }
#endif
            block_t block;
            std::fstream infile(filename, std::ios::in | std::ios::binary);
            if (!infile.is_open()) throw std::runtime_error("Can't find input file " + filename);
            write_header(filename, file_type_flag::REGULAR_FILE);
            while (infile.good()) {
                infile.read(block.data(), block.size());
                const auto read = infile.gcount();
                if (read < block.size()) std::fill_n(block.begin() + read, block.size() - read, 0);
                write(block);
            }
        }

        void add_file_streaming()
        {
            check_state();
            if (mode_ != output_mode::file_output) throw std::logic_error(__func__ + " only supports output mode file"s);

                // flush is necessary to get the correct position of the header
#ifdef WITH_LZ4
            if (compression_ == compression_mode::lz4) {
                lz4_flush();
            }
#endif
            // write empty header
            stream_file_header_pos_ = file_.tellg();
            block_t header {};
            write(header, true);
        }

        void add_file_streaming_data(const char* const data, std::streamsize size)
        {
            if (!is_open()) throw std::logic_error("Cannot append file, tar archive is not open");

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
            if (stream_file_header_pos_ < 0) throw std::logic_error("Can't finish stream file, none is in progress");

            // create last block, 0 init to ensure correctness if size < block size
            block_t block {};
            std::copy_n(stream_block_.data(), stream_block_used_, block.data());
            stream_block_used_ = 0;
            write(block);

            // flush is necessary so seek to the correct positions
#    ifdef WITH_LZ4
            if (compression_ == compression_mode::lz4) {
                lz4_flush();
            }
#    endif

            // seek to header
            const auto stream_pos = file_.tellp();
            file_.seekp(stream_file_header_pos_);
            write_header(filename, mode, uid, gid, size, mod_time, file_type_flag::REGULAR_FILE);
            stream_file_header_pos_ = -1;
            file_.seekp(stream_pos);
        }
#endif

    private:
        // Offsets
        static constexpr unsigned int UNIX_V7_USTAR_HEADER_POS_NAME = 0U;
        static constexpr unsigned int UNIX_V7_USTAR_HEADER_POS_MODE = 100U;
        static constexpr unsigned int UNIX_V7_USTAR_HEADER_POS_UID = 108U;
        static constexpr unsigned int UNIX_V7_USTAR_HEADER_POS_GID = 116U;
        static constexpr unsigned int UNIX_V7_USTAR_HEADER_POS_SIZE = 124U;
        static constexpr unsigned int UNIX_V7_USTAR_HEADER_POS_MTIM = 136U;
        static constexpr unsigned int UNIX_V7_USTAR_HEADER_POS_CHECKSUM = 148U;
        static constexpr unsigned int UNIX_V7_USTAR_HEADER_POS_TYPEFLAG = 156U;
        static constexpr unsigned int UNIX_V7_USTAR_HEADER_POS_LINKNAME = 157U;

        static constexpr unsigned int USTAR_HEADER_POS_MAGIC = 257U;
        static constexpr unsigned int USTAR_HEADER_POS_UNAME = 265U;
        static constexpr unsigned int USTAR_HEADER_POS_GNAME = 297U;
        static constexpr unsigned int USTAR_HEADER_POS_DEVMAJOR = 329U;
        static constexpr unsigned int USTAR_HEADER_POS_DEVMINOR = 337U;
        static constexpr unsigned int USTAR_HEADER_POS_PREFIX = 345U;

        // Lengths
        static constexpr unsigned int UNIX_V7_USTAR_HEADER_LEN_NAME = 100U;
        static constexpr unsigned int UNIX_V7_USTAR_HEADER_LEN_MODE = 8U;
        static constexpr unsigned int UNIX_V7_USTAR_HEADER_LEN_UID = 8U;
        static constexpr unsigned int UNIX_V7_USTAR_HEADER_LEN_GID = 8U;
        static constexpr unsigned int UNIX_V7_USTAR_HEADER_LEN_SIZE = 12U;
        static constexpr unsigned int UNIX_V7_USTAR_HEADER_LEN_MTIM = 12U;
        static constexpr unsigned int UNIX_V7_USTAR_HEADER_LEN_CHKSUM = 8U;
        static constexpr unsigned int UNIX_V7_USTAR_HEADER_LEN_TYPEFLAG = 1U;
        static constexpr unsigned int UNIX_V7_USTAR_HEADER_LEN_LINKNAME = 100U;

        static constexpr unsigned int USTAR_HEADER_LEN_MAGIC = 6U;
        static constexpr unsigned int USTAR_HEADER_LEN_UNAME = 32U;
        static constexpr unsigned int USTAR_HEADER_LEN_GNAME = 32U;
        static constexpr unsigned int USTAR_HEADER_LEN_DEVMAJOR = 8U;
        static constexpr unsigned int USTAR_HEADER_LEN_DEVMINOR = 8U;
        static constexpr unsigned int USTAR_HEADER_LEN_PREFIX = 155U;

        enum class file_type_flag : char {
            REGULAR_FILE = '0',
            HARD_LINK = '1',
            SYMBOLIC_LINK = '2',
            CHARACTER_SPECIAL_FILE = '3',
            BLOCK_SPECIAL_FILE = '4',
            DIRECTORY = '5',
            FIFO = '6',
            CONTIGUOUS_FILE = '7',
        };


#ifdef WITH_LZ4
        template<typename F, typename... Args>
        auto lz4_call_and_check_error(F&& func, Args&&... args)
        {
            const auto lz4_result = func(std::forward<Args>(args)...);
            if (LZ4F_isError(lz4_result) != 0) {
                throw std::runtime_error("lz4 function failed: error "s + LZ4F_getErrorName(lz4_result));
            }
            return lz4_result;
        }

        void lz4_flush()
        {
            lz4_out_buf_pos_ += lz4_call_and_check_error(LZ4F_flush, lz4_ctx_->get(), lz4_out_buf_.data(), lz4_out_buf_.capacity(), nullptr);
            const auto lz4_result = lz4_call_and_check_error(LZ4F_flush, lz4_ctx_->get(), lz4_out_buf_.data(), lz4_out_buf_.capacity(), nullptr);
            lz4_out_buf_pos_ += lz4_result;
            write_lz4_data();
        }
#endif

        void write(const block_t& data, [[maybe_unused]] bool is_header = false)
        {
            if (!is_open()) return;
#ifdef WITH_LZ4
            if (compression_ == compression_mode::lz4) {
                if (is_header) {
                    const auto lz4_result = lz4_call_and_check_error(LZ4F_uncompressedUpdate, lz4_ctx_->get(), lz4_out_buf_.data(), lz4_out_buf_.capacity(),
                                                                     data.data(), data.size(), nullptr);

                    lz4_out_buf_pos_ += lz4_result;
                    // flush is necessary to keep lz4_out_buf_pos_ consistent
                    lz4_flush();
                } else {
                    const auto lz4_result = lz4_call_and_check_error(LZ4F_compressUpdate, lz4_ctx_->get(), lz4_out_buf_.data(), lz4_out_buf_.capacity(),
                                                                     data.data(), data.size(), nullptr);
                    lz4_out_buf_pos_ += lz4_result;
                }
                write_lz4_data();
            } else {
#endif
                switch (mode_) {
                    case output_mode::stream_output:
                        callback_(data, data.size());
                        break;
                    case output_mode::file_output:
                        file_.write(data.data(), data.size());
                        break;
                }
#ifdef WITH_LZ4
            }
#endif
        }

        void finish()
        {
            block_t zeroes {};
            write(zeroes);
            write(zeroes);

#ifdef WITH_LZ4
            if (compression_ == compression_mode::lz4 && lz4_ctx_ != nullptr) {
                const auto lz4_result = lz4_call_and_check_error(LZ4F_compressEnd,
                                                                 lz4_ctx_->get(), lz4_out_buf_.data(), lz4_out_buf_.capacity(), nullptr);

                lz4_out_buf_pos_ += lz4_result;
                write_lz4_data();
            }
#endif
        }

        void write_header(const std::string& name, const file_type_flag& file_type)
        {
#ifdef __linux
            struct ::stat buffer {};
            const auto stat_result = ::stat(name.c_str(), &buffer);
            if (stat_result != 0) {
                throw errno_exception();
            }

            unsigned int dev_major = 0;
            unsigned int dev_minor = 0;
            // Store the device major number (for block or character devices).
            // todo write a test for this
            if (S_ISBLK(buffer.st_mode) || S_ISCHR(buffer.st_mode)) {
                dev_major = major(buffer.st_rdev);
                dev_minor = minor(buffer.st_rdev);
            }

            write_header(name, buffer.st_mode & ALLPERMS, buffer.st_uid, buffer.st_gid, buffer.st_size, buffer.st_mtim.tv_sec, file_type, dev_major, dev_minor);
#endif
        }

        void write_header(const std::string& name, __mode_t mode, __uid_t uid, __gid_t gid, __off_t size, __time_t time, const file_type_flag& file_type, unsigned int dev_major = 0, unsigned int dev_minor = 0)
        {
#ifdef __linux
            if (type_ != tar_type::unix_v7 && type_ != tar_type::ustar) throw std::logic_error("unsupported tar format");
            if (type_ == tar_type::unix_v7 && (static_cast<int>(file_type) > static_cast<int>(file_type_flag::SYMBOLIC_LINK))) throw std::logic_error("unsupported file type for tarv7 format");

            block_t header {};
            write_into_block(header, mode, UNIX_V7_USTAR_HEADER_POS_MODE, UNIX_V7_USTAR_HEADER_LEN_MODE);
            write_into_block(header, uid, UNIX_V7_USTAR_HEADER_POS_UID, UNIX_V7_USTAR_HEADER_LEN_UID);
            write_into_block(header, gid, UNIX_V7_USTAR_HEADER_POS_GID, UNIX_V7_USTAR_HEADER_LEN_GID);
            write_into_block(header, size, UNIX_V7_USTAR_HEADER_POS_SIZE, UNIX_V7_USTAR_HEADER_LEN_SIZE);
            write_into_block(header, time, UNIX_V7_USTAR_HEADER_POS_MTIM, UNIX_V7_USTAR_HEADER_LEN_MTIM);
            write_into_block(header, static_cast<char>(file_type), UNIX_V7_USTAR_HEADER_POS_TYPEFLAG, UNIX_V7_USTAR_HEADER_LEN_TYPEFLAG);
            write_name_and_prefix(header, name);
            //TODO link indicator (file type)
            //TODO name of linked file

            if (type_ == tar_type::ustar) {
                write_into_block(header, "ustar", USTAR_HEADER_POS_MAGIC, USTAR_HEADER_LEN_MAGIC);
                write_user_name(header, uid);
                write_group_name(header, gid);

                write_into_block(header, to_octal_ascii(dev_major, USTAR_HEADER_LEN_DEVMAJOR), USTAR_HEADER_POS_DEVMAJOR, USTAR_HEADER_LEN_DEVMAJOR);
                write_into_block(header, to_octal_ascii(dev_minor, USTAR_HEADER_LEN_DEVMAJOR), USTAR_HEADER_POS_DEVMINOR, USTAR_HEADER_LEN_DEVMINOR);
            }
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
            std::fill_n(block.data() + UNIX_V7_USTAR_HEADER_POS_CHECKSUM, UNIX_V7_USTAR_HEADER_LEN_CHKSUM, ' ');
            unsigned chksum = 0;
            for (unsigned char c : block) chksum += (c & 0xFF);
            write_into_block(block, chksum, UNIX_V7_USTAR_HEADER_POS_CHECKSUM, UNIX_V7_USTAR_HEADER_LEN_CHKSUM - 2);
            block[UNIX_V7_USTAR_HEADER_POS_CHECKSUM + UNIX_V7_USTAR_HEADER_LEN_CHKSUM - 1] = 0;
        }

        static std::string to_octal_ascii(unsigned long long value, unsigned width)
        {
            std::stringstream sstr;
            sstr << std::oct << std::ios::right << std::setfill('0') << std::setw(width) << value;
            auto str = sstr.str();
            if (str.size() > width) str = str.substr(str.size() - width);
            return str;
        }

        void write_name_and_prefix(block_t& block, const std::string& name)
        {
            const auto write_name_unix_v7_format = [&]() {
                write_into_block(block, name, UNIX_V7_USTAR_HEADER_POS_NAME, UNIX_V7_USTAR_HEADER_LEN_NAME);
            };

            if (name.size() <= UNIX_V7_USTAR_HEADER_LEN_NAME || type_ == tar_type::unix_v7) {
                write_name_unix_v7_format();
            } else {
                const auto last_slash_index = name.rfind('/');
                if (last_slash_index == std::string::npos) {
                    write_name_unix_v7_format();
                } else {
                    const auto remaining_length = name.size() - last_slash_index;
                    const auto length = remaining_length > UNIX_V7_USTAR_HEADER_LEN_NAME ? UNIX_V7_USTAR_HEADER_LEN_NAME : remaining_length;

                    write_into_block(block, name.c_str() + last_slash_index + 1, UNIX_V7_USTAR_HEADER_POS_NAME, length);
                    write_into_block(block, name, USTAR_HEADER_POS_PREFIX, last_slash_index);
                }
            }
        }

        static void write_user_name(block_t& block, __uid_t uid)
        {
#ifdef __linux
            const auto* const pwd = getpwuid(uid);
            // keep fields empty if we failed to get the name
            if (pwd == nullptr) return;

            write_into_block(block, pwd->pw_name, USTAR_HEADER_POS_UNAME, USTAR_HEADER_LEN_UNAME);
#endif
        }

        static void write_group_name(block_t& block, __gid_t gid)
        {
#ifdef __linux
            const auto* const group = getgrgid(gid);
            if (group == nullptr) return;
            write_into_block(block, group->gr_name, USTAR_HEADER_POS_GNAME, USTAR_HEADER_LEN_GNAME);
#endif
        }

        void check_state()
        {
            if (!is_open()) throw std::logic_error("Cannot add file, tar archive is not open");
            if (stream_file_header_pos_ >= 0) throw std::logic_error("Can't add new file while adding streaming data isn't completed");
        }

#ifdef WITH_LZ4
        void init_lz4()
        {
            if (compression_ != compression_mode::lz4) {
                return;
            }

            lz4_ctx_ = std::make_unique<lz4_ctx>();
            const auto outbuf_size = lz4_call_and_check_error(LZ4F_compressBound, 16 * 1024, &lz4_prefs_);

            lz4_out_buf_.reserve(outbuf_size);
            const auto headerSize = lz4_call_and_check_error(LZ4F_compressBegin, lz4_ctx_->get(), lz4_out_buf_.data(), lz4_out_buf_.capacity(), &lz4_prefs_);
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
#endif

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

#ifdef WITH_COMPRESSION
        compression_mode compression_ = compression_mode::none;
#endif

#ifdef WITH_LZ4
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
#endif
    };

} // namespace tarxx


#endif //TARXX_TARXX_H_F498949DFCF643A3B77C60CF3AA29F36
