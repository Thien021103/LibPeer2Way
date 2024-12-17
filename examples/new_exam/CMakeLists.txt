cmake_minimum_required(VERSION 3.1)

project(newexam)

file(GLOB SRCS "*.c")

# Find required libraries like GStreamer, pthread, etc.
find_package(PkgConfig REQUIRED)

pkg_check_modules(GST REQUIRED gstreamer-1.0>=1.4 gstreamer-base-1.0>=1.4 gstreamer-app-1.0)

# Add include directories
include_directories(${CMAKE_SOURCE_DIR}/src ${GST_INCLUDE_DIRS})

# Add library directories
link_directories( ${GST_LIBRARY_DIRS})

# Add the executable target
add_executable(newexam ${SRCS})

# Link libraries: peer (libpeer.so), pthread, and GStreamer
target_link_libraries(newexam peer pthread ${GST_LIBRARIES})



# Old CmakeLists.txt

# project(sample)

# file(GLOB SRCS "*.c")

# include_directories(${CMAKE_SOURCE_DIR}/src)

# add_executable(sample ${SRCS})

# target_link_libraries(sample peer pthread)

