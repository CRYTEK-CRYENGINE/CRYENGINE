# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindX11
# -------
#
# Find X11 installation
#
# Try to find X11 on UNIX systems. The following values are defined
#
# ::
#
#   X11_FOUND        - True if X11 is available
#   X11_INCLUDE_DIR  - include directories to use X11
#   X11_LIBRARIES    - link against these to use X11
#
# and also the following more fine grained variables:
#
# ::
#
#   X11_ICE_INCLUDE_PATH,          X11_ICE_LIB,        X11_ICE_FOUND
#   X11_SM_INCLUDE_PATH,           X11_SM_LIB,         X11_SM_FOUND
#   X11_X11_INCLUDE_PATH,          X11_X11_LIB
#   X11_Xaccessrules_INCLUDE_PATH,                     X11_Xaccess_FOUND
#   X11_Xaccessstr_INCLUDE_PATH,                       X11_Xaccess_FOUND
#   X11_Xau_INCLUDE_PATH,          X11_Xau_LIB,        X11_Xau_FOUND
#   X11_Xcomposite_INCLUDE_PATH,   X11_Xcomposite_LIB, X11_Xcomposite_FOUND
#   X11_Xcursor_INCLUDE_PATH,      X11_Xcursor_LIB,    X11_Xcursor_FOUND
#   X11_Xdamage_INCLUDE_PATH,      X11_Xdamage_LIB,    X11_Xdamage_FOUND
#   X11_Xdmcp_INCLUDE_PATH,        X11_Xdmcp_LIB,      X11_Xdmcp_FOUND
#   X11_Xext_LIB,       X11_Xext_FOUND
#   X11_dpms_INCLUDE_PATH,         (in X11_Xext_LIB),  X11_dpms_FOUND
#   X11_XShm_INCLUDE_PATH,         (in X11_Xext_LIB),  X11_XShm_FOUND
#   X11_Xshape_INCLUDE_PATH,       (in X11_Xext_LIB),  X11_Xshape_FOUND
#   X11_xf86misc_INCLUDE_PATH,     X11_Xxf86misc_LIB,  X11_xf86misc_FOUND
#   X11_xf86vmode_INCLUDE_PATH,    X11_Xxf86vm_LIB     X11_xf86vmode_FOUND
#   X11_Xfixes_INCLUDE_PATH,       X11_Xfixes_LIB,     X11_Xfixes_FOUND
#   X11_Xft_INCLUDE_PATH,          X11_Xft_LIB,        X11_Xft_FOUND
#   X11_Xi_INCLUDE_PATH,           X11_Xi_LIB,         X11_Xi_FOUND
#   X11_Xinerama_INCLUDE_PATH,     X11_Xinerama_LIB,   X11_Xinerama_FOUND
#   X11_Xinput_INCLUDE_PATH,       X11_Xinput_LIB,     X11_Xinput_FOUND
#   X11_Xkb_INCLUDE_PATH,                              X11_Xkb_FOUND
#   X11_Xkblib_INCLUDE_PATH,                           X11_Xkb_FOUND
#   X11_Xkbfile_INCLUDE_PATH,      X11_Xkbfile_LIB,    X11_Xkbfile_FOUND
#   X11_Xmu_INCLUDE_PATH,          X11_Xmu_LIB,        X11_Xmu_FOUND
#   X11_Xpm_INCLUDE_PATH,          X11_Xpm_LIB,        X11_Xpm_FOUND
#   X11_XTest_INCLUDE_PATH,        X11_XTest_LIB,      X11_XTest_FOUND
#   X11_Xrandr_INCLUDE_PATH,       X11_Xrandr_LIB,     X11_Xrandr_FOUND
#   X11_Xrender_INCLUDE_PATH,      X11_Xrender_LIB,    X11_Xrender_FOUND
#   X11_Xscreensaver_INCLUDE_PATH, X11_Xscreensaver_LIB, X11_Xscreensaver_FOUND
#   X11_Xt_INCLUDE_PATH,           X11_Xt_LIB,         X11_Xt_FOUND
#   X11_Xutil_INCLUDE_PATH,                            X11_Xutil_FOUND
#   X11_Xv_INCLUDE_PATH,           X11_Xv_LIB,         X11_Xv_FOUND
#   X11_XSync_INCLUDE_PATH,        (in X11_Xext_LIB),  X11_XSync_FOUND

