# tarxx

`tarxx` is intended to be a modern C++ (i.e. C++17) header-only 
library for dealing with `tar`-format files.

This was written for self-educational purposes only and is (at 
least for the moment) merely a quick rough hack and by no means 
complete or finished. It might get better over time, it might 
get more complete over time and it might get more polished over 
time - in every aspect of it.


## Contributors

A big thank you for contributions by
* Alexander Mohr, Mercedes-Benz Tech Innovation GmbH

## Tests

To build the tests configure the project with `-DWITH_TESTS=ON`. 
Run tests with these commands
```shell
./tests/unit-tests/unit-tests 
./tests/component-tests/component-tests
```

## Version history

### 0.3.0

- full UNIX v7 support for writing tars
  (including directories, links, special files)
- full ustar support for writing tars
- platform abstraction layer (but still Linux supported only)
- add directories/files from filesystem with another name to the archive
- fixes and general code improvements
- more and improved testing

### 0.2.0

- support for adding in-memory data to the archive as a file
- general enabling for compressed archives
- lz4 compression support
- automated CI builds through github

### 0.1.0

- initial version
- support for UNIX v7 tar format
- support for adding files
- limitation: only Linux platforms currently supported
- limitation: file links not properly implemented
- support for two output methods: to file or to "stream"
  (data blocks handed out via callback)
- only very initial CMake skeleton, no install, no nothing
- no (automated) testing whatsoever


## License

This work is licensed under the BSD 2-Clasue License.
See [LICENSE](LICENSE) for details.

For the optional lz4 compression support, the lz4 library
by Yann Collet is being used. See [LZ4_LICENSE](LZ4_LICENSE)
for lz4 licensing details.
