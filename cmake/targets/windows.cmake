# windows specific target definitions
set_target_properties(sunshine PROPERTIES LINK_SEARCH_START_STATIC 1)
set(CMAKE_FIND_LIBRARY_SUFFIXES ".dll")
find_library(ZLIB ZLIB1)
list(APPEND SUNSHINE_EXTERNAL_LIBRARIES
        $<TARGET_OBJECTS:sunshine_rc_object>
        Windowsapp.lib
        Wtsapi32.lib)

# Strip symbols from the Release binary. The unstripped build ships ~58 MB with
# full DWARF + a symbol table that maps the auth/crypto code; keep symbols only
# in Debug. Use `objcopy --only-keep-debug` first if you want a separate symbol
# file for release crash analysis.
target_link_options(sunshine PRIVATE $<$<CONFIG:Release>:-s>)
