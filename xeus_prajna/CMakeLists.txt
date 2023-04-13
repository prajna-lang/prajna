# include(cmake/openssl.cmake)

set(CMAKE_SYSTEM_PROGRAM_PATH /opt/homebrew/Cellar/llvm/15.0.4/bin/)

set(JSON_Install ON CACHE BOOL "" FORCE)
set(CPPZMQ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# set(XEUS_ZMQ_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(XEUS_ZMQ_STATIC_DEPENDENCIES ON CACHE BOOL "" FORCE)
# set(XEUS_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(XEUS_STATIC_DEPENDENCIES ON CACHE BOOL "" FORCE)

if (APPLE AND UNIX)
    set(ENABLE_WS OFF CACHE BOOL "" FORCE)
endif()

if (UNIX AND NOT APPLE)
    set(OPENSSL_INCLUDE_DIR /usr/include)
    set(OPENSSL_CRYPTO_LIBRARY /usr/lib/x86_64-linux-gnu/libcrypto.a)
endif()
if (UNIX AND APPLE)
    set(OPENSSL_INCLUDE_DIR /usr/local/include)
    set(OPENSSL_CRYPTO_LIBRARY /usr/local/lib/libcrypto.a)
endif()

add_subdirectory(third_party/json EXCLUDE_FROM_ALL)
add_subdirectory(third_party/xtl EXCLUDE_FROM_ALL)
add_subdirectory(third_party/xeus EXCLUDE_FROM_ALL)
add_subdirectory(third_party/libzmq EXCLUDE_FROM_ALL)
add_subdirectory(third_party/cppzmq EXCLUDE_FROM_ALL)
add_subdirectory(third_party/xeus-zmq EXCLUDE_FROM_ALL)
add_subdirectory(third_party/xproperty EXCLUDE_FROM_ALL)
add_subdirectory(third_party/xwidgets EXCLUDE_FROM_ALL)

add_executable(xeus_prajna main.cpp)
target_link_libraries(xeus_prajna
    PRIVATE fmt
    PRIVATE prajna_compiler
    PRIVATE cppzmq-static
    PRIVATE xeus-static
    PRIVATE xeus-zmq-static
    PRIVATE xwidgets-static
    PRIVATE ${CMAKE_DL_LIBS}
)
target_include_directories(xeus_prajna PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(xeus_prajna
    PRIVATE -Wno-unused-parameter
    PRIVATE -Wno-deprecated-declarations
)

install(TARGETS xeus_prajna EXPORT ${PROJECT_NAME}-targets)


