if (UNIX AND NOT APPLE)
    set(OPENSSL_INCLUDE_DIR /usr/include)
    set(OPENSSL_CRYPTO_LIBRARY /usr/lib/x86_64-linux-gnu/libcrypto.a)
endif()
if (UNIX AND APPLE)
    # install by homebrew
    set(OPENSSL_INCLUDE_DIR /opt/homebrew/include)
    set(OPENSSL_CRYPTO_LIBRARY /opt/homebrew/lib/libcrypto.a)
endif()

add_library(OpenSSL::Crypto STATIC IMPORTED)
set_target_properties(OpenSSL::Crypto PROPERTIES
  IMPORTED_LOCATION ${OPENSSL_CRYPTO_LIBRARY}
  INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIR}
)


