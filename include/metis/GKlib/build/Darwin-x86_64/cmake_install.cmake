# Install script for directory: /Users/chenhuan/Downloads/metis-5.1.0/GKlib

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

if(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified")
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/Users/chenhuan/Downloads/metis-5.1.0/GKlib/build/Darwin-x86_64/libGKlib.a")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libGKlib.a" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libGKlib.a")
    execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/ranlib" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libGKlib.a")
  endif()
endif()

if(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified")
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE FILE FILES
    "/Users/chenhuan/Downloads/metis-5.1.0/GKlib/gk_arch.h"
    "/Users/chenhuan/Downloads/metis-5.1.0/GKlib/gk_defs.h"
    "/Users/chenhuan/Downloads/metis-5.1.0/GKlib/gk_externs.h"
    "/Users/chenhuan/Downloads/metis-5.1.0/GKlib/gk_getopt.h"
    "/Users/chenhuan/Downloads/metis-5.1.0/GKlib/gk_macros.h"
    "/Users/chenhuan/Downloads/metis-5.1.0/GKlib/gk_mkblas.h"
    "/Users/chenhuan/Downloads/metis-5.1.0/GKlib/gk_mkmemory.h"
    "/Users/chenhuan/Downloads/metis-5.1.0/GKlib/gk_mkpqueue.h"
    "/Users/chenhuan/Downloads/metis-5.1.0/GKlib/gk_mkpqueue2.h"
    "/Users/chenhuan/Downloads/metis-5.1.0/GKlib/gk_mkrandom.h"
    "/Users/chenhuan/Downloads/metis-5.1.0/GKlib/gk_mksort.h"
    "/Users/chenhuan/Downloads/metis-5.1.0/GKlib/gk_mkutils.h"
    "/Users/chenhuan/Downloads/metis-5.1.0/GKlib/gk_proto.h"
    "/Users/chenhuan/Downloads/metis-5.1.0/GKlib/gk_struct.h"
    "/Users/chenhuan/Downloads/metis-5.1.0/GKlib/gk_types.h"
    "/Users/chenhuan/Downloads/metis-5.1.0/GKlib/GKlib.h"
    "/Users/chenhuan/Downloads/metis-5.1.0/GKlib/gkregex.h"
    "/Users/chenhuan/Downloads/metis-5.1.0/GKlib/ms_inttypes.h"
    "/Users/chenhuan/Downloads/metis-5.1.0/GKlib/ms_stat.h"
    "/Users/chenhuan/Downloads/metis-5.1.0/GKlib/ms_stdint.h"
    )
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/Users/chenhuan/Downloads/metis-5.1.0/GKlib/build/Darwin-x86_64/test/cmake_install.cmake")

endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "/Users/chenhuan/Downloads/metis-5.1.0/GKlib/build/Darwin-x86_64/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
