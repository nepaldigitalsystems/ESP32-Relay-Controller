# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "D:/Espressif/Esp/frameworks/esp-idf-v4.4.3/components/bootloader/subproject"
  "D:/GIT_NDS_WorkSpace/ESP32-Relay-Controller/NDS-firmware-esp32/build/bootloader"
  "D:/GIT_NDS_WorkSpace/ESP32-Relay-Controller/NDS-firmware-esp32/build/bootloader-prefix"
  "D:/GIT_NDS_WorkSpace/ESP32-Relay-Controller/NDS-firmware-esp32/build/bootloader-prefix/tmp"
  "D:/GIT_NDS_WorkSpace/ESP32-Relay-Controller/NDS-firmware-esp32/build/bootloader-prefix/src/bootloader-stamp"
  "D:/GIT_NDS_WorkSpace/ESP32-Relay-Controller/NDS-firmware-esp32/build/bootloader-prefix/src"
  "D:/GIT_NDS_WorkSpace/ESP32-Relay-Controller/NDS-firmware-esp32/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/GIT_NDS_WorkSpace/ESP32-Relay-Controller/NDS-firmware-esp32/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
