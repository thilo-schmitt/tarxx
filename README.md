# tarxx

`tarxx` is intended to be a modern C++ (i.e. C++17) header-only 
library for dealing with `tar`-format files.

This was written for self-educational purposes only and is (at 
least for the moment) merely a quick rough hack. It is far from 
what I would call a professional grade project and by no means 
complete or finished. It might get better over time, it might 
get more complete over time and it might get more polished over 
time - in every aspect of it.


## Version history

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
