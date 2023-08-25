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


#ifndef TARXX_TARXX_H_F498949DFCF643A3B77C60CF3AA29F36
#define TARXX_TARXX_H_F498949DFCF643A3B77C60CF3AA29F36

#ifdef WITH_LZ4

#    include <lz4frame_static.h>

#endif

#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

#if defined(__linux)

#    include <cerrno>
#    include <fcntl.h>
#    include <ftw.h>
#    include <grp.h>
#    include <pwd.h>
#    include <sys/stat.h>
#    include <sys/sysmacros.h>
#    include <sys/types.h>
#    include <unistd.h>

#else
#    error "no support for targeted platform"
#endif


namespace tarxx {

    using std::string_literals::operator""s;
    enum class file_type_flag : char {
        REGULAR_FILE = '0',
        HARD_LINK = '1',
        SYMBOLIC_LINK = '2',
        CHARACTER_SPECIAL_FILE = '3',
        BLOCK_SPECIAL_FILE = '4',
        DIRECTORY = '5',
        FIFO = '6',
        // contiguous files are not supported by the current implementation
        CONTIGUOUS_FILE = '7',
    };

#if defined(__linux)

    struct errno_exception : std::system_error {
        errno_exception() : std::system_error(std::error_code(errno, std::generic_category())) {}

        using std::system_error::system_error;
    };

#endif

    static constexpr int BLOCK_SIZE = 512;
    using block_t = std::array<char, BLOCK_SIZE>;
    using uid_t = uint32_t;
    using gid_t = uint32_t;
    using mod_time_t = int64_t;
    using size_t = uint64_t;
    using major_t = uint32_t;
    using minor_t = uint32_t;

    using mode_t = uint32_t;
    enum class permission_t : unsigned int {
        none = 0,
        owner_read = 0400,
        owner_write = 0200,
        owner_exec = 0100,
        owner_all = 0700,
        group_read = 040,
        group_write = 020,
        group_exec = 010,
        group_all = 070,
        others_read = 04,
        others_write = 02,
        others_exec = 01,
        others_all = 07,
        all_all = static_cast<unsigned int>(tarxx::permission_t::owner_all) |
                  static_cast<unsigned int>(tarxx::permission_t::group_all) |
                  static_cast<unsigned int>(tarxx::permission_t::others_all),
        mask = 07777
    };

    struct Filesystem {
        virtual void iterateDirectory(const std::string& path, std::function<void(const std::string&)>&& cb) const = 0;

        [[nodiscard]] virtual file_type_flag type_flag(const std::string& path) const = 0;
        [[nodiscard]] virtual size_t file_size(const std::string& path) const = 0;
        [[nodiscard]] virtual mod_time_t mod_time(const std::string& path) const = 0;
        [[nodiscard]] virtual mode_t mode(const std::string& path) const = 0;
        [[nodiscard]] virtual std::string read_symlink(const std::string& path) const = 0;
        [[nodiscard]] virtual bool file_exists(const std::string& path) const = 0;
        [[nodiscard]] virtual std::optional<std::string> file_equivalent_present(const std::string& path, const std::unordered_set<std::string>& stored_files) const = 0;
        [[nodiscard]] virtual std::string relative_path(std::string path) const
        {
            if (path.empty()) return "";
            if (path == "../") return "./";
            if (path == "/") throw std::invalid_argument("can't tar the rootfs");
            if (path.find('/') == 0) {
                path = path.erase(0, 1);
                return relative_path(path);
            }

            if (path.find("..") == 0) {
                path = path.erase(0, 2);
                return relative_path(path);
            }

            return path;
        };

