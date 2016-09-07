cmake_minimum_required(VERSION 2.8)
project(sdrplayserver)

find_library(MIR_LIBRARY NAMES mir_sdr mirsdrapi-rsp)
find_library(USB_LIBRARY NAMES usb usb-1.0)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O2")

set(SOURCE_FILES src/sdrplayserver.c)
#set(HEADER_FILES src/sdrplayserver.h)

#add_executable(sdrplayserver ${SOURCE_FILES} ${HEADER_FILES})
add_executable(sdrplayserver ${SOURCE_FILES} )
target_link_libraries(sdrplayserver ${MIR_LIBRARY} ${USB_LIBRARY})
install (TARGETS sdrplayserver DESTINATION bin)

