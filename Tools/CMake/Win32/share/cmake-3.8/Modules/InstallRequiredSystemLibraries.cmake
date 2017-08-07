# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# InstallRequiredSystemLibraries
# ------------------------------
#
# Include this module to search for compiler-provided system runtime
# libraries and add install rules for them.  Some optional variables
# may be set prior to including the module to adjust behavior:
#
# ``CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS``
#   Specify additional runtime libraries that may not be detected.
#   After inclusion any detected libraries will be appended to this.
#
# ``CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP``
#   Set to TRUE to skip calling the :command:`install(PROGRAMS)` command to
#   allow the includer to specify its own install rule, using the value of
#   ``CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS`` to get the list of libraries.
#
# ``CMAKE_INSTALL_DEBUG_LIBRARIES``
#   Set to TRUE to install the debug runtime libraries when available
#   with MSVC tools.
#
# ``CMAKE_INSTALL_DEBUG_LIBRARIES_ONLY``
#   Set to TRUE to install only the debug runtime libraries with MSVC
#   tools even if the release runtime libraries are also available.
#
# ``CMAKE_INSTALL_UCRT_LIBRARIES``
#   Set to TRUE to install the Windows Universal CRT libraries for
#   app-local deployment (e.g. to Windows XP).  This is meaningful
#   only with MSVC from Visual Studio 2015 or higher.
#
# ``CMAKE_INSTALL_MFC_LIBRARIES``
#   Set to TRUE to install the MSVC MFC runtime libraries.
#
# ``CMAKE_INSTALL_OPENMP_LIBRARIES``
#   Set to TRUE to install the MSVC OpenMP runtime libraries
#
# ``CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION``
#   Specify the :command:`install(PROGRAMS)` command ``DESTINATION``
#   option.  If not specified, the default is ``bin`` on Windows
#   and ``lib`` elsewhere.
#
# ``CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS``
#   Set to TRUE to disable warnings about required library files that
#   do not exist.  (For example, Visual Studio Express editions may
#   not provide the redistributable files.)
#
# ``CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT``
#   Specify the :command:`install(PROGRAMS)` command ``COMPONENT``
#   option.  If not specified, no such option will be used.