        static char file_type_to_char(const tarxx::file_type_flag& type)
        {
            switch (type) {
                case tarxx::file_type_flag::SYMBOLIC_LINK:
                    return 'l';
                case tarxx::file_type_flag::CHARACTER_SPECIAL_FILE:
                    return 'c';
                case tarxx::file_type_flag::BLOCK_SPECIAL_FILE:
                    return 'b';
                case tarxx::file_type_flag::DIRECTORY:
                    return 'd';
                case tarxx::file_type_flag::FIFO:
                    return 'p';
                case tarxx::file_type_flag::REGULAR_FILE:
                    [[fallthrough]];
                case tarxx::file_type_flag::HARD_LINK:
                    [[fallthrough]];
                case tarxx::file_type_flag::CONTIGUOUS_FILE:
                    [[fallthrough]];
                default:
                    return '-';
            }
        }

        [[nodiscard]] std::string permissions_str(const std::string& path) const
        {
            const auto permissions = mode(path) & static_cast<unsigned int>(permission_t::mask);
            std::bitset<9> bits(permissions);
            std::string str = "----------";
            for (int i = 0; i < bits.size(); i++) {
                const auto c =
                        (i % 3 == 2) ? 'r'
                                     : ((i % 3 == 1)
                                                ? 'w'
                                                : 'x');
                if (static_cast<bool>(bits[i])) {
                    str[i] = c;
                }
            }
            std::reverse(str.begin(), str.end());
            const auto type = type_flag(path);
            str[0] = file_type_to_char(type);
            return str;
        }
    };

    struct OS {
        [[nodiscard]] virtual uid_t user_id() const = 0;
        [[nodiscard]] virtual gid_t group_id() const = 0;
        [[nodiscard]] virtual std::string user_name(uid_t uid) const = 0;
        [[nodiscard]] virtual std::string group_name(gid_t gid) const = 0;
        [[nodiscard]] virtual uid_t file_owner(const std::string& path) const = 0;
        [[nodiscard]] virtual gid_t file_group(const std::string& path) const = 0;
        virtual void major_minor(const std::string& path, major_t& major, minor_t& minor) const = 0;
        [[nodiscard]] virtual char path_separator() const = 0;
        [[nodiscard]] virtual int truncate(const std::string& path, long length) const = 0;
    };

    struct StdFilesytem : public Filesystem {
        void iterateDirectory(const std::string& path, std::function<void(const std::string&)>&& cb) const override
        {
            if (std::filesystem::is_directory(std::filesystem::path(path))) {
                cb(path);
                for (const auto& f : std::filesystem::recursive_directory_iterator(path)) {
                    cb(f.path().string());
                }
            } else {
                cb(path);
            }
        }

        [[nodiscard]] file_type_flag type_flag(const std::string& path) const override
        {
            // check symlink first because according to std::filesystem
            // symlinks are also regular files, even though cppreference states otherwise.
            if (std::filesystem::is_symlink(path)) {
                return file_type_flag::SYMBOLIC_LINK;
            } else if (std::filesystem::is_block_file(path)) {
                return file_type_flag::BLOCK_SPECIAL_FILE;
            } else if (std::filesystem::is_character_file(path)) {
                return file_type_flag::CHARACTER_SPECIAL_FILE;
            } else if (std::filesystem::is_regular_file(path)) {
                return file_type_flag::REGULAR_FILE;
            } else if (std::filesystem::is_directory(path)) {
                return file_type_flag::DIRECTORY;
            } else if (std::filesystem::is_fifo(path)) {
                return file_type_flag::FIFO;
            } else {
                throw std::invalid_argument("Path is of an unsupported type");
            }
        }

        [[nodiscard]] uint64_t file_size(const std::string& path) const override
        {
            return std::filesystem::file_size(path);
        }

        [[nodiscard]] mod_time_t mod_time(const std::string& path) const override
        {
            auto file_time = std::filesystem::last_write_time(path);
            using clock = std::chrono::system_clock;
            auto system_time = std::chrono::time_point_cast<clock::duration>(
                    file_time - decltype(file_time)::clock::now() + clock::now());
            const auto time_t = clock::to_time_t(system_time);
            return time_t;
        }

