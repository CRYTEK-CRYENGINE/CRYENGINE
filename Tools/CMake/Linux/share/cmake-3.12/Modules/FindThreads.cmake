# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindThreads
# -----------
#
# This module determines the thread library of the system.
#
# The following variables are set
#
# ::
#
#   CMAKE_THREAD_LIBS_INIT     - the thread library
#   CMAKE_USE_SPROC_INIT       - are we using sproc?
#   CMAKE_USE_WIN32_THREADS_INIT - using WIN32 threads?
#   CMAKE_USE_PTHREADS_INIT    - are we using pthreads
#   CMAKE_HP_PTHREADS_INIT     - are we using hp pthreads
#
# The following import target is created
#
# ::
#
#   Threads::Threads
#
# For systems with multiple thread libraries, caller can set
#
# ::
#
#   CMAKE_THREAD_PREFER_PTHREAD
#
# If the use of the -pthread compiler and linker flag is preferred then the
# caller can set
#
# ::
#
#   THREADS_PREFER_PTHREAD_FLAG
#
# Please note that the compiler flag can only be used with the imported
# target. Use of both the imported target as well as this switch is highly
# recommended for new code.

include (CheckLibraryExists)
include (CheckSymbolExists)
set(Threads_FOUND FALSE)
set(CMAKE_REQUIRED_QUIET_SAVE ${CMAKE_REQUIRED_QUIET})
set(CMAKE_REQUIRED_QUIET ${Threads_FIND_QUIETLY})

if(CMAKE_C_COMPILER_LOADED)
  include (CheckIncludeFile)
elseif(CMAKE_CXX_COMPILER_LOADED)
  include (CheckIncludeFileCXX)
else()
  message(FATAL_ERROR "FindThreads only works if either C or CXX language is enabled")
endif()

# Do we have sproc?
if(CMAKE_SYSTEM_NAME MATCHES IRIX AND NOT CMAKE_THREAD_PREFER_PTHREAD)
  include (CheckIncludeFiles)
  CHECK_INCLUDE_FILES("sys/types.h;sys/prctl.h"  CMAKE_HAVE_SPROC_H)
endif()

# Internal helper macro.
# Do NOT even think about using it outside of this file!
macro(_check_threads_lib LIBNAME FUNCNAME VARNAME)
  if(NOT Threads_FOUND)
     CHECK_LIBRARY_EXISTS(${LIBNAME} ${FUNCNAME} "" ${VARNAME})
     if(${VARNAME})
       set(CMAKE_THREAD_LIBS_INIT "-l${LIBNAME}")
       set(CMAKE_HAVE_THREADS_LIBRARY 1)
       set(Threads_FOUND TRUE)
     endif()
  endif ()
endmacro()

# Internal helper macro.
# Do NOT even think about using it outside of this file!
macro(_check_pthreads_flag)
  if(NOT Threads_FOUND)
    # If we did not found -lpthread, -lpthread, or -lthread, look for -pthread
    if(NOT DEFINED THREADS_HAVE_PTHREAD_ARG)
      message(STATUS "Check if compiler accepts -pthread")
      if(CMAKE_C_COMPILER_LOADED)
        set(_threads_src ${CMAKE_CURRENT_LIST_DIR}/CheckForPthreads.c)
      elseif(CMAKE_CXX_COMPILER_LOADED)
        set(_threads_src ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/FindThreads/CheckForPthreads.cxx)
        configure_file(${CMAKE_CURRENT_LIST_DIR}/CheckForPthreads.c "${_threads_src}" COPYONLY)
      endif()
      try_compile(THREADS_HAVE_PTHREAD_ARG
        ${CMAKE_BINARY_DIR}
        ${_threads_src}
        CMAKE_FLAGS -DLINK_LIBRARIES:STRING=-pthread
        OUTPUT_VARIABLE OUTPUT)
      unset(_threads_src)

      if(THREADS_HAVE_PTHREAD_ARG)
        set(Threads_FOUND TRUE)
        message(STATUS "Check if compiler accepts -pthread - yes")
      else()
        message(STATUS "Check if compiler accepts -pthread - no")
        file(APPEND
          ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
          "Determining if compiler accepts -pthread failed with the following output:\n${OUTPUT}\n\n")
      endif()

    endif()

    if(THREADS_HAVE_PTHREAD_ARG)
      set(Threads_FOUND TRUE)
      set(CMAKE_THREAD_LIBS_INIT "-pthread")
    endif()
  endif()
endmacro()

