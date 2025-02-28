#----------------------------------------------------------------
# Generated CMake target import file for configuration "RelWithDebInfo".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "drmemory_annotations" for configuration "RelWithDebInfo"
set_property(TARGET drmemory_annotations APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drmemory_annotations PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "C"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libdrmemory_annotations.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS drmemory_annotations )
list(APPEND _IMPORT_CHECK_FILES_FOR_drmemory_annotations "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libdrmemory_annotations.a" )

# Import target "drsyscall" for configuration "RelWithDebInfo"
set_property(TARGET drsyscall APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drsyscall PROPERTIES
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libdrsyscall.so"
  IMPORTED_SONAME_RELWITHDEBINFO "libdrsyscall.so"
  )

list(APPEND _IMPORT_CHECK_TARGETS drsyscall )
list(APPEND _IMPORT_CHECK_FILES_FOR_drsyscall "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libdrsyscall.so" )

# Import target "drsyscall_static" for configuration "RelWithDebInfo"
set_property(TARGET drsyscall_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drsyscall_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "ASM;C"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libdrsyscall_static.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS drsyscall_static )
list(APPEND _IMPORT_CHECK_FILES_FOR_drsyscall_static "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libdrsyscall_static.a" )

# Import target "drsymcache" for configuration "RelWithDebInfo"
set_property(TARGET drsymcache APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drsymcache PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELWITHDEBINFO "drsyms"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libdrsymcache.so"
  IMPORTED_SONAME_RELWITHDEBINFO "libdrsymcache.so"
  )

list(APPEND _IMPORT_CHECK_TARGETS drsymcache )
list(APPEND _IMPORT_CHECK_FILES_FOR_drsymcache "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libdrsymcache.so" )

# Import target "drsymcache_static" for configuration "RelWithDebInfo"
set_property(TARGET drsymcache_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drsymcache_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "C"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libdrsymcache_static.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS drsymcache_static )
list(APPEND _IMPORT_CHECK_FILES_FOR_drsymcache_static "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libdrsymcache_static.a" )

# Import target "umbra" for configuration "RelWithDebInfo"
set_property(TARGET umbra APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(umbra PROPERTIES
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libumbra.so"
  IMPORTED_SONAME_RELWITHDEBINFO "libumbra.so"
  )

list(APPEND _IMPORT_CHECK_TARGETS umbra )
list(APPEND _IMPORT_CHECK_FILES_FOR_umbra "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libumbra.so" )

# Import target "umbra_static" for configuration "RelWithDebInfo"
set_property(TARGET umbra_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(umbra_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "C"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libumbra_static.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS umbra_static )
list(APPEND _IMPORT_CHECK_FILES_FOR_umbra_static "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libumbra_static.a" )

# Import target "drfuzz" for configuration "RelWithDebInfo"
set_property(TARGET drfuzz APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drfuzz PROPERTIES
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libdrfuzz.so"
  IMPORTED_SONAME_RELWITHDEBINFO "libdrfuzz.so"
  )

list(APPEND _IMPORT_CHECK_TARGETS drfuzz )
list(APPEND _IMPORT_CHECK_FILES_FOR_drfuzz "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libdrfuzz.so" )

# Import target "drfuzz_static" for configuration "RelWithDebInfo"
set_property(TARGET drfuzz_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drfuzz_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "C"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libdrfuzz_static.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS drfuzz_static )
list(APPEND _IMPORT_CHECK_FILES_FOR_drfuzz_static "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libdrfuzz_static.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