        [[nodiscard]] mode_t mode(const std::string& path) const override
        {
            const auto status = std::filesystem::status(path);
            const auto perms = status.permissions();
            using std_perms = std::filesystem::perms;
            mode_t mode = 0;

            if ((perms & std_perms::owner_read) == std_perms::owner_read) {
                mode |= static_cast<unsigned int>(permission_t::owner_read);
            }
            if ((perms & std_perms::owner_write) == std_perms::owner_write) {
                mode |= static_cast<unsigned int>(permission_t::owner_write);
            }
            if ((perms & std_perms::owner_exec) == std_perms::owner_exec) {
                mode |= static_cast<unsigned int>(permission_t::owner_exec);
            }
            if ((perms & std_perms::group_read) == std_perms::group_read) {
                mode |= static_cast<unsigned int>(permission_t::group_read);
            }
            if ((perms & std_perms::group_write) == std_perms::group_write) {
                mode |= static_cast<unsigned int>(permission_t::group_write);
            }
            if ((perms & std_perms::group_exec) == std_perms::group_exec) {
                mode |= static_cast<unsigned int>(permission_t::group_exec);
            }
            if ((perms & std_perms::others_read) == std_perms::others_read) {
                mode |= static_cast<unsigned int>(permission_t::others_read);
            }
            if ((perms & std_perms::others_write) == std_perms::others_write) {
                mode |= static_cast<unsigned int>(permission_t::others_write);
            }
            if ((perms & std_perms::others_exec) == std_perms::others_exec) {
                mode |= static_cast<unsigned int>(permission_t::others_exec);
            }

            return mode;
        }

        [[nodiscard]] std::string read_symlink(const std::string& path) const override
        {
            return std::filesystem::read_symlink(path);
        }

        [[nodiscard]] bool file_exists(const std::string& path) const override
        {
            return std::filesystem::exists(path);
        };

        [[nodiscard]] std::optional<std::string> file_equivalent_present(const std::string& path,
                                                                         const std::unordered_set<std::string>& stored_files) const override
        {
            for (const auto& f : stored_files) {
                if (std::filesystem::equivalent(f, path)) {
                    return f;
                }
            }
            return {};
        }
    };

#if defined(__linux)

    struct PosixOS : OS {
        [[nodiscard]] std::string user_name(uid_t uid) const override
        {
            const auto* const pwd = getpwuid(uid);
            // keep fields empty if we failed to get the name
            if (pwd == nullptr) return std::to_string(uid);
            return pwd->pw_name;
        }

        [[nodiscard]] std::string group_name(gid_t gid) const override
        {
            const auto* const group = getgrgid(gid);
            if (group == nullptr) return std::to_string(gid);
            return group->gr_name;
        }

        [[nodiscard]] uid_t user_id() const override
        {
            const auto pw = passwd();
            return pw == nullptr ? std::numeric_limits<decltype(pw->pw_uid)>::max() : pw->pw_uid;
        }

        [[nodiscard]] gid_t group_id() const override
        {
            const auto pw = passwd();
            return pw == nullptr ? std::numeric_limits<decltype(pw->pw_uid)>::max() : pw->pw_gid;
        }

        void major_minor(const std::string& path, major_t& major, minor_t& minor) const override
        {
            const auto file_stat = get_stat(path);
            major = major(file_stat.st_rdev);
            minor = minor(file_stat.st_rdev);
        }

        [[nodiscard]] uid_t file_owner(const std::string& path) const override
        {
            const auto file_stat = get_stat(path);
            return file_stat.st_uid;
        };

        [[nodiscard]] gid_t file_group(const std::string& path) const override
        {
            const auto file_stat = get_stat(path);
            return file_stat.st_gid;
        };

        [[nodiscard]] char path_separator() const override
        {
            return '/';
        }

        [[nodiscard]] int truncate(const std::string& path, const long length) const override
        {
            return ::truncate(path.c_str(), static_cast<off_t>(length));
        }


    protected:
        static struct passwd* passwd()
        {
            const auto uid = geteuid();
            return getpwuid(uid);
        }

