# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/ncs/v2.9.2/nrf52fun"
  "C:/ncs/v2.9.2/nrf52fun/build/nrf52fun"
  "C:/ncs/v2.9.2/nrf52fun/build/_sysbuild/sysbuild/images/nrf52fun-prefix"
  "C:/ncs/v2.9.2/nrf52fun/build/_sysbuild/sysbuild/images/nrf52fun-prefix/tmp"
  "C:/ncs/v2.9.2/nrf52fun/build/_sysbuild/sysbuild/images/nrf52fun-prefix/src/nrf52fun-stamp"
  "C:/ncs/v2.9.2/nrf52fun/build/_sysbuild/sysbuild/images/nrf52fun-prefix/src"
  "C:/ncs/v2.9.2/nrf52fun/build/_sysbuild/sysbuild/images/nrf52fun-prefix/src/nrf52fun-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/ncs/v2.9.2/nrf52fun/build/_sysbuild/sysbuild/images/nrf52fun-prefix/src/nrf52fun-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/ncs/v2.9.2/nrf52fun/build/_sysbuild/sysbuild/images/nrf52fun-prefix/src/nrf52fun-stamp${cfgdir}") # cfgdir has leading slash
endif()
