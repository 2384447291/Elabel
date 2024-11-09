# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "D:/Espressif/frameworks/esp-idf-v4.4.8/components/bootloader/subproject"
  "D:/fortag/esp32_code/Elabel/build/bootloader"
  "D:/fortag/esp32_code/Elabel/build/bootloader-prefix"
  "D:/fortag/esp32_code/Elabel/build/bootloader-prefix/tmp"
  "D:/fortag/esp32_code/Elabel/build/bootloader-prefix/src/bootloader-stamp"
  "D:/fortag/esp32_code/Elabel/build/bootloader-prefix/src"
  "D:/fortag/esp32_code/Elabel/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/fortag/esp32_code/Elabel/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
