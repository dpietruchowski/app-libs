add_library(app_securestorage_linux STATIC
    securestoragelinux.h
    securestoragelinux.cpp
)

target_link_libraries(app_securestorage_linux
    PUBLIC
        Qt6::Core
        PkgConfig::LIBSECRET
)

# glib's gio headers use `signals` as an identifier, which clashes with Qt's
# `signals` macro. This target uses no Qt keywords, so disable them here.
target_compile_definitions(app_securestorage_linux PRIVATE QT_NO_KEYWORDS)

target_include_directories(app_securestorage_linux
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)
