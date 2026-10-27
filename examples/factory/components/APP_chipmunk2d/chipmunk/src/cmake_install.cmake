# Install script for directory: /home/wxggc/桌面/chipmunk/Chipmunk2D/src

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
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
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

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libchipmunk.so.7.0.3"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libchipmunk.so.7"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      file(RPATH_CHECK
           FILE "${file}"
           RPATH "")
    endif()
  endforeach()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/src/libchipmunk.so.7.0.3"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/src/libchipmunk.so.7"
    )
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libchipmunk.so.7.0.3"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libchipmunk.so.7"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/usr/bin/strip" "${file}")
      endif()
    endif()
  endforeach()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libchipmunk.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libchipmunk.so")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libchipmunk.so"
         RPATH "")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/home/wxggc/桌面/chipmunk/Chipmunk2D/src/libchipmunk.so")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libchipmunk.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libchipmunk.so")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libchipmunk.so")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/home/wxggc/桌面/chipmunk/Chipmunk2D/src/libchipmunk.a")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/chipmunk" TYPE FILE FILES
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/chipmunk.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/chipmunk_ffi.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/chipmunk_private.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/chipmunk_structs.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/chipmunk_types.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/chipmunk_unsafe.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpArbiter.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpBB.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpBody.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpConstraint.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpDampedRotarySpring.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpDampedSpring.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpGearJoint.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpGrooveJoint.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpHastySpace.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpMarch.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpPinJoint.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpPivotJoint.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpPolyShape.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpPolyline.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpRatchetJoint.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpRobust.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpRotaryLimitJoint.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpShape.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpSimpleMotor.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpSlideJoint.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpSpace.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpSpatialIndex.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpTransform.h"
    "/home/wxggc/桌面/chipmunk/Chipmunk2D/include/chipmunk/cpVect.h"
    )
endif()

