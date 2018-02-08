CMAKE_MINIMUM_REQUIRED(VERSION 2.9)
PROJECT(MoonState CXX)

SET(MoonState_Build_lib "YES")

# Default build type is Release
if(NOT CMAKE_BUILD_TYPE)
	SET(CMAKE_BUILD_TYPE "Release")
endif(NOT CMAKE_BUILD_TYPE)

# Default DPDK header include path is in the system headers
if(NOT DPDK_INCLUDE_DIR)
	SET(DPDK_INCLUDE_DIR "/usr/include/dpdk/")
endif(NOT DPDK_INCLUDE_DIR)

if(LIBMOON_BUILD_LIBRARY)
	SET(DPDK_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/libmoon/deps/dpdk/x86_64-native-linuxapp-gcc/include")
	SET(DPDK_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libmoon/deps/dpdk/x86_64-native-linuxapp-gcc/lib")
endif(LIBMOON_BUILD_LIBRARY)

#SET(CMAKE_CXX_FLAGS "-Wall -Wextra -std=c++14 -march=native -g -O0 -fno-omit-frame-pointer -stdlib=libc++ -fsanitize=address,undefined")
SET(CMAKE_CXX_FLAGS "-Wall -Wextra -std=c++14 -march=native -g -fno-omit-frame-pointer")

SET(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG -g3 -O0 -fno-optimize-sibling-calls -fno-inline")
SET(CMAKE_CXX_FLAGS_RELEASE "-O3")

INCLUDE_DIRECTORIES(
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/deps/concurrentqueue
	${CMAKE_CURRENT_SOURCE_DIR}/deps/tbb/include/
	${CMAKE_CURRENT_SOURCE_DIR}/deps/libsodium_install/include
	${CMAKE_CURRENT_SOURCE_DIR}/deps/sparsehash/src/
)

INCLUDE_DIRECTORIES(
	${DPDK_INCLUDE_DIR}
)

FILE(GLOB SOURCES src/*.cpp)

if(NOT LIBMOON_BUILD_LIBRARY)
SET(DPDK_LIBS
	rte_mempool
	dpdk
)
endif(NOT LIBMOON_BUILD_LIBRARY)

SET(ALL_LIBS
	dl
	pthread
	numa
	${DPDK_LIBS}
	libtbb.a
	libsodium.a
)

if(WITH_PCAP)
	SET(ALL_LIBS
		${ALL_LIBS}
		pcap
	)
	ADD_DEFINITIONS(-DWITH_PCAP)
endif(WITH_PCAP)

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/deps/tbb/libtbb.a
	PRE_LINK
	COMMAND $(MAKE) "extra_inc=big_iron.inc"
	COMMENT "Building TBB"
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/deps/tbb/
)
add_custom_target(TBB DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/deps/tbb/libtbb.a)

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/deps/libsodium_install/lib/libsodium.a
	PRE_LINK
	COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/deps/libsodium/configure"
	"--prefix=${CMAKE_CURRENT_SOURCE_DIR}/deps/libsodium_install" "--enable-opt"
	"--libdir=${CMAKE_CURRENT_SOURCE_DIR}/deps/libsodium_install/lib"
	COMMAND $(MAKE)
	COMMAND $(MAKE) "install"
	COMMENT "Building libsodium"
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/deps/libsodium/
)
add_custom_target(libsodium DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/deps/libsodium_install/lib/libsodium.a)

LINK_DIRECTORIES(
	${CMAKE_CURRENT_SOURCE_DIR}/deps/tbb
	${CMAKE_CURRENT_SOURCE_DIR}/deps/libsodium_install/lib
	${DPDK_LIB_DIR}
)

ADD_LIBRARY(${PROJECT_NAME} STATIC ${SOURCES})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${ALL_LIBS})

add_dependencies(${PROJECT_NAME} TBB libsodium)

#ADD_EXECUTABLE(${PROJECT_NAME} ${SOURCES})
#TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${ALL_LIBS})

# For some reason, the tests don't compile for MoonGen...
if(NOT LIBMOON_BUILD_LIBRARY)
	ENABLE_TESTING()
	ADD_SUBDIRECTORY(tests)
	ADD_SUBDIRECTORY(benchmarks)
endif(NOT LIBMOON_BUILD_LIBRARY)