if(MSVC)
  file(TO_CMAKE_PATH "$ENV{SYSTEMROOT}" SYSTEMROOT)

  if(CMAKE_CL_64)
    if(MSVC_VERSION GREATER 1599)
      # VS 10 and later:
      set(CMAKE_MSVC_ARCH x64)
    else()
      # VS 9 and earlier:
      set(CMAKE_MSVC_ARCH amd64)
    endif()
  else()
    set(CMAKE_MSVC_ARCH x86)
  endif()

  get_filename_component(devenv_dir "${CMAKE_MAKE_PROGRAM}" PATH)
  get_filename_component(base_dir "${devenv_dir}/../.." ABSOLUTE)

  if(MSVC_VERSION EQUAL 1300)
    set(__install__libs
      "${SYSTEMROOT}/system32/msvcp70.dll"
      "${SYSTEMROOT}/system32/msvcr70.dll"
      )
  endif()

  if(MSVC_VERSION EQUAL 1310)
    set(__install__libs
      "${SYSTEMROOT}/system32/msvcp71.dll"
      "${SYSTEMROOT}/system32/msvcr71.dll"
      )
  endif()

  if(MSVC_VERSION EQUAL 1400)
    # Find the runtime library redistribution directory.
    get_filename_component(msvc_install_dir
      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\8.0;InstallDir]" ABSOLUTE)
    if(DEFINED MSVC80_REDIST_DIR AND EXISTS "${MSVC80_REDIST_DIR}")
      set(MSVC_REDIST_DIR "${MSVC80_REDIST_DIR}") # use old cache entry
    endif()
    find_path(MSVC_REDIST_DIR NAMES ${CMAKE_MSVC_ARCH}/Microsoft.VC80.CRT/Microsoft.VC80.CRT.manifest
      PATHS
        "${msvc_install_dir}/../../VC/redist"
        "${base_dir}/VC/redist"
      )
    mark_as_advanced(MSVC_REDIST_DIR)
    set(MSVC_CRT_DIR "${MSVC_REDIST_DIR}/${CMAKE_MSVC_ARCH}/Microsoft.VC80.CRT")

    # Install the manifest that allows DLLs to be loaded from the
    # directory containing the executable.
    if(NOT CMAKE_INSTALL_DEBUG_LIBRARIES_ONLY)
      set(__install__libs
        "${MSVC_CRT_DIR}/Microsoft.VC80.CRT.manifest"
        "${MSVC_CRT_DIR}/msvcm80.dll"
        "${MSVC_CRT_DIR}/msvcp80.dll"
        "${MSVC_CRT_DIR}/msvcr80.dll"
        )
    else()
      set(__install__libs)
    endif()

    if(CMAKE_INSTALL_DEBUG_LIBRARIES)
      set(MSVC_CRT_DIR
        "${MSVC_REDIST_DIR}/Debug_NonRedist/${CMAKE_MSVC_ARCH}/Microsoft.VC80.DebugCRT")
      set(__install__libs ${__install__libs}
        "${MSVC_CRT_DIR}/Microsoft.VC80.DebugCRT.manifest"
        "${MSVC_CRT_DIR}/msvcm80d.dll"
        "${MSVC_CRT_DIR}/msvcp80d.dll"
        "${MSVC_CRT_DIR}/msvcr80d.dll"
        )
    endif()
  endif()

  if(MSVC_VERSION EQUAL 1500)
    # Find the runtime library redistribution directory.
    get_filename_component(msvc_install_dir
      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\9.0;InstallDir]" ABSOLUTE)
    get_filename_component(msvc_express_install_dir
      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VCExpress\\9.0;InstallDir]" ABSOLUTE)
    if(DEFINED MSVC90_REDIST_DIR AND EXISTS "${MSVC90_REDIST_DIR}")
      set(MSVC_REDIST_DIR "${MSVC90_REDIST_DIR}") # use old cache entry
    endif()
    find_path(MSVC_REDIST_DIR NAMES ${CMAKE_MSVC_ARCH}/Microsoft.VC90.CRT/Microsoft.VC90.CRT.manifest
      PATHS
        "${msvc_install_dir}/../../VC/redist"
        "${msvc_express_install_dir}/../../VC/redist"
        "${base_dir}/VC/redist"
      )
    mark_as_advanced(MSVC_REDIST_DIR)
    set(MSVC_CRT_DIR "${MSVC_REDIST_DIR}/${CMAKE_MSVC_ARCH}/Microsoft.VC90.CRT")

    # Install the manifest that allows DLLs to be loaded from the
    # directory containing the executable.
    if(NOT CMAKE_INSTALL_DEBUG_LIBRARIES_ONLY)
      set(__install__libs
        "${MSVC_CRT_DIR}/Microsoft.VC90.CRT.manifest"
        "${MSVC_CRT_DIR}/msvcm90.dll"
        "${MSVC_CRT_DIR}/msvcp90.dll"
        "${MSVC_CRT_DIR}/msvcr90.dll"
        )
    else()
      set(__install__libs)
    endif()

    if(CMAKE_INSTALL_DEBUG_LIBRARIES)
      set(MSVC_CRT_DIR
        "${MSVC_REDIST_DIR}/Debug_NonRedist/${CMAKE_MSVC_ARCH}/Microsoft.VC90.DebugCRT")
      set(__install__libs ${__install__libs}
        "${MSVC_CRT_DIR}/Microsoft.VC90.DebugCRT.manifest"
        "${MSVC_CRT_DIR}/msvcm90d.dll"
        "${MSVC_CRT_DIR}/msvcp90d.dll"
        "${MSVC_CRT_DIR}/msvcr90d.dll"
        )
    endif()
  endif()

  if(MSVC_VERSION EQUAL 1910)
    set(_MSVCRT_DLL_VERSION 140)
    set(_MSVCRT_IDE_VERSION 15)
  elseif(MSVC_VERSION EQUAL 1900)
    set(_MSVCRT_DLL_VERSION 140)
    set(_MSVCRT_IDE_VERSION 14)
  elseif(MSVC_VERSION EQUAL 1800)
    set(_MSVCRT_DLL_VERSION 120)
    set(_MSVCRT_IDE_VERSION 12)
  elseif(MSVC_VERSION EQUAL 1700)
    set(_MSVCRT_DLL_VERSION 110)
    set(_MSVCRT_IDE_VERSION 11)
  elseif(MSVC_VERSION EQUAL 1600)
    set(_MSVCRT_DLL_VERSION 100)
    set(_MSVCRT_IDE_VERSION 10)
  else()
    set(_MSVCRT_DLL_VERSION "")
    set(_MSVCRT_IDE_VERSION "")
  endif()

  if(_MSVCRT_DLL_VERSION)
    set(v "${_MSVCRT_DLL_VERSION}")
    set(vs "${_MSVCRT_IDE_VERSION}")

    # Find the runtime library redistribution directory.
    if(vs VERSION_LESS 15 AND DEFINED MSVC${vs}_REDIST_DIR AND EXISTS "${MSVC${vs}_REDIST_DIR}")
      set(MSVC_REDIST_DIR "${MSVC${vs}_REDIST_DIR}") # use old cache entry
    endif()
    if(NOT vs VERSION_LESS 15)
      set(_vs_redist_paths "")
      cmake_host_system_information(RESULT _vs_dir QUERY VS_${vs}_DIR) # undocumented query
      if(IS_DIRECTORY "${_vs_dir}")
        file(GLOB _vs_redist_paths "${_vs_dir}/VC/Redist/MSVC/*")
      endif()
      unset(_vs_dir)
    else()
      get_filename_component(_vs_dir
        "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\${vs}.0;InstallDir]" ABSOLUTE)
      set(programfilesx86 "ProgramFiles(x86)")
      set(_vs_redist_paths
        "${_vs_dir}/../../VC/redist"
        "${base_dir}/VC/redist"
        "$ENV{ProgramFiles}/Microsoft Visual Studio ${vs}.0/VC/redist"
        "$ENV{${programfilesx86}}/Microsoft Visual Studio ${vs}.0/VC/redist"
        )
      unset(_vs_dir)
      unset(programfilesx86)
    endif()
    find_path(MSVC_REDIST_DIR NAMES ${CMAKE_MSVC_ARCH}/Microsoft.VC${vs}0.CRT PATHS ${_vs_redist_paths})
    unset(_vs_redist_paths)
    mark_as_advanced(MSVC_REDIST_DIR)
    set(MSVC_CRT_DIR "${MSVC_REDIST_DIR}/${CMAKE_MSVC_ARCH}/Microsoft.VC${vs}0.CRT")

    if(NOT CMAKE_INSTALL_DEBUG_LIBRARIES_ONLY)
      set(__install__libs
        "${MSVC_CRT_DIR}/msvcp${v}.dll"
        )
      if(NOT vs VERSION_LESS 14)
        list(APPEND __install__libs
            "${MSVC_CRT_DIR}/vcruntime${v}.dll"
            "${MSVC_CRT_DIR}/concrt${v}.dll"
            )
      else()
        list(APPEND __install__libs "${MSVC_CRT_DIR}/msvcr${v}.dll")
      endif()
    else()
      set(__install__libs)
    endif()

    if(CMAKE_INSTALL_DEBUG_LIBRARIES)
      set(MSVC_CRT_DIR
        "${MSVC_REDIST_DIR}/Debug_NonRedist/${CMAKE_MSVC_ARCH}/Microsoft.VC${vs}0.DebugCRT")
      set(__install__libs ${__install__libs}
        "${MSVC_CRT_DIR}/msvcp${v}d.dll"
        )
      if(NOT vs VERSION_LESS 14)
        list(APPEND __install__libs
            "${MSVC_CRT_DIR}/vcruntime${v}d.dll"
            "${MSVC_CRT_DIR}/concrt${v}d.dll"
            )
      else()
        list(APPEND __install__libs "${MSVC_CRT_DIR}/msvcr${v}d.dll")
      endif()
    endif()

    if(CMAKE_INSTALL_UCRT_LIBRARIES AND NOT vs VERSION_LESS 14)
      # Find the Windows Kits directory.
      get_filename_component(windows_kits_dir
        "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]" ABSOLUTE)
      set(programfilesx86 "ProgramFiles(x86)")
      find_path(WINDOWS_KITS_DIR NAMES Redist/ucrt/DLLs/${CMAKE_MSVC_ARCH}/ucrtbase.dll
        PATHS
        "${windows_kits_dir}"
        "$ENV{ProgramFiles}/Windows Kits/10"
        "$ENV{${programfilesx86}}/Windows Kits/10"
        )
      mark_as_advanced(WINDOWS_KITS_DIR)

      # Glob the list of UCRT DLLs.
      if(NOT CMAKE_INSTALL_DEBUG_LIBRARIES_ONLY)
        file(GLOB __ucrt_dlls "${WINDOWS_KITS_DIR}/Redist/ucrt/DLLs/${CMAKE_MSVC_ARCH}/*.dll")
        list(APPEND __install__libs ${__ucrt_dlls})
      endif()
      if(CMAKE_INSTALL_DEBUG_LIBRARIES)
        file(GLOB __ucrt_dlls "${WINDOWS_KITS_DIR}/bin/${CMAKE_MSVC_ARCH}/ucrt/*.dll")
        list(APPEND __install__libs ${__ucrt_dlls})
      endif()
    endif()
  endif()

  if(CMAKE_INSTALL_MFC_LIBRARIES)
    if(MSVC_VERSION EQUAL 1300)
      set(__install__libs ${__install__libs}
        "${SYSTEMROOT}/system32/mfc70.dll"
        )
    endif()

    if(MSVC_VERSION EQUAL 1310)
      set(__install__libs ${__install__libs}
        "${SYSTEMROOT}/system32/mfc71.dll"
        )
    endif()

    if(MSVC_VERSION EQUAL 1400)
      if(CMAKE_INSTALL_DEBUG_LIBRARIES)
        set(MSVC_MFC_DIR
          "${MSVC_REDIST_DIR}/Debug_NonRedist/${CMAKE_MSVC_ARCH}/Microsoft.VC80.DebugMFC")
        set(__install__libs ${__install__libs}
          "${MSVC_MFC_DIR}/Microsoft.VC80.DebugMFC.manifest"
          "${MSVC_MFC_DIR}/mfc80d.dll"
          "${MSVC_MFC_DIR}/mfc80ud.dll"
          "${MSVC_MFC_DIR}/mfcm80d.dll"
          "${MSVC_MFC_DIR}/mfcm80ud.dll"
          )
      endif()

      set(MSVC_MFC_DIR "${MSVC_REDIST_DIR}/${CMAKE_MSVC_ARCH}/Microsoft.VC80.MFC")
      # Install the manifest that allows DLLs to be loaded from the
      # directory containing the executable.
      if(NOT CMAKE_INSTALL_DEBUG_LIBRARIES_ONLY)
        set(__install__libs ${__install__libs}
          "${MSVC_MFC_DIR}/Microsoft.VC80.MFC.manifest"
          "${MSVC_MFC_DIR}/mfc80.dll"
          "${MSVC_MFC_DIR}/mfc80u.dll"
          "${MSVC_MFC_DIR}/mfcm80.dll"
          "${MSVC_MFC_DIR}/mfcm80u.dll"
          )
      endif()

      # include the language dll's for vs8 as well as the actuall dll's
      set(MSVC_MFCLOC_DIR "${MSVC_REDIST_DIR}/${CMAKE_MSVC_ARCH}/Microsoft.VC80.MFCLOC")
      # Install the manifest that allows DLLs to be loaded from the
      # directory containing the executable.
      set(__install__libs ${__install__libs}
        "${MSVC_MFCLOC_DIR}/Microsoft.VC80.MFCLOC.manifest"
        "${MSVC_MFCLOC_DIR}/mfc80chs.dll"
        "${MSVC_MFCLOC_DIR}/mfc80cht.dll"
        "${MSVC_MFCLOC_DIR}/mfc80enu.dll"
        "${MSVC_MFCLOC_DIR}/mfc80esp.dll"
        "${MSVC_MFCLOC_DIR}/mfc80deu.dll"
        "${MSVC_MFCLOC_DIR}/mfc80fra.dll"
        "${MSVC_MFCLOC_DIR}/mfc80ita.dll"
        "${MSVC_MFCLOC_DIR}/mfc80jpn.dll"
        "${MSVC_MFCLOC_DIR}/mfc80kor.dll"
        )
    endif()

    if(MSVC_VERSION EQUAL 1500)
      if(CMAKE_INSTALL_DEBUG_LIBRARIES)
        set(MSVC_MFC_DIR
          "${MSVC_REDIST_DIR}/Debug_NonRedist/${CMAKE_MSVC_ARCH}/Microsoft.VC90.DebugMFC")
        set(__install__libs ${__install__libs}
          "${MSVC_MFC_DIR}/Microsoft.VC90.DebugMFC.manifest"
          "${MSVC_MFC_DIR}/mfc90d.dll"
          "${MSVC_MFC_DIR}/mfc90ud.dll"
          "${MSVC_MFC_DIR}/mfcm90d.dll"
          "${MSVC_MFC_DIR}/mfcm90ud.dll"
          )
      endif()

      set(MSVC_MFC_DIR "${MSVC_REDIST_DIR}/${CMAKE_MSVC_ARCH}/Microsoft.VC90.MFC")
      # Install the manifest that allows DLLs to be loaded from the
      # directory containing the executable.
      if(NOT CMAKE_INSTALL_DEBUG_LIBRARIES_ONLY)
        set(__install__libs ${__install__libs}
          "${MSVC_MFC_DIR}/Microsoft.VC90.MFC.manifest"
          "${MSVC_MFC_DIR}/mfc90.dll"
          "${MSVC_MFC_DIR}/mfc90u.dll"
          "${MSVC_MFC_DIR}/mfcm90.dll"
          "${MSVC_MFC_DIR}/mfcm90u.dll"
          )
      endif()

      # include the language dll's for vs9 as well as the actuall dll's
      set(MSVC_MFCLOC_DIR "${MSVC_REDIST_DIR}/${CMAKE_MSVC_ARCH}/Microsoft.VC90.MFCLOC")
      # Install the manifest that allows DLLs to be loaded from the
      # directory containing the executable.
      set(__install__libs ${__install__libs}
        "${MSVC_MFCLOC_DIR}/Microsoft.VC90.MFCLOC.manifest"
        "${MSVC_MFCLOC_DIR}/mfc90chs.dll"
        "${MSVC_MFCLOC_DIR}/mfc90cht.dll"
        "${MSVC_MFCLOC_DIR}/mfc90enu.dll"
        "${MSVC_MFCLOC_DIR}/mfc90esp.dll"
        "${MSVC_MFCLOC_DIR}/mfc90deu.dll"
        "${MSVC_MFCLOC_DIR}/mfc90fra.dll"
        "${MSVC_MFCLOC_DIR}/mfc90ita.dll"
        "${MSVC_MFCLOC_DIR}/mfc90jpn.dll"
        "${MSVC_MFCLOC_DIR}/mfc90kor.dll"
        )
    endif()

    if(MSVC_VERSION EQUAL 1910)
      set(_MFC_DLL_VERSION 140)
      set(_MFC_IDE_VERSION 15)
    elseif(MSVC_VERSION EQUAL 1900)
      set(_MFC_DLL_VERSION 140)
      set(_MFC_IDE_VERSION 14)
    elseif(MSVC_VERSION EQUAL 1800)
      set(_MFC_DLL_VERSION 120)
      set(_MFC_IDE_VERSION 12)
    elseif(MSVC_VERSION EQUAL 1700)
      set(_MFC_DLL_VERSION 110)
      set(_MFC_IDE_VERSION 11)
    elseif(MSVC_VERSION EQUAL 1600)
      set(_MFC_DLL_VERSION 100)
      set(_MFC_IDE_VERSION 10)
    else()
      set(_MFC_DLL_VERSION "")
      set(_MFC_IDE_VERSION "")
    endif()

    if(_MFC_DLL_VERSION)
      set(v "${_MFC_DLL_VERSION}")
      set(vs "${_MFC_IDE_VERSION}")

      # Starting with VS 15 the MFC DLLs may be in a different directory.
      if (NOT vs VERSION_LESS 15)
        file(GLOB _MSVC_REDIST_DIRS "${MSVC_REDIST_DIR}/../*")
        find_path(MSVC_REDIST_MFC_DIR NAMES ${CMAKE_MSVC_ARCH}/Microsoft.VC${vs}0.MFC
          PATHS ${_MSVC_REDIST_DIRS} NO_DEFAULT_PATH)
        mark_as_advanced(MSVC_REDIST_MFC_DIR)
        unset(_MSVC_REDIST_DIRS)
      else()
        set(MSVC_REDIST_MFC_DIR "${MSVC_REDIST_DIR}")
      endif()

      # Multi-Byte Character Set versions of MFC are available as optional
      # addon since Visual Studio 12.  So for version 12 or higher, check
      # whether they are available and exclude them if they are not.

      if(CMAKE_INSTALL_DEBUG_LIBRARIES)
        set(MSVC_MFC_DIR
          "${MSVC_REDIST_MFC_DIR}/Debug_NonRedist/${CMAKE_MSVC_ARCH}/Microsoft.VC${vs}0.DebugMFC")
        set(__install__libs ${__install__libs}
          "${MSVC_MFC_DIR}/mfc${v}ud.dll"
          "${MSVC_MFC_DIR}/mfcm${v}ud.dll"
          )
        if("${v}" LESS 12 OR EXISTS "${MSVC_MFC_DIR}/mfc${v}d.dll")
          set(__install__libs ${__install__libs}
            "${MSVC_MFC_DIR}/mfc${v}d.dll"
            "${MSVC_MFC_DIR}/mfcm${v}d.dll"
          )
        endif()
      endif()

      set(MSVC_MFC_DIR "${MSVC_REDIST_MFC_DIR}/${CMAKE_MSVC_ARCH}/Microsoft.VC${vs}0.MFC")
      if(NOT CMAKE_INSTALL_DEBUG_LIBRARIES_ONLY)
        set(__install__libs ${__install__libs}
          "${MSVC_MFC_DIR}/mfc${v}u.dll"
          "${MSVC_MFC_DIR}/mfcm${v}u.dll"
          )
        if("${v}" LESS 12 OR EXISTS "${MSVC_MFC_DIR}/mfc${v}.dll")
          set(__install__libs ${__install__libs}
            "${MSVC_MFC_DIR}/mfc${v}.dll"
            "${MSVC_MFC_DIR}/mfcm${v}.dll"
          )
        endif()
      endif()

      # include the language dll's as well as the actuall dll's
      set(MSVC_MFCLOC_DIR "${MSVC_REDIST_MFC_DIR}/${CMAKE_MSVC_ARCH}/Microsoft.VC${vs}0.MFCLOC")
      set(__install__libs ${__install__libs}
        "${MSVC_MFCLOC_DIR}/mfc${v}chs.dll"
        "${MSVC_MFCLOC_DIR}/mfc${v}cht.dll"
        "${MSVC_MFCLOC_DIR}/mfc${v}deu.dll"
        "${MSVC_MFCLOC_DIR}/mfc${v}enu.dll"
        "${MSVC_MFCLOC_DIR}/mfc${v}esn.dll"
        "${MSVC_MFCLOC_DIR}/mfc${v}fra.dll"
        "${MSVC_MFCLOC_DIR}/mfc${v}ita.dll"
        "${MSVC_MFCLOC_DIR}/mfc${v}jpn.dll"
        "${MSVC_MFCLOC_DIR}/mfc${v}kor.dll"
        "${MSVC_MFCLOC_DIR}/mfc${v}rus.dll"
        )
    endif()
  endif()

  # MSVC 8 was the first version with OpenMP
  # Furthermore, there is no debug version of this
  if(CMAKE_INSTALL_OPENMP_LIBRARIES)
    if(MSVC_VERSION EQUAL 1910)
      set(_MSOMP_DLL_VERSION 140)
      set(_MSOMP_IDE_VERSION 15)
    elseif(MSVC_VERSION EQUAL 1900)
      set(_MSOMP_DLL_VERSION 140)
      set(_MSOMP_IDE_VERSION 14)
    elseif(MSVC_VERSION EQUAL 1800)
      set(_MSOMP_DLL_VERSION 120)
      set(_MSOMP_IDE_VERSION 12)
    elseif(MSVC_VERSION EQUAL 1700)
      set(_MSOMP_DLL_VERSION 110)
      set(_MSOMP_IDE_VERSION 11)
    elseif(MSVC_VERSION EQUAL 1600)
      set(_MSOMP_DLL_VERSION 100)
      set(_MSOMP_IDE_VERSION 10)
    elseif(MSVC_VERSION EQUAL 1500)
      set(_MSOMP_DLL_VERSION 90)
      set(_MSOMP_IDE_VERSION 9)
    elseif(MSVC_VERSION EQUAL 1400)
      set(_MSOMP_DLL_VERSION 80)
      set(_MSOMP_IDE_VERSION 8)
    else()
      set(_MSOMP_DLL_VERSION "")
      set(_MSOMP_IDE_VERSION "")
    endif()

    if(_MSOMP_DLL_VERSION)
      set(v "${_MSOMP_DLL_VERSION}")
      set(vs "${_MSOMP_IDE_VERSION}")
      set(MSVC_OPENMP_DIR "${MSVC_REDIST_DIR}/${CMAKE_MSVC_ARCH}/Microsoft.VC${vs}0.OPENMP")

      if(NOT CMAKE_INSTALL_DEBUG_LIBRARIES_ONLY)
        set(__install__libs ${__install__libs}
          "${MSVC_OPENMP_DIR}/vcomp${v}.dll")
      endif()
    endif()
  endif()

  foreach(lib
      ${__install__libs}
      )
    if(EXISTS ${lib})
      set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS
        ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} ${lib})
    else()
      if(NOT CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS)
        message(WARNING "system runtime library file does not exist: '${lib}'")
        # This warning indicates an incomplete Visual Studio installation
        # or a bug somewhere above here in this file.
        # If you would like to avoid this warning, fix the real problem, or
        # set CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS before including
        # this file.
      endif()
    endif()
  endforeach()