if (UNIX)
  set(X11_FOUND 0)
  # X11 is never a framework and some header files may be
  # found in tcl on the mac
  set(CMAKE_FIND_FRAMEWORK_SAVE ${CMAKE_FIND_FRAMEWORK})
  set(CMAKE_FIND_FRAMEWORK NEVER)
  set(CMAKE_REQUIRED_QUIET_SAVE ${CMAKE_REQUIRED_QUIET})
  set(CMAKE_REQUIRED_QUIET ${X11_FIND_QUIETLY})
  set(X11_INC_SEARCH_PATH
    /usr/pkg/xorg/include
    /usr/X11R6/include
    /usr/X11R7/include
    /usr/include/X11
    /usr/openwin/include
    /usr/openwin/share/include
    /opt/graphics/OpenGL/include
    /opt/X11/include
  )

  set(X11_LIB_SEARCH_PATH
    /usr/pkg/xorg/lib
    /usr/X11R6/lib
    /usr/X11R7/lib
    /usr/openwin/lib
    /opt/X11/lib
  )

  find_path(X11_X11_INCLUDE_PATH X11/X.h                             ${X11_INC_SEARCH_PATH})
  find_path(X11_Xlib_INCLUDE_PATH X11/Xlib.h                         ${X11_INC_SEARCH_PATH})

  # Look for includes; keep the list sorted by name of the cmake *_INCLUDE_PATH
  # variable (which doesn't need to match the include file name).

  # Solaris lacks XKBrules.h, so we should skip kxkbd there.
  find_path(X11_ICE_INCLUDE_PATH X11/ICE/ICE.h                       ${X11_INC_SEARCH_PATH})
  find_path(X11_SM_INCLUDE_PATH X11/SM/SM.h                          ${X11_INC_SEARCH_PATH})
  find_path(X11_Xaccessrules_INCLUDE_PATH X11/extensions/XKBrules.h  ${X11_INC_SEARCH_PATH})
  find_path(X11_Xaccessstr_INCLUDE_PATH X11/extensions/XKBstr.h      ${X11_INC_SEARCH_PATH})
  find_path(X11_Xau_INCLUDE_PATH X11/Xauth.h                         ${X11_INC_SEARCH_PATH})
  find_path(X11_Xcomposite_INCLUDE_PATH X11/extensions/Xcomposite.h  ${X11_INC_SEARCH_PATH})
  find_path(X11_Xcursor_INCLUDE_PATH X11/Xcursor/Xcursor.h           ${X11_INC_SEARCH_PATH})
  find_path(X11_Xdamage_INCLUDE_PATH X11/extensions/Xdamage.h        ${X11_INC_SEARCH_PATH})
  find_path(X11_Xdmcp_INCLUDE_PATH X11/Xdmcp.h                       ${X11_INC_SEARCH_PATH})
  find_path(X11_dpms_INCLUDE_PATH X11/extensions/dpms.h              ${X11_INC_SEARCH_PATH})
  find_path(X11_xf86misc_INCLUDE_PATH X11/extensions/xf86misc.h      ${X11_INC_SEARCH_PATH})
  find_path(X11_xf86vmode_INCLUDE_PATH X11/extensions/xf86vmode.h    ${X11_INC_SEARCH_PATH})
  find_path(X11_Xfixes_INCLUDE_PATH X11/extensions/Xfixes.h          ${X11_INC_SEARCH_PATH})
  find_path(X11_Xft_INCLUDE_PATH X11/Xft/Xft.h                       ${X11_INC_SEARCH_PATH})
  find_path(X11_Xi_INCLUDE_PATH X11/extensions/XInput.h              ${X11_INC_SEARCH_PATH})
  find_path(X11_Xinerama_INCLUDE_PATH X11/extensions/Xinerama.h      ${X11_INC_SEARCH_PATH})
  find_path(X11_Xinput_INCLUDE_PATH X11/extensions/XInput.h          ${X11_INC_SEARCH_PATH})
  find_path(X11_Xkb_INCLUDE_PATH X11/extensions/XKB.h                ${X11_INC_SEARCH_PATH})
  find_path(X11_Xkblib_INCLUDE_PATH X11/XKBlib.h                     ${X11_INC_SEARCH_PATH})
  find_path(X11_Xkbfile_INCLUDE_PATH X11/extensions/XKBfile.h        ${X11_INC_SEARCH_PATH})
  find_path(X11_Xmu_INCLUDE_PATH X11/Xmu/Xmu.h                       ${X11_INC_SEARCH_PATH})
  find_path(X11_Xpm_INCLUDE_PATH X11/xpm.h                           ${X11_INC_SEARCH_PATH})
  find_path(X11_XTest_INCLUDE_PATH X11/extensions/XTest.h            ${X11_INC_SEARCH_PATH})
  find_path(X11_XShm_INCLUDE_PATH X11/extensions/XShm.h              ${X11_INC_SEARCH_PATH})
  find_path(X11_Xrandr_INCLUDE_PATH X11/extensions/Xrandr.h          ${X11_INC_SEARCH_PATH})
  find_path(X11_Xrender_INCLUDE_PATH X11/extensions/Xrender.h        ${X11_INC_SEARCH_PATH})
  find_path(X11_XRes_INCLUDE_PATH X11/extensions/XRes.h              ${X11_INC_SEARCH_PATH})
  find_path(X11_Xscreensaver_INCLUDE_PATH X11/extensions/scrnsaver.h ${X11_INC_SEARCH_PATH})
  find_path(X11_Xshape_INCLUDE_PATH X11/extensions/shape.h           ${X11_INC_SEARCH_PATH})
  find_path(X11_Xutil_INCLUDE_PATH X11/Xutil.h                       ${X11_INC_SEARCH_PATH})
  find_path(X11_Xt_INCLUDE_PATH X11/Intrinsic.h                      ${X11_INC_SEARCH_PATH})
  find_path(X11_Xv_INCLUDE_PATH X11/extensions/Xvlib.h               ${X11_INC_SEARCH_PATH})
  find_path(X11_XSync_INCLUDE_PATH X11/extensions/sync.h             ${X11_INC_SEARCH_PATH})


  find_library(X11_X11_LIB X11               ${X11_LIB_SEARCH_PATH})

  # Find additional X libraries. Keep list sorted by library name.
  find_library(X11_ICE_LIB ICE               ${X11_LIB_SEARCH_PATH})
  find_library(X11_SM_LIB SM                 ${X11_LIB_SEARCH_PATH})
  find_library(X11_Xau_LIB Xau               ${X11_LIB_SEARCH_PATH})
  find_library(X11_Xcomposite_LIB Xcomposite ${X11_LIB_SEARCH_PATH})
  find_library(X11_Xcursor_LIB Xcursor       ${X11_LIB_SEARCH_PATH})
  find_library(X11_Xdamage_LIB Xdamage       ${X11_LIB_SEARCH_PATH})
  find_library(X11_Xdmcp_LIB Xdmcp           ${X11_LIB_SEARCH_PATH})
  find_library(X11_Xext_LIB Xext             ${X11_LIB_SEARCH_PATH})
  find_library(X11_Xfixes_LIB Xfixes         ${X11_LIB_SEARCH_PATH})
  find_library(X11_Xft_LIB Xft               ${X11_LIB_SEARCH_PATH})
  find_library(X11_Xi_LIB Xi                 ${X11_LIB_SEARCH_PATH})
  find_library(X11_Xinerama_LIB Xinerama     ${X11_LIB_SEARCH_PATH})
  find_library(X11_Xinput_LIB Xi             ${X11_LIB_SEARCH_PATH})
  find_library(X11_Xkbfile_LIB xkbfile       ${X11_LIB_SEARCH_PATH})
  find_library(X11_Xmu_LIB Xmu               ${X11_LIB_SEARCH_PATH})
  find_library(X11_Xpm_LIB Xpm               ${X11_LIB_SEARCH_PATH})
  find_library(X11_Xrandr_LIB Xrandr         ${X11_LIB_SEARCH_PATH})
  find_library(X11_Xrender_LIB Xrender       ${X11_LIB_SEARCH_PATH})
  find_library(X11_XRes_LIB XRes             ${X11_LIB_SEARCH_PATH})
  find_library(X11_Xscreensaver_LIB Xss      ${X11_LIB_SEARCH_PATH})
  find_library(X11_Xt_LIB Xt                 ${X11_LIB_SEARCH_PATH})
  find_library(X11_XTest_LIB Xtst            ${X11_LIB_SEARCH_PATH})
  find_library(X11_Xv_LIB Xv                 ${X11_LIB_SEARCH_PATH})
  find_library(X11_Xxf86misc_LIB Xxf86misc   ${X11_LIB_SEARCH_PATH})
  find_library(X11_Xxf86vm_LIB Xxf86vm       ${X11_LIB_SEARCH_PATH})

  set(X11_LIBRARY_DIR "")
  if(X11_X11_LIB)
    get_filename_component(X11_LIBRARY_DIR ${X11_X11_LIB} PATH)
  endif()

  set(X11_INCLUDE_DIR) # start with empty list
  if(X11_X11_INCLUDE_PATH)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_X11_INCLUDE_PATH})
  endif()

  if(X11_Xlib_INCLUDE_PATH)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xlib_INCLUDE_PATH})
  endif()

  if(X11_Xutil_INCLUDE_PATH)
    set(X11_Xutil_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xutil_INCLUDE_PATH})
  endif()

  if(X11_Xshape_INCLUDE_PATH)
    set(X11_Xshape_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xshape_INCLUDE_PATH})
  endif()

  set(X11_LIBRARIES) # start with empty list
  if(X11_X11_LIB)
    set(X11_LIBRARIES ${X11_LIBRARIES} ${X11_X11_LIB})
  endif()

  if(X11_Xext_LIB)
    set(X11_Xext_FOUND TRUE)
    set(X11_LIBRARIES ${X11_LIBRARIES} ${X11_Xext_LIB})
  endif()

  if(X11_Xt_LIB AND X11_Xt_INCLUDE_PATH)
    set(X11_Xt_FOUND TRUE)
  endif()

  if(X11_Xft_LIB AND X11_Xft_INCLUDE_PATH)
    set(X11_Xft_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xft_INCLUDE_PATH})
  endif()

  if(X11_Xv_LIB AND X11_Xv_INCLUDE_PATH)
    set(X11_Xv_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xv_INCLUDE_PATH})
  endif()

  if (X11_Xau_LIB AND X11_Xau_INCLUDE_PATH)
    set(X11_Xau_FOUND TRUE)
  endif ()

  if (X11_Xdmcp_INCLUDE_PATH AND X11_Xdmcp_LIB)
      set(X11_Xdmcp_FOUND TRUE)
      set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xdmcp_INCLUDE_PATH})
  endif ()

  if (X11_Xaccessrules_INCLUDE_PATH AND X11_Xaccessstr_INCLUDE_PATH)
      set(X11_Xaccess_FOUND TRUE)
      set(X11_Xaccess_INCLUDE_PATH ${X11_Xaccessstr_INCLUDE_PATH})
      set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xaccess_INCLUDE_PATH})
  endif ()

  if (X11_Xpm_INCLUDE_PATH AND X11_Xpm_LIB)
      set(X11_Xpm_FOUND TRUE)
      set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xpm_INCLUDE_PATH})
  endif ()

  if (X11_Xcomposite_INCLUDE_PATH AND X11_Xcomposite_LIB)
     set(X11_Xcomposite_FOUND TRUE)
     set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xcomposite_INCLUDE_PATH})
  endif ()

  if (X11_Xdamage_INCLUDE_PATH AND X11_Xdamage_LIB)
     set(X11_Xdamage_FOUND TRUE)
     set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xdamage_INCLUDE_PATH})
  endif ()

  if (X11_XShm_INCLUDE_PATH)
     set(X11_XShm_FOUND TRUE)
     set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_XShm_INCLUDE_PATH})
  endif ()

  if (X11_XTest_INCLUDE_PATH AND X11_XTest_LIB)
      set(X11_XTest_FOUND TRUE)
      set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_XTest_INCLUDE_PATH})
  endif ()

  if (X11_Xi_INCLUDE_PATH AND X11_Xi_LIB)
     set(X11_Xi_FOUND TRUE)
     set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xi_INCLUDE_PATH})
  endif ()

  if (X11_Xinerama_INCLUDE_PATH AND X11_Xinerama_LIB)
     set(X11_Xinerama_FOUND TRUE)
     set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xinerama_INCLUDE_PATH})
  endif ()

  if (X11_Xfixes_INCLUDE_PATH AND X11_Xfixes_LIB)
     set(X11_Xfixes_FOUND TRUE)
     set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xfixes_INCLUDE_PATH})
  endif ()

  if (X11_Xrender_INCLUDE_PATH AND X11_Xrender_LIB)
     set(X11_Xrender_FOUND TRUE)
     set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xrender_INCLUDE_PATH})
  endif ()

  if (X11_XRes_INCLUDE_PATH AND X11_XRes_LIB)
     set(X11_XRes_FOUND TRUE)
     set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_XRes_INCLUDE_PATH})
  endif ()

  if (X11_Xrandr_INCLUDE_PATH AND X11_Xrandr_LIB)
     set(X11_Xrandr_FOUND TRUE)
     set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xrandr_INCLUDE_PATH})
  endif ()

  if (X11_xf86misc_INCLUDE_PATH AND X11_Xxf86misc_LIB)
     set(X11_xf86misc_FOUND TRUE)
     set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_xf86misc_INCLUDE_PATH})
  endif ()

  if (X11_xf86vmode_INCLUDE_PATH AND X11_Xxf86vm_LIB)
     set(X11_xf86vmode_FOUND TRUE)
     set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_xf86vmode_INCLUDE_PATH})
  endif ()

  if (X11_Xcursor_INCLUDE_PATH AND X11_Xcursor_LIB)
     set(X11_Xcursor_FOUND TRUE)
     set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xcursor_INCLUDE_PATH})
  endif ()

  if (X11_Xscreensaver_INCLUDE_PATH AND X11_Xscreensaver_LIB)
     set(X11_Xscreensaver_FOUND TRUE)
     set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xscreensaver_INCLUDE_PATH})
  endif ()

  if (X11_dpms_INCLUDE_PATH)
     set(X11_dpms_FOUND TRUE)
     set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_dpms_INCLUDE_PATH})
  endif ()

  if (X11_Xkb_INCLUDE_PATH AND X11_Xkblib_INCLUDE_PATH AND X11_Xlib_INCLUDE_PATH)
     set(X11_Xkb_FOUND TRUE)
     set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xkb_INCLUDE_PATH} )
  endif ()

  if (X11_Xkbfile_INCLUDE_PATH AND X11_Xkbfile_LIB AND X11_Xlib_INCLUDE_PATH)
     set(X11_Xkbfile_FOUND TRUE)
     set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xkbfile_INCLUDE_PATH} )
  endif ()

  if (X11_Xmu_INCLUDE_PATH AND X11_Xmu_LIB)
     set(X11_Xmu_FOUND TRUE)
     set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xmu_INCLUDE_PATH})
  endif ()

  if (X11_Xinput_INCLUDE_PATH AND X11_Xinput_LIB)
     set(X11_Xinput_FOUND TRUE)
     set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xinput_INCLUDE_PATH})
  endif ()

  if (X11_XSync_INCLUDE_PATH)
     set(X11_XSync_FOUND TRUE)
     set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_XSync_INCLUDE_PATH})
  endif ()

  if(X11_ICE_LIB AND X11_ICE_INCLUDE_PATH)
     set(X11_ICE_FOUND TRUE)
  endif()

  if(X11_SM_LIB AND X11_SM_INCLUDE_PATH)
     set(X11_SM_FOUND TRUE)
  endif()

  # Most of the X11 headers will be in the same directories, avoid
  # creating a huge list of duplicates.
  if (X11_INCLUDE_DIR)
     list(REMOVE_DUPLICATES X11_INCLUDE_DIR)
  endif ()

  # Deprecated variable for backwards compatibility with CMake 1.4
  if (X11_X11_INCLUDE_PATH AND X11_LIBRARIES)
    set(X11_FOUND 1)
  endif ()

  if(X11_FOUND)
    include(${CMAKE_CURRENT_LIST_DIR}/CheckFunctionExists.cmake)
    include(${CMAKE_CURRENT_LIST_DIR}/CheckLibraryExists.cmake)

    # Translated from an autoconf-generated configure script.
    # See libs.m4 in autoconf's m4 directory.
    if($ENV{ISC} MATCHES "^yes$")
      set(X11_X_EXTRA_LIBS -lnsl_s -linet)
    else()
      set(X11_X_EXTRA_LIBS "")

      # See if XOpenDisplay in X11 works by itself.
      CHECK_LIBRARY_EXISTS("${X11_LIBRARIES}" "XOpenDisplay" "${X11_LIBRARY_DIR}" X11_LIB_X11_SOLO)
      if(NOT X11_LIB_X11_SOLO)
        # Find library needed for dnet_ntoa.
        CHECK_LIBRARY_EXISTS("dnet" "dnet_ntoa" "" X11_LIB_DNET_HAS_DNET_NTOA)
        if (X11_LIB_DNET_HAS_DNET_NTOA)
          set (X11_X_EXTRA_LIBS ${X11_X_EXTRA_LIBS} -ldnet)
        else ()
          CHECK_LIBRARY_EXISTS("dnet_stub" "dnet_ntoa" "" X11_LIB_DNET_STUB_HAS_DNET_NTOA)
          if (X11_LIB_DNET_STUB_HAS_DNET_NTOA)
            set (X11_X_EXTRA_LIBS ${X11_X_EXTRA_LIBS} -ldnet_stub)
          endif ()
        endif ()
      endif()

      # Find library needed for gethostbyname.
      CHECK_FUNCTION_EXISTS("gethostbyname" CMAKE_HAVE_GETHOSTBYNAME)
      if(NOT CMAKE_HAVE_GETHOSTBYNAME)
        CHECK_LIBRARY_EXISTS("nsl" "gethostbyname" "" CMAKE_LIB_NSL_HAS_GETHOSTBYNAME)
        if (CMAKE_LIB_NSL_HAS_GETHOSTBYNAME)
          set (X11_X_EXTRA_LIBS ${X11_X_EXTRA_LIBS} -lnsl)
        else ()
          CHECK_LIBRARY_EXISTS("bsd" "gethostbyname" "" CMAKE_LIB_BSD_HAS_GETHOSTBYNAME)
          if (CMAKE_LIB_BSD_HAS_GETHOSTBYNAME)
            set (X11_X_EXTRA_LIBS ${X11_X_EXTRA_LIBS} -lbsd)
          endif ()
        endif ()
      endif()

      # Find library needed for connect.
      CHECK_FUNCTION_EXISTS("connect" CMAKE_HAVE_CONNECT)
      if(NOT CMAKE_HAVE_CONNECT)
        CHECK_LIBRARY_EXISTS("socket" "connect" "" CMAKE_LIB_SOCKET_HAS_CONNECT)
        if (CMAKE_LIB_SOCKET_HAS_CONNECT)
          set (X11_X_EXTRA_LIBS -lsocket ${X11_X_EXTRA_LIBS})
        endif ()
      endif()

      # Find library needed for remove.
      CHECK_FUNCTION_EXISTS("remove" CMAKE_HAVE_REMOVE)
      if(NOT CMAKE_HAVE_REMOVE)
        CHECK_LIBRARY_EXISTS("posix" "remove" "" CMAKE_LIB_POSIX_HAS_REMOVE)
        if (CMAKE_LIB_POSIX_HAS_REMOVE)
          set (X11_X_EXTRA_LIBS ${X11_X_EXTRA_LIBS} -lposix)
        endif ()
      endif()

      # Find library needed for shmat.
      CHECK_FUNCTION_EXISTS("shmat" CMAKE_HAVE_SHMAT)
      if(NOT CMAKE_HAVE_SHMAT)
        CHECK_LIBRARY_EXISTS("ipc" "shmat" "" CMAKE_LIB_IPS_HAS_SHMAT)
        if (CMAKE_LIB_IPS_HAS_SHMAT)
          set (X11_X_EXTRA_LIBS ${X11_X_EXTRA_LIBS} -lipc)
        endif ()
      endif()
    endif()

    if (X11_ICE_FOUND)
      CHECK_LIBRARY_EXISTS("ICE" "IceConnectionNumber" "${X11_LIBRARY_DIR}"
                            CMAKE_LIB_ICE_HAS_ICECONNECTIONNUMBER)
      if(CMAKE_LIB_ICE_HAS_ICECONNECTIONNUMBER)
        set (X11_X_PRE_LIBS ${X11_ICE_LIB})
        if(X11_SM_LIB)
          set (X11_X_PRE_LIBS ${X11_SM_LIB} ${X11_X_PRE_LIBS})
        endif()
      endif()
    endif ()

    # Build the final list of libraries.
    set(X11_LIBRARIES ${X11_X_PRE_LIBS} ${X11_LIBRARIES} ${X11_X_EXTRA_LIBS})

    include(${CMAKE_CURRENT_LIST_DIR}/FindPackageMessage.cmake)
    FIND_PACKAGE_MESSAGE(X11 "Found X11: ${X11_X11_LIB}"
      "[${X11_X11_LIB}][${X11_INCLUDE_DIR}]")
  else ()
    if (X11_FIND_REQUIRED)
      message(FATAL_ERROR "Could not find X11")
    endif ()
  endif ()

  mark_as_advanced(
    X11_X11_INCLUDE_PATH
    X11_X11_LIB
    X11_Xext_LIB
    X11_Xau_LIB
    X11_Xau_INCLUDE_PATH
    X11_Xlib_INCLUDE_PATH
    X11_Xutil_INCLUDE_PATH
    X11_Xcomposite_INCLUDE_PATH
    X11_Xcomposite_LIB
    X11_Xaccess_INCLUDE_PATH
    X11_Xfixes_LIB
    X11_Xfixes_INCLUDE_PATH
    X11_Xrandr_LIB
    X11_Xrandr_INCLUDE_PATH
    X11_Xdamage_LIB
    X11_Xdamage_INCLUDE_PATH
    X11_Xrender_LIB
    X11_Xrender_INCLUDE_PATH
    X11_XRes_LIB
    X11_XRes_INCLUDE_PATH
    X11_Xxf86misc_LIB
    X11_xf86misc_INCLUDE_PATH
    X11_Xxf86vm_LIB
    X11_xf86vmode_INCLUDE_PATH
    X11_Xi_LIB
    X11_Xi_INCLUDE_PATH
    X11_Xinerama_LIB
    X11_Xinerama_INCLUDE_PATH
    X11_XTest_LIB
    X11_XTest_INCLUDE_PATH
    X11_Xcursor_LIB
    X11_Xcursor_INCLUDE_PATH
    X11_dpms_INCLUDE_PATH
    X11_Xt_LIB
    X11_Xt_INCLUDE_PATH
    X11_Xdmcp_LIB
    X11_LIBRARIES
    X11_Xaccessrules_INCLUDE_PATH
    X11_Xaccessstr_INCLUDE_PATH
    X11_Xdmcp_INCLUDE_PATH
    X11_Xkb_INCLUDE_PATH
    X11_Xkblib_INCLUDE_PATH
    X11_Xkbfile_INCLUDE_PATH
    X11_Xkbfile_LIB
    X11_Xmu_INCLUDE_PATH
    X11_Xmu_LIB
    X11_Xscreensaver_INCLUDE_PATH
    X11_Xscreensaver_LIB
    X11_Xpm_INCLUDE_PATH
    X11_Xpm_LIB
    X11_Xinput_LIB
    X11_Xinput_INCLUDE_PATH
    X11_Xft_LIB
    X11_Xft_INCLUDE_PATH
    X11_Xshape_INCLUDE_PATH
    X11_Xv_LIB
    X11_Xv_INCLUDE_PATH
    X11_XShm_INCLUDE_PATH
    X11_ICE_LIB
    X11_ICE_INCLUDE_PATH
    X11_SM_LIB
    X11_SM_INCLUDE_PATH
    X11_XSync_INCLUDE_PATH
  )
  set(CMAKE_FIND_FRAMEWORK ${CMAKE_FIND_FRAMEWORK_SAVE})
  set(CMAKE_REQUIRED_QUIET ${CMAKE_REQUIRED_QUIET_SAVE})
endif ()

# X11_FIND_REQUIRED_<component> could be checked too
