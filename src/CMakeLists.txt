project(lib)

file(GLOB SRCS "*.c")

file(GLOB HEADERS "peer.h" "peer_connection.h" "peer_signaling.h" "streamout.h")

add_library(peer
  ${SRCS}
  ${HTTP_SOURCES}
  ${MQTT_SOURCES}
  ${MQTT_SERIALIZER_SOURCES}
)

include_directories(peer PUBLIC
  ${HTTP_INCLUDE_PUBLIC_DIRS}
  ${MQTT_INCLUDE_PUBLIC_DIRS}
)

add_dependencies(peer cjson mbedtls usrsctp srtp2)

find_package(PkgConfig REQUIRED)
pkg_check_modules(JSON_GLIB REQUIRED json-glib-1.0)
pkg_check_modules(GST REQUIRED gstreamer-1.0>=1.4 gstreamer-base-1.0>=1.4 gstreamer-app-1.0)
pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)

include_directories(${JSON_GLIB_INCLUDE_DIRS} )
include_directories(${GST_INCLUDE_DIRS} )
include_directories(${LIBWEBSOCKETS_INCLUDE_DIRS} )

link_directories(${JSON_GLIB_LIBRARY_DIRS}, ${LIBWEBSOCKETS_LIBRARY_DIRS} ${GST_LIBRARY_DIRS})

add_definitions(${JSON_GLIB_CFLAGS_OTHER})
add_definitions(${LIBWEBSOCKETS_CFLAGS_OTHER})

target_link_libraries(peer ${DEP_LIBS} ${JSON_GLIB_LIBRARIES} ${LIBWEBSOCKETS_LIBRARIES} ${GST_LIBRARIES})

set_target_properties(peer PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(TARGETS peer
  LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/
)

