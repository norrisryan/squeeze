# Install script for directory: /home/norris/RESEARCH/EHT/squeeze/lib/cfitsio

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

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

if(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified")
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/usr/local/lib/libcfitsio.a")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
file(INSTALL DESTINATION "/usr/local/lib" TYPE STATIC_LIBRARY FILES "/home/norris/RESEARCH/EHT/squeeze/lib/cfitsio/libcfitsio.a")
endif()

if(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Devel")
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/usr/local/include/longnam.h;/usr/local/include/region.h;/usr/local/include/grparser.h;/usr/local/include/f77_wrap.h;/usr/local/include/cfortran.h;/usr/local/include/eval_defs.h;/usr/local/include/fpack.h;/usr/local/include/fitsio2.h;/usr/local/include/group.h;/usr/local/include/drvrgsiftp.h;/usr/local/include/drvrsmem.h;/usr/local/include/simplerng.h;/usr/local/include/fitsio.h;/usr/local/include/eval_tab.h")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
file(INSTALL DESTINATION "/usr/local/include" TYPE FILE FILES
    "/home/norris/RESEARCH/EHT/squeeze/lib/cfitsio/longnam.h"
    "/home/norris/RESEARCH/EHT/squeeze/lib/cfitsio/region.h"
    "/home/norris/RESEARCH/EHT/squeeze/lib/cfitsio/grparser.h"
    "/home/norris/RESEARCH/EHT/squeeze/lib/cfitsio/f77_wrap.h"
    "/home/norris/RESEARCH/EHT/squeeze/lib/cfitsio/cfortran.h"
    "/home/norris/RESEARCH/EHT/squeeze/lib/cfitsio/eval_defs.h"
    "/home/norris/RESEARCH/EHT/squeeze/lib/cfitsio/fpack.h"
    "/home/norris/RESEARCH/EHT/squeeze/lib/cfitsio/fitsio2.h"
    "/home/norris/RESEARCH/EHT/squeeze/lib/cfitsio/group.h"
    "/home/norris/RESEARCH/EHT/squeeze/lib/cfitsio/drvrgsiftp.h"
    "/home/norris/RESEARCH/EHT/squeeze/lib/cfitsio/drvrsmem.h"
    "/home/norris/RESEARCH/EHT/squeeze/lib/cfitsio/simplerng.h"
    "/home/norris/RESEARCH/EHT/squeeze/lib/cfitsio/fitsio.h"
    "/home/norris/RESEARCH/EHT/squeeze/lib/cfitsio/eval_tab.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "/home/norris/RESEARCH/EHT/squeeze/lib/cfitsio/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
