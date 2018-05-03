
#
# Build OpenSSL
#

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/openssl/libssl.a
	${CMAKE_CURRENT_SOURCE_DIR}/openssl/libcrypto.a
	PRE_LINK
	COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/openssl/config"
	COMMAND $(MAKE) "build_libs"
	COMMENT "Building OpenSSL - This may take a while"
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/openssl/
)
add_custom_target(openssl DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/openssl/libssl.a
	${CMAKE_CURRENT_SOURCE_DIR}/openssl/libcrypto.a)

#
# Build TBB
#

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/tbb/libtbb.a
	PRE_LINK
	COMMAND $(MAKE) "extra_inc=big_iron.inc"
	COMMENT "Building TBB"
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tbb/
)
add_custom_target(TBB DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tbb/libtbb.a)

#
# Build Sparsehash
#

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/sparsehash/src/sparsehash/internal/sparseconfig.h
	PRE_LINK
	COMMAND "./configure"
	COMMAND $(MAKE)
	COMMENT "Building Google sparsehash"
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sparsehash
)
add_custom_target(sparsehash DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sparsehash/src/sparsehash/internal/sparseconfig.h)

#
# Build libsodium
#

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/libsodium_install/lib/libsodium.a
	PRE_LINK
	COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/libsodium/configure"
	"--prefix=${CMAKE_CURRENT_SOURCE_DIR}/libsodium_install" "--enable-opt"
	"--libdir=${CMAKE_CURRENT_SOURCE_DIR}/libsodium_install/lib"
	COMMAND $(MAKE)
	COMMAND $(MAKE) "install"
	COMMENT "Building libsodium"
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libsodium/
)

add_custom_target(libsodium DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libsodium_install/lib/libsodium.a)

#
# Build Astraeus
#

# These libs are already contained in MoonState
SET(ASTRAEUSVPN_BUILD_TBB OFF CACHE BOOL "Build TBB in AstraeusVPN")
SET(ASTRAEUSVPN_BUILD_LIBSODIUM OFF CACHE BOOL "Build libsodium in AstraeusVPN")
SET(ASTRAEUSVPN_BUILD_HEXDUMP OFF CACHE BOOL "Build hexdump in AstraeusVPN")
SET(ASTRAEUSVPN_BUILD_BINARIES OFF CACHE BOOL "Build binaries for AstraeusVPN")
SET(ASTRAEUSVPN_BUILD_OPENSSL OFF CACHE BOOL "Build OpenSSL in AstraeusVPN")

ADD_SUBDIRECTORY(AstraeusVPN)