        static struct stat get_stat(const std::string& path)
        {
            struct stat file_stat {};
            const auto stat_res = stat(path.c_str(), &file_stat);
            if (stat_res != 0) throw errno_exception();
            return file_stat;
        }
    };

#endif

#if defined(__linux)
    struct Platform : public PosixOS, public StdFilesytem {
    };
#else
#    error "no support for targeted platform"
#endif

    struct tarfile {

        enum class tar_type {
            unix_v7,
            ustar,
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
              file_name_(filename),
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
            : file_(), file_name_(), callback_(std::move(callback)), mode_(output_mode::stream_output),
              compression_(compression),
              type_(type), stream_file_header_pos_(-1), stream_block_ {0}, stream_block_used_(0)
        {
#    ifdef WITH_LZ4
            init_lz4();
#    endif
        }

#else
        explicit tarfile(const std::string& filename, tar_type type = tar_type::unix_v7)
            : file_(filename, std::ios::out | std::ios::binary), file_name_(filename), callback_(nullptr), mode_(output_mode::file_output), type_(type), stream_block_ {0}, stream_file_header_pos_(-1), stream_block_used_(0)
        {
        }

        explicit tarfile(callback_t&& callback,
                         tar_type type = tar_type::unix_v7)
            : file_(), file_name_(), callback_(std::move(callback)), mode_(output_mode::stream_output),
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
                stored_files_.clear();
                if (is_open()) {
                    finish();
                    file_.close();
                    callback_ = nullptr;
                }
            } catch (const std::exception& ex) {
                // ignore exception in destructor, as they cannot be caught.
            }
        }

        void add_from_filesystem_recursive(const std::string& path)
        {
            if (platform_.type_flag(path) != file_type_flag::DIRECTORY) {
                add_from_filesystem(path);
            } else {
                platform_.iterateDirectory(path, [&](const std::string& callback_path) {
                    add_from_filesystem(callback_path);
                });
            }
        }

        void add_from_filesystem_recursive(const std::string& source_path, std::string target_path)
        {
            if (target_path.find("../") != std::string::npos) throw std::invalid_argument("target path can't contain ../");
            if (target_path.find("/..") != std::string::npos) throw std::invalid_argument("target path can't contain /..");
            if (target_path == "..") throw std::invalid_argument("target path can't be ..");
            if (target_path.empty()) throw std::invalid_argument("target path cannot be empty");
            if (target_path.rfind('/') == target_path.size() - 1) target_path = target_path.substr(0, target_path.size() - 1);

            if (platform_.type_flag(source_path) != file_type_flag::DIRECTORY) {
                add_from_filesystem(source_path, target_path);
            } else {
                platform_.iterateDirectory(source_path, [&](const std::string& callback_path) {
                    auto target = callback_path;
                    target.replace(target.begin(), target.begin() + source_path.size(), target_path);
                    add_from_filesystem(callback_path, target);
                });
            }
        }

        void add_from_filesystem(const std::string& filename)
        {
            check_state_and_flush();
            read_from_filesystem_write_to_tar(filename, filename);
        }

        void add_from_filesystem(const std::string& source_path, const std::string& target_path)
        {
            if (target_path.find("../") != std::string::npos) throw std::invalid_argument("target path can't contain ../");
            if (target_path.find("/..") != std::string::npos) throw std::invalid_argument("target path can't contain /..");
            if (target_path == "..") throw std::invalid_argument("target path can't be ..");
            if (target_path.empty()) throw std::invalid_argument("target path cannot be empty");

            if (platform_.type_flag(source_path) != file_type_flag::DIRECTORY && target_path.rfind('/') == target_path.size() - 1) {
                throw std::invalid_argument("target path can't end with / for non directories");
            }

            check_state_and_flush();
            read_from_filesystem_write_to_tar(source_path, target_path);
        }

        void add_symlink(const std::string& file_name, const std::string& link_name, uid_t uid, gid_t gid, mod_time_t time)
        {
            check_state_and_flush();
            write_header(link_name, static_cast<mode_t>(tarxx::permission_t::all_all), uid, gid, 0U, time,
                         file_type_flag::SYMBOLIC_LINK, 0U, 0U, file_name);
        }

