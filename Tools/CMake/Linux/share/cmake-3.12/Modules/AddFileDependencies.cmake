# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# AddFileDependencies
# -------------------
#
# ADD_FILE_DEPENDENCIES(source_file depend_files...)
#
# Adds the given files as dependencies to source_file

macro(ADD_FILE_DEPENDENCIES _file)

   get_source_file_property(_deps ${_file} OBJECT_DEPENDS)
   if (_deps)
      set(_deps ${_deps} ${ARGN})
   else ()
      set(_deps ${ARGN})
   endif ()

   set_source_files_properties(${_file} PROPERTIES OBJECT_DEPENDS "${_deps}")

endmacro()
