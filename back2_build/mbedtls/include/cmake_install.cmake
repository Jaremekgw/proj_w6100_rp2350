# Install script for directory: /home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include

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
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
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

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/home/jaremekg/.pico-sdk/toolchain/14_2_Rel1/bin/arm-none-eabi-objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/mbedtls" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/aes.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/aria.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/asn1.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/asn1write.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/base64.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/bignum.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/block_cipher.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/build_info.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/camellia.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/ccm.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/chacha20.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/chachapoly.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/check_config.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/cipher.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/cmac.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/compat-2.x.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/config_adjust_legacy_crypto.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/config_adjust_legacy_from_psa.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/config_adjust_psa_from_legacy.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/config_adjust_psa_superset_legacy.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/config_adjust_ssl.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/config_adjust_x509.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/config_psa.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/constant_time.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/ctr_drbg.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/debug.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/des.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/dhm.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/ecdh.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/ecdsa.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/ecjpake.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/ecp.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/entropy.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/error.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/gcm.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/hkdf.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/hmac_drbg.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/lms.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/mbedtls_config.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/md.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/md5.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/memory_buffer_alloc.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/net_sockets.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/nist_kw.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/oid.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/pem.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/pk.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/pkcs12.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/pkcs5.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/pkcs7.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/platform.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/platform_time.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/platform_util.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/poly1305.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/private_access.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/psa_util.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/ripemd160.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/rsa.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/sha1.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/sha256.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/sha3.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/sha512.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/ssl.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/ssl_cache.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/ssl_ciphersuites.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/ssl_cookie.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/ssl_ticket.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/threading.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/timing.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/version.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/x509.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/x509_crl.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/x509_crt.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/mbedtls/x509_csr.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/psa" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/psa/build_info.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/psa/crypto.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/psa/crypto_adjust_auto_enabled.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/psa/crypto_adjust_config_key_pair_types.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/psa/crypto_adjust_config_synonyms.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/psa/crypto_builtin_composites.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/psa/crypto_builtin_key_derivation.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/psa/crypto_builtin_primitives.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/psa/crypto_compat.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/psa/crypto_config.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/psa/crypto_driver_common.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/psa/crypto_driver_contexts_composites.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/psa/crypto_driver_contexts_key_derivation.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/psa/crypto_driver_contexts_primitives.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/psa/crypto_extra.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/psa/crypto_legacy.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/psa/crypto_platform.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/psa/crypto_se_driver.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/psa/crypto_sizes.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/psa/crypto_struct.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/psa/crypto_types.h"
    "/home/jaremekg/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/include/psa/crypto_values.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/jaremekg/project/pico2/projects/build/mbedtls/include/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