        void add_hardlink(const std::string& file_name, const std::string& link_name, uid_t uid, gid_t gid,
                          mod_time_t time)
        {
            check_state_and_flush();
            write_header(link_name, static_cast<mode_t>(tarxx::permission_t::all_all), uid, gid, 0U, time,
                         file_type_flag::HARD_LINK, 0U, 0U, file_name);
        }

        void add_character_special_file(const std::string& name, mode_t mode, uid_t uid, gid_t gid, size_t size,
                                        mod_time_t time, major_t dev_major, minor_t dev_minor)
        {
            check_state_and_flush();
            write_header(name, mode, uid, gid, size, time, file_type_flag::CHARACTER_SPECIAL_FILE, dev_major,
                         dev_minor);
        }

        void add_block_special_file(const std::string& name, mode_t mode, uid_t uid, gid_t gid, size_t size, mod_time_t time,
                                    major_t dev_major, minor_t dev_minor)
        {
            check_state_and_flush();
            write_header(name, mode, uid, gid, size, time, file_type_flag::BLOCK_SPECIAL_FILE, dev_major, dev_minor);
        }

        void add_fifo(const std::string& name, mode_t mode, uid_t uid, gid_t gid, mod_time_t time)
        {
            check_state_and_flush();
            write_header(name, mode, uid, gid, 0, time, file_type_flag::FIFO);
        }

        void add_directory(const std::string& dirname, mode_t mode, uid_t uid, gid_t gid, mod_time_t mod_time)
        {
            check_state_and_flush();
            write_header(dirname, mode, uid, gid, 0, mod_time, file_type_flag::DIRECTORY);
        }

        void add_file_streaming()
        {
            if (mode_ != output_mode::file_output)
                throw std::logic_error(__func__ + " only supports output mode file"s);
            check_state_and_flush();

            // write empty header
            stream_file_header_pos_ = file_.tellg();
            block_t header {};
            write(header, true);
        }

