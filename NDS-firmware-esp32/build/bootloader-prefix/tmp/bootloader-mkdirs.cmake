# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "D:/Espressif/Esp/frameworks/esp-idf-v4.4.3/components/bootloader/subproject"
  "D:/Espressif/PROJECTS/HTTP_SERVER_NDS/build/bootloader"
  "D:/Espressif/PROJECTS/HTTP_SERVER_NDS/build/bootloader-prefix"
  "D:/Espressif/PROJECTS/HTTP_SERVER_NDS/build/bootloader-prefix/tmp"
  "D:/Espressif/PROJECTS/HTTP_SERVER_NDS/build/bootloader-prefix/src/bootloader-stamp"
  "D:/Espressif/PROJECTS/HTTP_SERVER_NDS/build/bootloader-prefix/src"
  "D:/Espressif/PROJECTS/HTTP_SERVER_NDS/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/Espressif/PROJECTS/HTTP_SERVER_NDS/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