endif()

if(WATCOM)
  get_filename_component( CompilerPath ${CMAKE_C_COMPILER} PATH )
  if(CMAKE_C_COMPILER_VERSION)
    set(_compiler_version ${CMAKE_C_COMPILER_VERSION})
  else()
    set(_compiler_version ${CMAKE_CXX_COMPILER_VERSION})
  endif()
  string(REGEX MATCHALL "[0-9]+" _watcom_version_list "${_compiler_version}")
  list(GET _watcom_version_list 0 _watcom_major)
  list(GET _watcom_version_list 1 _watcom_minor)
  set( __install__libs
    ${CompilerPath}/clbr${_watcom_major}${_watcom_minor}.dll
    ${CompilerPath}/mt7r${_watcom_major}${_watcom_minor}.dll
    ${CompilerPath}/plbr${_watcom_major}${_watcom_minor}.dll )
  foreach(lib
      ${__install__libs}
      )
    if(EXISTS ${lib})
      set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS
        ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} ${lib})
    else()
      if(NOT CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS)
        message(WARNING "system runtime library file does not exist: '${lib}'")
        # This warning indicates an incomplete Watcom installation
        # or a bug somewhere above here in this file.
        # If you would like to avoid this warning, fix the real problem, or
        # set CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS before including
        # this file.
      endif()
    endif()
  endforeach()
endif()


# Include system runtime libraries in the installation if any are
# specified by CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS.
if(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
  if(NOT CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP)
    if(NOT CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION)
      if(WIN32)
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION bin)
      else()
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION lib)
      endif()
    endif()
    if(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT)
      set(_CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT
        COMPONENT ${CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT})
    endif()
    install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
      DESTINATION ${CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION}
      ${_CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT}
      )
  endif()
endif()
