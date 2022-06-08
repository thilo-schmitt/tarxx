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

int tar_file_mode(const int argc, const char* const* const argv)
{
    if (argc < 2) return 1;
    tarxx::tarfile tar(argv[1] + std::string("_file.tar"));
    if (!tar.is_open()) return 2;
    for (unsigned i = 2; i < argc; ++i) {
        tar.add_file(argv[i]);
    }
    return 0;
}

int tar_streaming_mode(const int argc, const char* const* const argv)
{
    if (argc < 2) return 1;
    tarxx::tarfile tar(argv[1] + std::string("_stream.tar"));
    if (!tar.is_open()) return 2;
    for (unsigned i = 2; i < argc; ++i) {
        tar.add_file_streaming();
        std::ifstream infile(argv[i], std::ios::binary);
        std::array<char, 1466> buf {};
        std::streamsize file_size = 0;

        while (infile.good()) {
            infile.read(buf.data(), buf.size());
            const auto read = infile.gcount();
            file_size += read;
            tar.add_file_streaming_data(buf.data(), read);
        }

        tar.stream_file_complete(argv[i], 0777, 0, 0, file_size, 0);
    }
    return 0;
}

int main(const int argc, const char* const* const argv)
{
    auto returnValue = tar_file_mode(argc, argv);
    if (returnValue != 0) return returnValue;

    returnValue = tar_streaming_mode(argc, argv);
    return returnValue;
}
