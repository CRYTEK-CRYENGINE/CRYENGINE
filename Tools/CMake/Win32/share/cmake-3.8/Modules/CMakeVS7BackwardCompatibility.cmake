# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


# hard code these for fast backwards compatibility tests
set (CMAKE_SIZEOF_INT       4   CACHE INTERNAL "Size of int data type")
set (CMAKE_SIZEOF_LONG      4   CACHE INTERNAL "Size of long data type")
set (CMAKE_SIZEOF_VOID_P    4   CACHE INTERNAL "Size of void* data type")
set (CMAKE_SIZEOF_CHAR      1   CACHE INTERNAL "Size of char data type")
set (CMAKE_SIZEOF_SHORT     2   CACHE INTERNAL "Size of short data type")
set (CMAKE_SIZEOF_FLOAT     4   CACHE INTERNAL "Size of float data type")
set (CMAKE_SIZEOF_DOUBLE    8   CACHE INTERNAL "Size of double data type")
set (CMAKE_NO_ANSI_FOR_SCOPE 0 CACHE INTERNAL
         "Does the compiler support ansi for scope.")
set (CMAKE_USE_WIN32_THREADS  TRUE CACHE BOOL    "Use the win32 thread library.")
set (CMAKE_WORDS_BIGENDIAN 0 CACHE INTERNAL "endianness of bytes")
