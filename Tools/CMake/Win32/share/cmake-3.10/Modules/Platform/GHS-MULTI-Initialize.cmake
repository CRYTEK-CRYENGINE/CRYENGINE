# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


#Setup Greenhills MULTI specific compilation information

if (NOT GHS_INT_DIRECTORY)
  #Assume the C:/ghs/int#### directory that is latest is preferred
  set(GHS_EXPECTED_ROOT "C:/ghs")
  if (EXISTS ${GHS_EXPECTED_ROOT})
    FILE(GLOB GHS_CANDIDATE_INT_DIRS RELATIVE
      ${GHS_EXPECTED_ROOT} ${GHS_EXPECTED_ROOT}/*)
    string(REGEX MATCHALL  "int[0-9][0-9][0-9][0-9a-z]" GHS_CANDIDATE_INT_DIRS
      ${GHS_CANDIDATE_INT_DIRS})
    if (GHS_CANDIDATE_INT_DIRS)
      list(SORT GHS_CANDIDATE_INT_DIRS)
      list(GET GHS_CANDIDATE_INT_DIRS -1 GHS_INT_DIRECTORY)
      string(CONCAT GHS_INT_DIRECTORY ${GHS_EXPECTED_ROOT} "/"
        ${GHS_INT_DIRECTORY})
    endif ()
  endif ()

  #Try to look for known registry values
  if (NOT GHS_INT_DIRECTORY)
    find_path(GHS_INT_DIRECTORY INTEGRITY.ld PATHS
      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\GreenHillsSoftware6433c345;InstallLocation]" #int1122
      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\GreenHillsSoftware289b6625;InstallLocation]" #int1104
      )
  endif ()

  set(GHS_INT_DIRECTORY ${GHS_INT_DIRECTORY} CACHE PATH
    "Path to integrity directory")
endif ()

set(GHS_OS_DIR ${GHS_INT_DIRECTORY} CACHE PATH "OS directory")
set(GHS_PRIMARY_TARGET "arm_integrity.tgt" CACHE STRING "target for compilation")
set(GHS_BSP_NAME "simarm" CACHE STRING "BSP name")
set(GHS_CUSTOMIZATION "" CACHE FILEPATH "optional GHS customization")
mark_as_advanced(GHS_CUSTOMIZATION)
set(GHS_GPJ_MACROS "" CACHE STRING "optional GHS macros generated in the .gpjs for legacy reasons")
mark_as_advanced(GHS_GPJ_MACROS)