        void add_file_streaming_data(const char* const data, std::streamsize size)
        {
            if (!is_open()) throw std::logic_error("Cannot append file, tar archive is not open");
            if (stream_file_header_pos_ < 0)
                throw std::logic_error("Can't stream file data, no file added via add_file_streaming");

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

        void stream_file_complete(const std::string& filename, mode_t mode, uid_t uid, gid_t gid, size_t size,
                                  mod_time_t mod_time)
        {
            if (stream_file_header_pos_ < 0) throw std::logic_error("Can't finish stream file, none is in progress");

            // create last block, 0 init to ensure correctness if size < block size
            block_t block {};
            std::copy_n(stream_block_.data(), stream_block_used_, block.data());
            stream_block_used_ = 0;
            write(block);

            // flush is necessary so seek to the correct positions
#ifdef WITH_LZ4
            if (compression_ == compression_mode::lz4) {
                lz4_flush();
            }
#endif

            // seek to header
            const auto stream_pos = file_.tellp();
            file_.seekp(stream_file_header_pos_);
            stream_file_header_pos_ = -1;
            write_header(filename, mode, uid, gid, size, mod_time, file_type_flag::REGULAR_FILE);
            file_.seekp(stream_pos);
        }

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
            lz4_out_buf_pos_ += lz4_call_and_check_error(LZ4F_flush, lz4_ctx_->get(), lz4_out_buf_.data(),
                                                         lz4_out_buf_.capacity(), nullptr);
            const auto lz4_result = lz4_call_and_check_error(LZ4F_flush, lz4_ctx_->get(), lz4_out_buf_.data(),
                                                             lz4_out_buf_.capacity(), nullptr);
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
                    const auto lz4_result = lz4_call_and_check_error(LZ4F_uncompressedUpdate, lz4_ctx_->get(),
                                                                     lz4_out_buf_.data(), lz4_out_buf_.capacity(),
                                                                     data.data(), data.size(), nullptr);

                    lz4_out_buf_pos_ += lz4_result;
                    // flush is necessary to keep lz4_out_buf_pos_ consistent
                    lz4_flush();
                } else {
                    const auto lz4_result = lz4_call_and_check_error(LZ4F_compressUpdate, lz4_ctx_->get(),
                                                                     lz4_out_buf_.data(), lz4_out_buf_.capacity(),
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
                                                                 lz4_ctx_->get(), lz4_out_buf_.data(),
                                                                 lz4_out_buf_.capacity(), nullptr);

                lz4_out_buf_pos_ += lz4_result;
                write_lz4_data();
            }
#endif
        }

        bool is_file_type_supported(const file_type_flag& type_flag)
        {
            const auto int_type_flag = static_cast<int>(type_flag);
            switch (type_) {
                case tar_type::unix_v7:
                    return type_flag == file_type_flag::DIRECTORY || int_type_flag <= static_cast<int>(file_type_flag::SYMBOLIC_LINK);
                case tar_type::ustar:
                    return true;
                default:
                    throw std::invalid_argument("type flag is not supported: " + std::to_string(int_type_flag));
            }
        }

        void write_regular_file_const_size(const std::string& name, const size_t expected_size)
        {
            block_t block {};
            std::fstream infile(name, std::ios::in | std::ios::binary);
            if (!infile.is_open()) throw std::runtime_error("Can't find input file " + name);
            size_t processed_bytes = 0;
            while (infile.good() && processed_bytes < expected_size) {
                infile.read(block.data(), block.size());
                const auto read = infile.gcount();
                processed_bytes += read;
                const auto write_size = processed_bytes < expected_size ? read : processed_bytes - expected_size;

                if (write_size < block.size()) std::fill_n(block.begin() + write_size, block.size() - write_size, 0);
                write(block);
            }

            std::fill_n(block.begin(), block.size(), 0);
            while (processed_bytes < expected_size) {
                write(block);
                processed_bytes += block.size();
            }
        }

        size_t write_regular_file_dynamic_size(const std::string& name)
        {
            block_t block {};
            std::fstream infile(name, std::ios::in | std::ios::binary);
            if (!infile.is_open()) throw std::runtime_error("Can't find input file " + name);
            long processed_bytes = 0;
            while (infile.good()) {
                infile.read(block.data(), block.size());
                const auto read = infile.gcount();
                processed_bytes += read;
                if (read < block.size()) std::fill_n(block.begin() + read, block.size() - read, 0);
                write(block);
            }

            return processed_bytes;
        }


        void read_from_filesystem_write_to_tar(const std::string& source_path, const std::string& target_path)
        {
            if (!platform_.file_exists(source_path)) throw std::invalid_argument(source_path + " does not exist");
            if (source_path == file_name_) throw std::invalid_argument("tar cannot be part of itself");

            std::function<void()> write_data;

            size_t size = 0;
            major_t dev_major = 0;
            minor_t dev_minor = 0;
            std::string link_name;
            const auto file_uid = platform_.file_owner(source_path);
            const auto file_gid = platform_.file_group(source_path);
            auto mode = platform_.mode(source_path);

            auto file_type = platform_.type_flag(source_path);
            const auto defer_header_writing = file_type == file_type_flag::REGULAR_FILE && mode_ == output_mode::file_output;

            // regular files should only be stored once in the archive
            // if the same file is added again (i.e. via a hard link)
            // we only store a new header but not the file again.
            if ((file_type == file_type_flag::REGULAR_FILE) || (file_type == file_type_flag::HARD_LINK)) {
                const auto equivalent = platform_.file_equivalent_present(source_path, stored_files_);
                if (equivalent.has_value()) {
                    link_name = equivalent.value();
                    file_type = file_type_flag::HARD_LINK;
                }
            }

            switch (file_type) {
                case file_type_flag::REGULAR_FILE:
                    write_data = [&]() {
                        if (defer_header_writing) {
                            size = write_regular_file_dynamic_size(source_path);
                        } else {
                            write_regular_file_const_size(source_path, size);
                        }
                    };
                    size = platform_.file_size(source_path);
                    break;
                case file_type_flag::CHARACTER_SPECIAL_FILE:
                    [[fallthrough]];
                case file_type_flag::BLOCK_SPECIAL_FILE:
                    platform_.major_minor(source_path, dev_major, dev_minor);
                    break;
                case file_type_flag::SYMBOLIC_LINK:
                    mode = static_cast<mode_t>(permission_t::all_all);
                    link_name = platform_.read_symlink(source_path);
                    break;
                case file_type_flag::CONTIGUOUS_FILE:
                    // won't happen as we would have to set the contiguous flag.
                    throw std::invalid_argument("contiguous files not supported");
                case file_type_flag::HARD_LINK:
                    [[fallthrough]];
                case file_type_flag::DIRECTORY:
                    [[fallthrough]];
                case file_type_flag::FIFO:
                    break;
            }

            // ignore unsupported file types.
            // i.e. directories for tar v7
            if (!is_file_type_supported(file_type)) {
                return;
            }

            const auto write_header_data = [&]() {
                write_header(
                        target_path,
                        mode,
                        file_uid,
                        file_gid,
                        size,
                        platform_.mod_time(source_path),
                        file_type,
                        dev_major,
                        dev_minor,
                        link_name);
            };

            if (defer_header_writing) {
#ifdef WITH_LZ4
                if (compression_ == compression_mode::lz4) {
                    lz4_flush();
                }
#endif
                auto header_pos = file_.tellp();
                block_t dummy_header {};
                write(dummy_header, true);

                if (write_data) {
                    write_data();
#ifdef WITH_LZ4
                    if (compression_ == compression_mode::lz4) {
                        lz4_flush();
                    }
#endif
                }

                const auto data_pos = file_.tellp();
                file_.seekp(header_pos);
                write_header_data();
                file_.seekp(data_pos);
            } else {
                write_header_data();
                if (write_data) {
                    write_data();
                }
            }
        }

        void write_header(std::string name, mode_t mode, uid_t uid, gid_t gid, size_t size, mod_time_t time,
                          file_type_flag file_type, major_t dev_major = 0, minor_t dev_minor = 0,
                          const std::string& link_name = "")
        {
            if (type_ != tar_type::unix_v7 && type_ != tar_type::ustar) throw std::logic_error("unsupported tar format");
            if (stream_file_header_pos_ > -1) throw std::logic_error("Can't write a header while file streaming is in progress");
            // allow adding files that consist only of a header multiple times
            // this is the same behaviour as gnu tar 1.30
            if (stored_files_.find(name) != stored_files_.end() && (file_type == file_type_flag::REGULAR_FILE || file_type == file_type_flag::CONTIGUOUS_FILE))
                throw std::logic_error("Can't add a file with the same name twice");

            if (file_type == file_type_flag::DIRECTORY) {
                // directories are regular files in unix v7
                if (type_ == tar_type::unix_v7) {
                    file_type = file_type_flag::REGULAR_FILE;
                }

                // directories should always be indicated with a trailing /
                if (name.at(name.size() - 1) != '/') {
                    name += "/";
                }
            }

            if (type_ == tar_type::unix_v7 && (static_cast<int>(file_type) > static_cast<int>(file_type_flag::SYMBOLIC_LINK)))
                throw std::logic_error("unsupported file type for unix_v7 format");

            stored_files_.insert(name);

            const auto store_name = platform_.relative_path(name);
            const auto& store_link = file_type == file_type_flag::SYMBOLIC_LINK
                                             ? link_name
                                             : platform_.relative_path(link_name);


            block_t header {};
            write_into_block(header, mode, UNIX_V7_USTAR_HEADER_POS_MODE, UNIX_V7_USTAR_HEADER_LEN_MODE);
            write_into_block(header, uid, UNIX_V7_USTAR_HEADER_POS_UID, UNIX_V7_USTAR_HEADER_LEN_UID);
            write_into_block(header, gid, UNIX_V7_USTAR_HEADER_POS_GID, UNIX_V7_USTAR_HEADER_LEN_GID);
            write_into_block(header, size, UNIX_V7_USTAR_HEADER_POS_SIZE, UNIX_V7_USTAR_HEADER_LEN_SIZE);
            write_into_block(header, time, UNIX_V7_USTAR_HEADER_POS_MTIM, UNIX_V7_USTAR_HEADER_LEN_MTIM);
            write_into_block(header, static_cast<char>(file_type), UNIX_V7_USTAR_HEADER_POS_TYPEFLAG,
                             UNIX_V7_USTAR_HEADER_LEN_TYPEFLAG);
            write_name_and_prefix(header, store_name);

            if (!link_name.empty()) {
                write_into_block(header, store_link, UNIX_V7_USTAR_HEADER_POS_LINKNAME,
                                 UNIX_V7_USTAR_HEADER_LEN_LINKNAME);
            }

            if (type_ == tar_type::ustar) {
                write_into_block(header, "ustar", USTAR_HEADER_POS_MAGIC, USTAR_HEADER_LEN_MAGIC);

                write_into_block(header, platform_.user_name(uid), USTAR_HEADER_POS_UNAME, USTAR_HEADER_LEN_UNAME);
                write_into_block(header, platform_.group_name(gid), USTAR_HEADER_POS_GNAME, USTAR_HEADER_LEN_GNAME);

                write_into_block(header, to_octal_ascii(dev_major, USTAR_HEADER_LEN_DEVMAJOR),
                                 USTAR_HEADER_POS_DEVMAJOR, USTAR_HEADER_LEN_DEVMAJOR);
                write_into_block(header, to_octal_ascii(dev_minor, USTAR_HEADER_LEN_DEVMAJOR),
                                 USTAR_HEADER_POS_DEVMINOR, USTAR_HEADER_LEN_DEVMINOR);
            }

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
                const auto last_separator_index = name.rfind(platform_.path_separator());
                if (last_separator_index == std::string::npos) {
                    write_name_unix_v7_format();
                } else {
                    // create the prefix by trimming the name to the prefix length
                    const auto prefix_trimmed = name.substr(0, USTAR_HEADER_LEN_PREFIX);
                    const auto last_separator_in_prefix = prefix_trimmed.rfind(platform_.path_separator());
                    if (last_separator_in_prefix == std::string::npos) {
                        // no possible delimiter found to splice part
                        // write name in unix v7 and truncate at the end
                        write_name_unix_v7_format();
                    } else {
                        // create the prefix by cutting at the found delimiter
                        const auto prefix = std::string(name.begin(), name.begin() + last_separator_in_prefix);
                        write_into_block(block, prefix, USTAR_HEADER_POS_PREFIX, USTAR_HEADER_LEN_PREFIX);

                        // name is remaining part of the string
                        const auto split_name = std::string(name.begin() + last_separator_in_prefix + 1, name.end());
                        write_into_block(block, split_name, UNIX_V7_USTAR_HEADER_POS_NAME, UNIX_V7_USTAR_HEADER_LEN_NAME);
                    }
                }
            }
        }

        void check_state_and_flush()
        {
            if (!is_open()) throw std::logic_error("Cannot add file, tar archive is not open");
            if (stream_file_header_pos_ >= 0)
                throw std::logic_error("Can't add new file while adding streaming data isn't completed");

#ifdef WITH_LZ4
            if (compression_ == compression_mode::lz4) {
                lz4_flush();
            }
#endif
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
            const auto headerSize = lz4_call_and_check_error(LZ4F_compressBegin, lz4_ctx_->get(), lz4_out_buf_.data(),
                                                             lz4_out_buf_.capacity(), &lz4_prefs_);
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

        std::string file_name_;
        std::fstream file_;
        callback_t callback_;
        long stream_file_header_pos_;
        block_t stream_block_;
        size_t stream_block_used_;

        Platform platform_;
        std::unordered_set<std::string> stored_files_;

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
                LZ4F_freeCompressionContext(ctx_);
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
