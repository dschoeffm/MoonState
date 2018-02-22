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
	${CMAKE_CURRENT_SOURCE_DIR}/deps/openssl/include/
	${CMAKE_CURRENT_SOURCE_DIR}/deps/tbb/include/
	${CMAKE_CURRENT_SOURCE_DIR}/deps/AstraeusVPN/deps/libsodium_install/include
)

INCLUDE_DIRECTORIES(
	${DPDK_INCLUDE_DIR}
)

FILE(GLOB SOURCES src/*.cpp)

SET(OPENSSL_LIBS
	-Wl,--whole-archive
	libssl.a
	-Wl,--whole-archive
	libcrypto.a
	-Wl,--no-whole-archive
	dl
)

if(NOT LIBMOON_BUILD_LIBRARY)
SET(DPDK_LIBS
	rte_mempool
	dpdk
)
endif(NOT LIBMOON_BUILD_LIBRARY)

SET(ALL_LIBS
	pthread
	numa
	${OPENSSL_LIBS}
	${DPDK_LIBS}
	libtbb.a
	Astraeus
)

if(WITH_PCAP)
	SET(ALL_LIBS
		${ALL_LIBS}
		pcap
	)
	ADD_DEFINITIONS(-DWITH_PCAP)
endif(WITH_PCAP)

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/deps/openssl/libssl.a
	${CMAKE_CURRENT_SOURCE_DIR}/deps/openssl/libcrypto.a
	PRE_LINK
	COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/deps/openssl/config"
	COMMAND $(MAKE) "build_libs"
	COMMENT "Building OpenSSL - This may take a while"
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/deps/openssl/
)
add_custom_target(opensslBuild DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/deps/openssl/libssl.a
	${CMAKE_CURRENT_SOURCE_DIR}/deps/openssl/libcrypto.a)

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/tbb/libtbb.a
	PRE_LINK
	COMMAND $(MAKE) "extra_inc=big_iron.inc"
	COMMENT "Building TBB"
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/deps/tbb/
)
add_custom_target(TBB DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tbb/libtbb.a)

LINK_DIRECTORIES(
	${CMAKE_CURRENT_SOURCE_DIR}/deps/openssl
	${CMAKE_CURRENT_SOURCE_DIR}/deps/tbb
	${CMAKE_CURRENT_SOURCE_DIR}/deps/AstraeusVPN/deps/libsodium_install/lib
	${DPDK_LIB_DIR}
)

ADD_SUBDIRECTORY(deps/AstraeusVPN)

ADD_LIBRARY(${PROJECT_NAME} STATIC ${SOURCES})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${ALL_LIBS})

add_dependencies(${PROJECT_NAME} opensslBuild TBB)


#ADD_EXECUTABLE(${PROJECT_NAME} ${SOURCES})
#TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${ALL_LIBS})

# For some reason, the tests don't compile for MoonGen...
if(NOT LIBMOON_BUILD_LIBRARY)
	ENABLE_TESTING()
	ADD_SUBDIRECTORY(tests)
endif(NOT LIBMOON_BUILD_LIBRARY)
