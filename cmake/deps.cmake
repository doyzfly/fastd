find_package(BISON 2.5 REQUIRED)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads)
if(NOT CMAKE_USE_PTHREADS_INIT)
  MESSAGE(FATAL_ERROR "No pthread support found.")
endif(NOT CMAKE_USE_PTHREADS_INIT)

find_package(PkgConfig REQUIRED)
pkg_check_modules(UECC REQUIRED libuecc>=3)


set(NACL_INCLUDE_DIRS "")
set(NACL_CFLAGS_OTHER "")
set(NACL_LIBRARY_DIRS "")
set(NACL_LIBRARIES "")
set(NACL_LDFLAGS_OTHER "")

if(ENABLE_LIBSODIUM)
  pkg_check_modules(SODIUM libsodium)

  if(SODIUM_FOUND)
    set(NACL_INCLUDE_DIRS "${SODIUM_INCLUDE_DIRS}")
    foreach(dir "${SODIUM_INCLUDEDIR}" ${SODIUM_INCLUDE_DIRS})
      list(APPEND NACL_INCLUDE_DIRS "${dir}/sodium")
    endforeach(dir)

    set(NACL_CFLAGS_OTHER "${SODIUM_CFLAGS_OTHER}")
    set(NACL_LIBRARY_DIRS "${SODIUM_LIBRARY_DIRS}")
    set(NACL_LIBRARIES "${SODIUM_LIBRARIES}")
    set(NACL_LDFLAGS_OTHER "${SODIUM_LDFLAGS_OTHER}")
  endif(SODIUM_FOUND)
else(ENABLE_LIBSODIUM)
  find_package(NaCl)

  if(NACL_FOUND)
    set(NACL_INCLUDE_DIRS "${NACL_INCLUDE_DIR}")
    set(NACL_LIBRARIES "${NACL_LIBRARY}")
  endif(NACL_FOUND)
endif(ENABLE_LIBSODIUM)

set_property(GLOBAL PROPERTY NACL_REQUIRED FALSE)


if(ENABLE_OPENSSL)
  pkg_check_modules(OPENSSL_CRYPTO REQUIRED libcrypto)
else(ENABLE_OPENSSL)
  set(OPENSSL_CRYPTO_INCLUDE_DIRS "")
  set(OPENSSL_CRYPTO_CFLAGS_OTHER "")
  set(OPENSSL_CRYPTO_LIBRARY_DIRS "")
  set(OPENSSL_CRYPTO_LIBRARIES "")
  set(OPENSSL_CRYPTO_LDFLAGS_OTHER "")
endif(ENABLE_OPENSSL)

if(ENABLE_SYSTEMD)
  pkg_check_modules(SYSTEMD_DAEMON REQUIRED libsystemd-daemon)
else(ENABLE_SYSTEMD)
  set(SYSTEMD_DAEMON_INCLUDE_DIRS "")
  set(SYSTEMD_DAEMON_CFLAGS_OTHER "")
  set(SYSTEMD_DAEMON_LIBRARY_DIRS "")
  set(SYSTEMD_DAEMON_LIBRARIES "")
  set(SYSTEMD_DAEMON_LDFLAGS_OTHER "")
endif(ENABLE_SYSTEMD)


if(WITH_CAPABILITIES)
  find_package(CAP REQUIRED)
else(WITH_CAPABILITIES)
  set(CAP_INCLUDE_DIR "")
  set(CAP_LIBRARY "")
endif(WITH_CAPABILITIES)