if(CMAKE_HAVE_SPROC_H AND NOT CMAKE_THREAD_PREFER_PTHREAD)
  # We have sproc
  set(CMAKE_USE_SPROC_INIT 1)
else()
  # Do we have pthreads?
  if(CMAKE_C_COMPILER_LOADED)
    CHECK_INCLUDE_FILE("pthread.h" CMAKE_HAVE_PTHREAD_H)
  else()
    CHECK_INCLUDE_FILE_CXX("pthread.h" CMAKE_HAVE_PTHREAD_H)
  endif()
  if(CMAKE_HAVE_PTHREAD_H)

    #
    # We have pthread.h
    # Let's check for the library now.
    #
    set(CMAKE_HAVE_THREADS_LIBRARY)
    if(NOT THREADS_HAVE_PTHREAD_ARG)
      # Check if pthread functions are in normal C library
      CHECK_SYMBOL_EXISTS(pthread_create pthread.h CMAKE_HAVE_LIBC_CREATE)
      if(CMAKE_HAVE_LIBC_CREATE)
        set(CMAKE_THREAD_LIBS_INIT "")
        set(CMAKE_HAVE_THREADS_LIBRARY 1)
        set(Threads_FOUND TRUE)
      else()

        # Check for -pthread first if enabled. This is the recommended
        # way, but not backwards compatible as one must also pass -pthread
        # as compiler flag then.
        if (THREADS_PREFER_PTHREAD_FLAG)
           _check_pthreads_flag()
        endif ()

        _check_threads_lib(pthreads pthread_create CMAKE_HAVE_PTHREADS_CREATE)
        _check_threads_lib(pthread  pthread_create CMAKE_HAVE_PTHREAD_CREATE)
        if(CMAKE_SYSTEM_NAME MATCHES "SunOS")
            # On sun also check for -lthread
            _check_threads_lib(thread thr_create CMAKE_HAVE_THR_CREATE)
        endif()
      endif()
    endif()

    _check_pthreads_flag()
  endif()
endif()

if(CMAKE_THREAD_LIBS_INIT OR CMAKE_HAVE_LIBC_CREATE)
  set(CMAKE_USE_PTHREADS_INIT 1)
  set(Threads_FOUND TRUE)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(CMAKE_USE_WIN32_THREADS_INIT 1)
  set(Threads_FOUND TRUE)
endif()

if(CMAKE_USE_PTHREADS_INIT)
  if(CMAKE_SYSTEM_NAME MATCHES "HP-UX")
    # Use libcma if it exists and can be used.  It provides more
    # symbols than the plain pthread library.  CMA threads
    # have actually been deprecated:
    #   http://docs.hp.com/en/B3920-90091/ch12s03.html#d0e11395
    #   http://docs.hp.com/en/947/d8.html
    # but we need to maintain compatibility here.
    # The CMAKE_HP_PTHREADS setting actually indicates whether CMA threads
    # are available.
    CHECK_LIBRARY_EXISTS(cma pthread_attr_create "" CMAKE_HAVE_HP_CMA)
    if(CMAKE_HAVE_HP_CMA)
      set(CMAKE_THREAD_LIBS_INIT "-lcma")
      set(CMAKE_HP_PTHREADS_INIT 1)
      set(Threads_FOUND TRUE)
    endif()
    set(CMAKE_USE_PTHREADS_INIT 1)
  endif()

  if(CMAKE_SYSTEM MATCHES "OSF1-V")
    set(CMAKE_USE_PTHREADS_INIT 0)
    set(CMAKE_THREAD_LIBS_INIT )
  endif()

  if(CMAKE_SYSTEM MATCHES "CYGWIN_NT")
    set(CMAKE_USE_PTHREADS_INIT 1)
    set(Threads_FOUND TRUE)
    set(CMAKE_THREAD_LIBS_INIT )
    set(CMAKE_USE_WIN32_THREADS_INIT 0)
  endif()
endif()

set(CMAKE_REQUIRED_QUIET ${CMAKE_REQUIRED_QUIET_SAVE})
include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Threads DEFAULT_MSG Threads_FOUND)

if(THREADS_FOUND AND NOT TARGET Threads::Threads)
  add_library(Threads::Threads INTERFACE IMPORTED)

  if(THREADS_HAVE_PTHREAD_ARG)
    set_property(TARGET Threads::Threads PROPERTY INTERFACE_COMPILE_OPTIONS "-pthread")
  endif()

  if(CMAKE_THREAD_LIBS_INIT)
    set_property(TARGET Threads::Threads PROPERTY INTERFACE_LINK_LIBRARIES "${CMAKE_THREAD_LIBS_INIT}")
  endif()
endif()
