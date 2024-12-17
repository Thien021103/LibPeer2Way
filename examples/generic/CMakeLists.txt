project(sample)

file(GLOB SRCS "*.c")

find_package(PkgConfig REQUIRED)

pkg_check_modules(GST REQUIRED gstreamer-1.0>=1.4 gstreamer-base-1.0>=1.4 gstreamer-app-1.0)

include_directories(${CMAKE_SOURCE_DIR}/src ${GST_INCLUDE_DIRS})

link_directories( ${GST_LIBRARY_DIRS})

add_executable(sample ${SRCS})

target_link_libraries(sample peer pthread ${GST_LIBRARIES})

