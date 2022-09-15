#!/bin/bash
find . -name "*.h" -or -name "*.cpp" -not -path "*build*" | xargs clang-format -i

