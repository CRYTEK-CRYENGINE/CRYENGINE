# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


include(Compiler/GNU)

macro(__compiler_qcc lang)
  __compiler_gnu(${lang})

  # http://www.qnx.com/developers/docs/6.4.0/neutrino/utilities/q/qcc.html#examples
  set(CMAKE_${lang}_COMPILE_OPTIONS_TARGET "-V")

  set(CMAKE_INCLUDE_SYSTEM_FLAG_${lang} "-Wp,-isystem,")
  set(CMAKE_DEPFILE_FLAGS_${lang} "-Wc,-MD,<DEPFILE>,-MT,<OBJECT>,-MF,<DEPFILE>")
endmacro()
