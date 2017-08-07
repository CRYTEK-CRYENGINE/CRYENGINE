# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
ExternalProject
---------------

Create custom targets to build projects in external trees

.. command:: ExternalProject_Add

  The ``ExternalProject_Add`` function creates a custom target to drive
  download, update/patch, configure, build, install and test steps of an
  external project::

   ExternalProject_Add(<name> [<option>...])

  General options are:

  ``DEPENDS <projects>...``
    Targets on which the project depends
  ``PREFIX <dir>``
    Root dir for entire project
  ``LIST_SEPARATOR <sep>``
    Sep to be replaced by ; in cmd lines
  ``TMP_DIR <dir>``
    Directory to store temporary files
  ``STAMP_DIR <dir>``
    Directory to store step timestamps
  ``EXCLUDE_FROM_ALL 1``
    The "all" target does not depend on this

  Download step options are:

  ``DOWNLOAD_NAME <fname>``
    File name to store (if not end of URL)
  ``DOWNLOAD_DIR <dir>``
    Directory to store downloaded files
  ``DOWNLOAD_COMMAND <cmd>...``
    Command to download source tree
  ``DOWNLOAD_NO_PROGRESS 1``
    Disable download progress reports
  ``CVS_REPOSITORY <cvsroot>``
    CVSROOT of CVS repository
  ``CVS_MODULE <mod>``
    Module to checkout from CVS repo
  ``CVS_TAG <tag>``
    Tag to checkout from CVS repo
  ``SVN_REPOSITORY <url>``
    URL of Subversion repo
  ``SVN_REVISION -r<rev>``
    Revision to checkout from Subversion repo
  ``SVN_USERNAME <username>``
    Username for Subversion checkout and update
  ``SVN_PASSWORD <password>``
    Password for Subversion checkout and update
  ``SVN_TRUST_CERT 1``
    Trust the Subversion server site certificate
  ``GIT_REPOSITORY <url>``
    URL of git repo
  ``GIT_TAG <tag>``
    Git branch name, commit id or tag
  ``GIT_REMOTE_NAME <name>``
    The optional name of the remote, default to ``origin``
  ``GIT_SUBMODULES <module>...``
    Git submodules that shall be updated, all if empty
  ``GIT_SHALLOW 1``
    Tell Git to clone with ``--depth 1``.  Use when ``GIT_TAG`` is not
    specified or when it names a branch in order to download only the
    tip of the branch without the rest of its history.
  ``GIT_PROGRESS 1``
    Tell Git to clone with ``--progress``.  For large projects, the clone step
    does not output anything which can make the build appear to have stalled.
    This option forces Git to output progress information during the clone step
    so that forward progress is indicated.
  ``GIT_CONFIG <option>...``
    Tell Git to clone with ``--config <option>``.  Use additional configuration
    parameters when cloning the project (``key=value`` as expected by ``git
    config``).
  ``HG_REPOSITORY <url>``
    URL of mercurial repo
  ``HG_TAG <tag>``
    Mercurial branch name, commit id or tag
  ``URL /.../src.tgz [/.../src.tgz]...``
    Full path or URL(s) of source.  Multiple URLs are allowed as mirrors.
  ``URL_HASH ALGO=value``
    Hash of file at URL
  ``URL_MD5 md5``
    Equivalent to URL_HASH MD5=md5
  ``HTTP_USERNAME <username>``
    Username for download operation
  ``HTTP_PASSWORD <username>``
    Password for download operation
  ``HTTP_HEADER <header>``
    HTTP header for download operation. Suboption can be repeated several times.
  ``TLS_VERIFY <bool>``
    Should certificate for https be checked
  ``TLS_CAINFO <file>``
    Path to a certificate authority file
  ``TIMEOUT <seconds>``
    Time allowed for file download operations
  ``DOWNLOAD_NO_EXTRACT 1``
    Just download the file and do not extract it; the full path to the
    downloaded file is available as ``<DOWNLOADED_FILE>``.

  Update/Patch step options are:

  ``UPDATE_COMMAND <cmd>...``
    Source work-tree update command
  ``UPDATE_DISCONNECTED 1``
    Never update automatically from the remote repository
  ``PATCH_COMMAND <cmd>...``
    Command to patch downloaded source

  Configure step options are:

  ``SOURCE_DIR <dir>``
    Source dir to be used for build
  ``SOURCE_SUBDIR <dir>``
    Path to source CMakeLists.txt relative to ``SOURCE_DIR``
  ``CONFIGURE_COMMAND <cmd>...``
    Build tree configuration command
  ``CMAKE_COMMAND /.../cmake``
    Specify alternative cmake executable
  ``CMAKE_GENERATOR <gen>``
    Specify generator for native build
  ``CMAKE_GENERATOR_PLATFORM <platform>``
    Generator-specific platform name
  ``CMAKE_GENERATOR_TOOLSET <toolset>``
    Generator-specific toolset name
  ``CMAKE_ARGS <arg>...``
    Arguments to CMake command line.
    These arguments are passed to CMake command line, and can contain
    arguments other than cache values, see also
    :manual:`CMake Options <cmake(1)>`. Arguments in the form
    ``-Dvar:string=on`` are always passed to the command line, and
    therefore cannot be changed by the user.
    Arguments may use
    :manual:`generator expressions <cmake-generator-expressions(7)>`.
  ``CMAKE_CACHE_ARGS <arg>...``
    Initial cache arguments, of the form ``-Dvar:string=on``.
    These arguments are written in a pre-load a script that populates
    CMake cache, see also :manual:`cmake -C <cmake(1)>`. This allows one to
    overcome command line length limits.
    These arguments are :command:`set` using the ``FORCE`` argument,
    and therefore cannot be changed by the user.
    Arguments may use
    :manual:`generator expressions <cmake-generator-expressions(7)>`.
  ``CMAKE_CACHE_DEFAULT_ARGS <arg>...``
    Initial default cache arguments, of the form ``-Dvar:string=on``.
    These arguments are written in a pre-load a script that populates
    CMake cache, see also :manual:`cmake -C <cmake(1)>`. This allows one to
    overcome command line length limits.
    These arguments can be used as default value that will be set if no
    previous value is found in the cache, and that the user can change
    later.
    Arguments may use
    :manual:`generator expressions <cmake-generator-expressions(7)>`.

  Build step options are:

  ``BINARY_DIR <dir>``
    Specify build dir location
  ``BUILD_COMMAND <cmd>...``
    Command to drive the native build
  ``BUILD_IN_SOURCE 1``
    Use source dir for build dir
  ``BUILD_ALWAYS 1``
    No stamp file, build step always runs
  ``BUILD_BYPRODUCTS <file>...``
    Files that will be generated by the build command but may or may
    not have their modification time updated by subsequent builds.

  Install step options are:

  ``INSTALL_DIR <dir>``
    Installation prefix to be placed in the ``<INSTALL_DIR>`` placeholder.
    This does not actually configure the external project to install to
    the given prefix.  That must be done by passing appropriate arguments
    to the external project configuration step, e.g. using ``<INSTALL_DIR>``.
  ``INSTALL_COMMAND <cmd>...``
    Command to drive installation of the external project after it has been
    built.  This only happens at the *build* time of the calling project.
    In order to install files from the external project alongside the
    locally-built files, a separate local :command:`install` call must be
    added to pick the files up from one of the external project trees.

  Test step options are:

  ``TEST_BEFORE_INSTALL 1``
    Add test step executed before install step
  ``TEST_AFTER_INSTALL 1``
    Add test step executed after install step
  ``TEST_EXCLUDE_FROM_MAIN 1``
    Main target does not depend on the test step
  ``TEST_COMMAND <cmd>...``
    Command to drive test

  Output logging options are:

  ``LOG_DOWNLOAD 1``
    Wrap download in script to log output
  ``LOG_UPDATE 1``
    Wrap update in script to log output
  ``LOG_CONFIGURE 1``
    Wrap configure in script to log output
  ``LOG_BUILD 1``
    Wrap build in script to log output
  ``LOG_TEST 1``
    Wrap test in script to log output
  ``LOG_INSTALL 1``
    Wrap install in script to log output

  Steps can be given direct access to the terminal if possible.  With
  the :generator:`Ninja` generator, this places the steps in the
  ``console`` :prop_gbl:`pool <JOB_POOLS>`.  Options are:

  ``USES_TERMINAL_DOWNLOAD 1``
    Give download terminal access.
  ``USES_TERMINAL_UPDATE 1``
    Give update terminal access.
  ``USES_TERMINAL_CONFIGURE 1``
    Give configure terminal access.
  ``USES_TERMINAL_BUILD 1``
    Give build terminal access.
  ``USES_TERMINAL_TEST 1``
    Give test terminal access.
  ``USES_TERMINAL_INSTALL 1``
    Give install terminal access.

  Other options are:

  ``STEP_TARGETS <step-target>...``
    Generate custom targets for these steps
  ``INDEPENDENT_STEP_TARGETS <step-target>...``
    Generate custom targets for these steps that do not depend on other
    external projects even if a dependency is set

  The ``*_DIR`` options specify directories for the project, with default
  directories computed as follows.  If the ``PREFIX`` option is given to
  ``ExternalProject_Add()`` or the ``EP_PREFIX`` directory property is set,
  then an external project is built and installed under the specified prefix::

   TMP_DIR      = <prefix>/tmp
   STAMP_DIR    = <prefix>/src/<name>-stamp
   DOWNLOAD_DIR = <prefix>/src
   SOURCE_DIR   = <prefix>/src/<name>
   BINARY_DIR   = <prefix>/src/<name>-build
   INSTALL_DIR  = <prefix>

  Otherwise, if the ``EP_BASE`` directory property is set then components
  of an external project are stored under the specified base::

   TMP_DIR      = <base>/tmp/<name>
   STAMP_DIR    = <base>/Stamp/<name>
   DOWNLOAD_DIR = <base>/Download/<name>
   SOURCE_DIR   = <base>/Source/<name>
   BINARY_DIR   = <base>/Build/<name>
   INSTALL_DIR  = <base>/Install/<name>

  If no ``PREFIX``, ``EP_PREFIX``, or ``EP_BASE`` is specified then the
  default is to set ``PREFIX`` to ``<name>-prefix``.  Relative paths are
  interpreted with respect to the build directory corresponding to the
  source directory in which ``ExternalProject_Add`` is invoked.

  If ``SOURCE_SUBDIR`` is set and no ``CONFIGURE_COMMAND`` is specified, the
  configure command will run CMake using the ``CMakeLists.txt`` located in the
  relative path specified by ``SOURCE_SUBDIR``, relative to the ``SOURCE_DIR``.
  If no ``SOURCE_SUBDIR`` is given, ``SOURCE_DIR`` is used.

  If ``SOURCE_DIR`` is explicitly set to an existing directory the project
  will be built from it.  Otherwise a download step must be specified
  using one of the ``DOWNLOAD_COMMAND``, ``CVS_*``, ``SVN_*``, or ``URL``
  options.  The ``URL`` option may refer locally to a directory or source
  tarball, or refer to a remote tarball (e.g. ``http://.../src.tgz``).

  If ``UPDATE_DISCONNECTED`` is set, the update step is not executed
  automatically when building the main target. The update step can still
  be added as a step target and called manually. This is useful if you
  want to allow one to build the project when you are disconnected from the
  network (you might still need the network for the download step).
  This is disabled by default.
  The directory property ``EP_UPDATE_DISCONNECTED`` can be used to change
  the default value for all the external projects in the current
  directory and its subdirectories.

.. command:: ExternalProject_Add_Step

  The ``ExternalProject_Add_Step`` function adds a custom step to an
  external project::

   ExternalProject_Add_Step(<name> <step> [<option>...])

  Options are:

  ``COMMAND <cmd>...``
    Command line invoked by this step
  ``COMMENT "<text>..."``
    Text printed when step executes
  ``DEPENDEES <step>...``
    Steps on which this step depends
  ``DEPENDERS <step>...``
    Steps that depend on this step
  ``DEPENDS <file>...``
    Files on which this step depends
  ``BYPRODUCTS <file>...``
    Files that will be generated by this step but may or may not
    have their modification time updated by subsequent builds.
  ``ALWAYS 1``
    No stamp file, step always runs
  ``EXCLUDE_FROM_MAIN 1``
    Main target does not depend on this step
  ``WORKING_DIRECTORY <dir>``
    Working directory for command
  ``LOG 1``
    Wrap step in script to log output
  ``USES_TERMINAL 1``
    Give the step direct access to the terminal if possible.

  The command line, comment, working directory, and byproducts of every
  standard and custom step are processed to replace tokens ``<SOURCE_DIR>``,
  ``<SOURCE_SUBDIR>``,  ``<BINARY_DIR>``, ``<INSTALL_DIR>``, and ``<TMP_DIR>``
  with corresponding property values.

Any builtin step that specifies a ``<step>_COMMAND cmd...`` or custom
step that specifies a ``COMMAND cmd...`` may specify additional command
lines using the form ``COMMAND cmd...``.  At build time the commands
will be executed in order and aborted if any one fails.  For example::

 ... BUILD_COMMAND make COMMAND echo done ...

specifies to run ``make`` and then ``echo done`` during the build step.
Whether the current working directory is preserved between commands is
not defined.  Behavior of shell operators like ``&&`` is not defined.

Arguments to ``<step>_COMMAND`` or ``COMMAND`` options may use
:manual:`generator expressions <cmake-generator-expressions(7)>`.

.. command:: ExternalProject_Get_Property

  The ``ExternalProject_Get_Property`` function retrieves external project
  target properties::

    ExternalProject_Get_Property(<name> [prop1 [prop2 [...]]])

  It stores property values in variables of the same name.  Property
  names correspond to the keyword argument names of
  ``ExternalProject_Add``.

.. command:: ExternalProject_Add_StepTargets

  The ``ExternalProject_Add_StepTargets`` function generates custom
  targets for the steps listed::

    ExternalProject_Add_StepTargets(<name> [NO_DEPENDS] [step1 [step2 [...]]])

If ``NO_DEPENDS`` is set, the target will not depend on the
dependencies of the complete project. This is usually safe to use for
the download, update, and patch steps that do not require that all the
dependencies are updated and built.  Using ``NO_DEPENDS`` for other
of the default steps might break parallel builds, so you should avoid,
it.  For custom steps, you should consider whether or not the custom
commands requires that the dependencies are configured, built and
installed.

If ``STEP_TARGETS`` or ``INDEPENDENT_STEP_TARGETS`` is set then
``ExternalProject_Add_StepTargets`` is automatically called at the end
of matching calls to ``ExternalProject_Add_Step``.  Pass
``STEP_TARGETS`` or ``INDEPENDENT_STEP_TARGETS`` explicitly to
individual ``ExternalProject_Add`` calls, or implicitly to all
``ExternalProject_Add`` calls by setting the directory properties
``EP_STEP_TARGETS`` and ``EP_INDEPENDENT_STEP_TARGETS``.  The
``INDEPENDENT`` version of the argument and of the property will call
``ExternalProject_Add_StepTargets`` with the ``NO_DEPENDS`` argument.

If ``STEP_TARGETS`` and ``INDEPENDENT_STEP_TARGETS`` are not set,
clients may still manually call ``ExternalProject_Add_StepTargets``
after calling ``ExternalProject_Add`` or ``ExternalProject_Add_Step``.

This functionality is provided to make it easy to drive the steps
independently of each other by specifying targets on build command
lines.  For example, you may be submitting to a sub-project based
dashboard, where you want to drive the configure portion of the build,
then submit to the dashboard, followed by the build portion, followed
by tests.  If you invoke a custom target that depends on a step
halfway through the step dependency chain, then all the previous steps
will also run to ensure everything is up to date.

For example, to drive configure, build and test steps independently
for each ``ExternalProject_Add`` call in your project, write the following
line prior to any ``ExternalProject_Add`` calls in your ``CMakeLists.txt``
file::

 set_property(DIRECTORY PROPERTY EP_STEP_TARGETS configure build test)

.. command:: ExternalProject_Add_StepDependencies

  The ``ExternalProject_Add_StepDependencies`` function add some
  dependencies for some external project step::

    ExternalProject_Add_StepDependencies(<name> <step> [target1 [target2 [...]]])

  This function takes care to set both target and file level
  dependencies, and will ensure that parallel builds will not break.
  It should be used instead of :command:`add_dependencies()` when adding
  a dependency for some of the step targets generated by
  ``ExternalProject``.
#]=======================================================================]

# Pre-compute a regex to match documented keywords for each command.
math(EXPR _ep_documentation_line_count "${CMAKE_CURRENT_LIST_LINE} - 16")
file(STRINGS "${CMAKE_CURRENT_LIST_FILE}" lines
     LIMIT_COUNT ${_ep_documentation_line_count}
     REGEX "^\\.\\. command:: [A-Za-z0-9_]+|^  ``[A-Z0-9_]+ .*``$")
foreach(line IN LISTS lines)
  if("${line}" MATCHES "^\\.\\. command:: ([A-Za-z0-9_]+)")
    if(_ep_func)
      set(_ep_keywords_${_ep_func} "${_ep_keywords_${_ep_func}})$")
    endif()
    set(_ep_func "${CMAKE_MATCH_1}")
    #message("function [${_ep_func}]")
    set(_ep_keywords_${_ep_func} "^(")
    set(_ep_keyword_sep)
  elseif("${line}" MATCHES "^  ``([A-Z0-9_]+) .*``$")
    set(_ep_key "${CMAKE_MATCH_1}")
    #message("  keyword [${_ep_key}]")
    set(_ep_keywords_${_ep_func}
      "${_ep_keywords_${_ep_func}}${_ep_keyword_sep}${_ep_key}")
    set(_ep_keyword_sep "|")
  endif()
endforeach()
if(_ep_func)
  set(_ep_keywords_${_ep_func} "${_ep_keywords_${_ep_func}})$")
endif()

# Save regex matching supported hash algorithm names.
set(_ep_hash_algos "MD5|SHA1|SHA224|SHA256|SHA384|SHA512|SHA3_224|SHA3_256|SHA3_384|SHA3_512")
set(_ep_hash_regex "^(${_ep_hash_algos})=([0-9A-Fa-f]+)$")

set(_ExternalProject_SELF "${CMAKE_CURRENT_LIST_FILE}")
get_filename_component(_ExternalProject_SELF_DIR "${_ExternalProject_SELF}" PATH)

function(_ep_parse_arguments f name ns args)
  # Transfer the arguments to this function into target properties for the
  # new custom target we just added so that we can set up all the build steps
  # correctly based on target properties.
  #
  # We loop through ARGN and consider the namespace starting with an
  # upper-case letter followed by at least two more upper-case letters,
  # numbers or underscores to be keywords.
  set(key)

  foreach(arg IN LISTS args)
    set(is_value 1)

    if(arg MATCHES "^[A-Z][A-Z0-9_][A-Z0-9_]+$" AND
        NOT (("x${arg}x" STREQUAL "x${key}x") AND ("x${key}x" STREQUAL "xCOMMANDx")) AND
        NOT arg MATCHES "^(TRUE|FALSE)$")
      if(_ep_keywords_${f} AND arg MATCHES "${_ep_keywords_${f}}")
        set(is_value 0)
      endif()
    endif()

    if(is_value)
      if(key)
        # Value
        if(NOT arg STREQUAL "")
          set_property(TARGET ${name} APPEND PROPERTY ${ns}${key} "${arg}")
        else()
          get_property(have_key TARGET ${name} PROPERTY ${ns}${key} SET)
          if(have_key)
            get_property(value TARGET ${name} PROPERTY ${ns}${key})
            set_property(TARGET ${name} PROPERTY ${ns}${key} "${value};${arg}")
          else()
            set_property(TARGET ${name} PROPERTY ${ns}${key} "${arg}")
          endif()
        endif()
      else()
        # Missing Keyword
        message(AUTHOR_WARNING "value '${arg}' with no previous keyword in ${f}")
      endif()
    else()
      set(key "${arg}")
    endif()
  endforeach()
endfunction()


define_property(DIRECTORY PROPERTY "EP_BASE" INHERITED
  BRIEF_DOCS "Base directory for External Project storage."
  FULL_DOCS
  "See documentation of the ExternalProject_Add() function in the "
  "ExternalProject module."
  )

define_property(DIRECTORY PROPERTY "EP_PREFIX" INHERITED
  BRIEF_DOCS "Top prefix for External Project storage."
  FULL_DOCS
  "See documentation of the ExternalProject_Add() function in the "
  "ExternalProject module."
  )

define_property(DIRECTORY PROPERTY "EP_STEP_TARGETS" INHERITED
  BRIEF_DOCS
  "List of ExternalProject steps that automatically get corresponding targets"
  FULL_DOCS
  "These targets will be dependent on the main target dependencies"
  "See documentation of the ExternalProject_Add_StepTargets() function in the "
  "ExternalProject module."
  )

define_property(DIRECTORY PROPERTY "EP_INDEPENDENT_STEP_TARGETS" INHERITED
  BRIEF_DOCS
  "List of ExternalProject steps that automatically get corresponding targets"
  FULL_DOCS
  "These targets will not be dependent on the main target dependencies"
  "See documentation of the ExternalProject_Add_StepTargets() function in the "
  "ExternalProject module."
  )

define_property(DIRECTORY PROPERTY "EP_UPDATE_DISCONNECTED" INHERITED
  BRIEF_DOCS "Never update automatically from the remote repo."
  FULL_DOCS
  "See documentation of the ExternalProject_Add() function in the "
  "ExternalProject module."
  )

function(_ep_write_gitclone_script script_filename source_dir git_EXECUTABLE git_repository git_tag git_remote_name git_submodules git_shallow git_progress git_config src_name work_dir gitclone_infofile gitclone_stampfile tls_verify)
  if(NOT GIT_VERSION_STRING VERSION_LESS 1.7.10)
    set(git_clone_shallow_options "--depth 1 --no-single-branch")
  else()
    set(git_clone_shallow_options "--depth 1")
  endif()
  file(WRITE ${script_filename}
"if(\"${git_tag}\" STREQUAL \"\")
  message(FATAL_ERROR \"Tag for git checkout should not be empty.\")
endif()

set(run 0)

if(\"${gitclone_infofile}\" IS_NEWER_THAN \"${gitclone_stampfile}\")
  set(run 1)
endif()

if(NOT run)
  message(STATUS \"Avoiding repeated git clone, stamp file is up to date: '${gitclone_stampfile}'\")
  return()
endif()

execute_process(
  COMMAND \${CMAKE_COMMAND} -E remove_directory \"${source_dir}\"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR \"Failed to remove directory: '${source_dir}'\")
endif()

set(git_options)

# disable cert checking if explicitly told not to do it
set(tls_verify \"${tls_verify}\")
if(NOT \"x${tls_verify}\" STREQUAL \"x\" AND NOT tls_verify)
  list(APPEND git_options
    -c http.sslVerify=false)
endif()

set(git_clone_options)

set(git_shallow \"${git_shallow}\")
if(git_shallow)
  list(APPEND git_clone_options ${git_clone_shallow_options})
endif()

set(git_progress \"${git_progress}\")
if(git_progress)
  list(APPEND git_clone_options --progress)
endif()

set(git_config \"${git_config}\")
foreach(config IN LISTS git_config)
  list(APPEND git_clone_options --config \${config})
endforeach()

# try the clone 3 times incase there is an odd git clone issue
set(error_code 1)
set(number_of_tries 0)
while(error_code AND number_of_tries LESS 3)
  execute_process(
    COMMAND \"${git_EXECUTABLE}\" \${git_options} clone \${git_clone_options} --origin \"${git_remote_name}\" \"${git_repository}\" \"${src_name}\"
    WORKING_DIRECTORY \"${work_dir}\"
    RESULT_VARIABLE error_code
    )
  math(EXPR number_of_tries \"\${number_of_tries} + 1\")
endwhile()
if(number_of_tries GREATER 1)
  message(STATUS \"Had to git clone more than once:
          \${number_of_tries} times.\")
endif()
if(error_code)
  message(FATAL_ERROR \"Failed to clone repository: '${git_repository}'\")
endif()

# Use `git checkout <branch>` even though this risks ambiguity with a
# local path.  Unfortunately we cannot use `git checkout <tree-ish> --`
# because that will not search for remote branch names, a common use case.
execute_process(
  COMMAND \"${git_EXECUTABLE}\" \${git_options} checkout ${git_tag}
  WORKING_DIRECTORY \"${work_dir}/${src_name}\"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR \"Failed to checkout tag: '${git_tag}'\")
endif()

execute_process(
  COMMAND \"${git_EXECUTABLE}\" \${git_options} submodule init ${git_submodules}
  WORKING_DIRECTORY \"${work_dir}/${src_name}\"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR \"Failed to init submodules in: '${work_dir}/${src_name}'\")
endif()

execute_process(
  COMMAND \"${git_EXECUTABLE}\" \${git_options} submodule update --recursive --init ${git_submodules}
  WORKING_DIRECTORY \"${work_dir}/${src_name}\"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR \"Failed to update submodules in: '${work_dir}/${src_name}'\")
endif()

# Complete success, update the script-last-run stamp file:
#
execute_process(
  COMMAND \${CMAKE_COMMAND} -E copy
    \"${gitclone_infofile}\"
    \"${gitclone_stampfile}\"
  WORKING_DIRECTORY \"${work_dir}/${src_name}\"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR \"Failed to copy script-last-run stamp file: '${gitclone_stampfile}'\")
endif()

"
)

endfunction()

function(_ep_write_hgclone_script script_filename source_dir hg_EXECUTABLE hg_repository hg_tag src_name work_dir hgclone_infofile hgclone_stampfile)
  file(WRITE ${script_filename}
"if(\"${hg_tag}\" STREQUAL \"\")
  message(FATAL_ERROR \"Tag for hg checkout should not be empty.\")
endif()

set(run 0)

if(\"${hgclone_infofile}\" IS_NEWER_THAN \"${hgclone_stampfile}\")
  set(run 1)
endif()

if(NOT run)
  message(STATUS \"Avoiding repeated hg clone, stamp file is up to date: '${hgclone_stampfile}'\")
  return()
endif()

execute_process(
  COMMAND \${CMAKE_COMMAND} -E remove_directory \"${source_dir}\"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR \"Failed to remove directory: '${source_dir}'\")
endif()

execute_process(
  COMMAND \"${hg_EXECUTABLE}\" clone -U \"${hg_repository}\" \"${src_name}\"
  WORKING_DIRECTORY \"${work_dir}\"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR \"Failed to clone repository: '${hg_repository}'\")
endif()

execute_process(
  COMMAND \"${hg_EXECUTABLE}\" update ${hg_tag}
  WORKING_DIRECTORY \"${work_dir}/${src_name}\"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR \"Failed to checkout tag: '${hg_tag}'\")
endif()

# Complete success, update the script-last-run stamp file:
#
execute_process(
  COMMAND \${CMAKE_COMMAND} -E copy
    \"${hgclone_infofile}\"
    \"${hgclone_stampfile}\"
  WORKING_DIRECTORY \"${work_dir}/${src_name}\"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR \"Failed to copy script-last-run stamp file: '${hgclone_stampfile}'\")
endif()

"
)

endfunction()


function(_ep_write_gitupdate_script script_filename git_EXECUTABLE git_tag git_remote_name git_submodules git_repository work_dir)
  if(NOT GIT_VERSION_STRING VERSION_LESS 1.7.6)
    set(git_stash_save_options --all --quiet)
  else()
    set(git_stash_save_options --quiet)
  endif()
  file(WRITE ${script_filename}
"if(\"${git_tag}\" STREQUAL \"\")
  message(FATAL_ERROR \"Tag for git checkout should not be empty.\")
endif()

execute_process(
  COMMAND \"${git_EXECUTABLE}\" rev-list --max-count=1 HEAD
  WORKING_DIRECTORY \"${work_dir}\"
  RESULT_VARIABLE error_code
  OUTPUT_VARIABLE head_sha
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )
if(error_code)
  message(FATAL_ERROR \"Failed to get the hash for HEAD\")
endif()

execute_process(
  COMMAND \"${git_EXECUTABLE}\" show-ref ${git_tag}
  WORKING_DIRECTORY \"${work_dir}\"
  OUTPUT_VARIABLE show_ref_output
  )
# If a remote ref is asked for, which can possibly move around,
# we must always do a fetch and checkout.
if(\"\${show_ref_output}\" MATCHES \"remotes\")
  set(is_remote_ref 1)
else()
  set(is_remote_ref 0)
endif()

# Tag is in the form <remote>/<tag> (i.e. origin/master) we must strip
# the remote from the tag.
if(\"\${show_ref_output}\" MATCHES \"refs/remotes/${git_tag}\")
  string(REGEX MATCH \"^([^/]+)/(.+)$\" _unused \"${git_tag}\")
  set(git_remote \"\${CMAKE_MATCH_1}\")
  set(git_tag \"\${CMAKE_MATCH_2}\")
else()
  set(git_remote \"${git_remote_name}\")
  set(git_tag \"${git_tag}\")
endif()

# This will fail if the tag does not exist (it probably has not been fetched
# yet).
execute_process(
  COMMAND \"${git_EXECUTABLE}\" rev-list --max-count=1 ${git_tag}
  WORKING_DIRECTORY \"${work_dir}\"
  RESULT_VARIABLE error_code
  OUTPUT_VARIABLE tag_sha
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )

# Is the hash checkout out that we want?
if(error_code OR is_remote_ref OR NOT (\"\${tag_sha}\" STREQUAL \"\${head_sha}\"))
  execute_process(
    COMMAND \"${git_EXECUTABLE}\" fetch
    WORKING_DIRECTORY \"${work_dir}\"
    RESULT_VARIABLE error_code
    )
  if(error_code)
    message(FATAL_ERROR \"Failed to fetch repository '${git_repository}'\")
  endif()

  if(is_remote_ref)
    # Check if stash is needed
    execute_process(
      COMMAND \"${git_EXECUTABLE}\" status --porcelain
      WORKING_DIRECTORY \"${work_dir}\"
      RESULT_VARIABLE error_code
      OUTPUT_VARIABLE repo_status
      )
    if(error_code)
      message(FATAL_ERROR \"Failed to get the status\")
    endif()
    string(LENGTH \"\${repo_status}\" need_stash)

    # If not in clean state, stash changes in order to be able to be able to
    # perform git pull --rebase
    if(need_stash)
      execute_process(
        COMMAND \"${git_EXECUTABLE}\" stash save ${git_stash_save_options}
        WORKING_DIRECTORY \"${work_dir}\"
        RESULT_VARIABLE error_code
        )
      if(error_code)
        message(FATAL_ERROR \"Failed to stash changes\")
      endif()
    endif()

    # Pull changes from the remote branch
    execute_process(
      COMMAND \"${git_EXECUTABLE}\" rebase \${git_remote}/\${git_tag}
      WORKING_DIRECTORY \"${work_dir}\"
      RESULT_VARIABLE error_code
      )
    if(error_code)
      # Rebase failed: Restore previous state.
      execute_process(
        COMMAND \"${git_EXECUTABLE}\" rebase --abort
        WORKING_DIRECTORY \"${work_dir}\"
      )
      if(need_stash)
        execute_process(
          COMMAND \"${git_EXECUTABLE}\" stash pop --index --quiet
          WORKING_DIRECTORY \"${work_dir}\"
          )
      endif()
      message(FATAL_ERROR \"\\nFailed to rebase in: '${work_dir}/${src_name}'.\\nYou will have to resolve the conflicts manually\")
    endif()

    if(need_stash)
      execute_process(
        COMMAND \"${git_EXECUTABLE}\" stash pop --index --quiet
        WORKING_DIRECTORY \"${work_dir}\"
        RESULT_VARIABLE error_code
        )
      if(error_code)
        # Stash pop --index failed: Try again dropping the index
        execute_process(
          COMMAND \"${git_EXECUTABLE}\" reset --hard --quiet
          WORKING_DIRECTORY \"${work_dir}\"
          RESULT_VARIABLE error_code
          )
        execute_process(
          COMMAND \"${git_EXECUTABLE}\" stash pop --quiet
          WORKING_DIRECTORY \"${work_dir}\"
          RESULT_VARIABLE error_code
          )
        if(error_code)
          # Stash pop failed: Restore previous state.
          execute_process(
            COMMAND \"${git_EXECUTABLE}\" reset --hard --quiet \${head_sha}
            WORKING_DIRECTORY \"${work_dir}\"
          )
          execute_process(
            COMMAND \"${git_EXECUTABLE}\" stash pop --index --quiet
            WORKING_DIRECTORY \"${work_dir}\"
          )
          message(FATAL_ERROR \"\\nFailed to unstash changes in: '${work_dir}/${src_name}'.\\nYou will have to resolve the conflicts manually\")
        endif()
      endif()
    endif()
  else()
    execute_process(
      COMMAND \"${git_EXECUTABLE}\" checkout ${git_tag}
      WORKING_DIRECTORY \"${work_dir}\"
      RESULT_VARIABLE error_code
      )
    if(error_code)
      message(FATAL_ERROR \"Failed to checkout tag: '${git_tag}'\")
    endif()
  endif()

  execute_process(
    COMMAND \"${git_EXECUTABLE}\" submodule update --recursive --init ${git_submodules}
    WORKING_DIRECTORY \"${work_dir}/${src_name}\"
    RESULT_VARIABLE error_code
    )
  if(error_code)
    message(FATAL_ERROR \"Failed to update submodules in: '${work_dir}/${src_name}'\")
  endif()
endif()

"
)

endfunction(_ep_write_gitupdate_script)

function(_ep_write_downloadfile_script script_filename REMOTE LOCAL timeout no_progress hash tls_verify tls_cainfo userpwd http_headers)
  if(timeout)
    set(TIMEOUT_ARGS TIMEOUT ${timeout})
    set(TIMEOUT_MSG "${timeout} seconds")
  else()
    set(TIMEOUT_ARGS "# no TIMEOUT")
    set(TIMEOUT_MSG "none")
  endif()

  if(no_progress)
    set(SHOW_PROGRESS "")
  else()
    set(SHOW_PROGRESS "SHOW_PROGRESS")
  endif()

  if("${hash}" MATCHES "${_ep_hash_regex}")
    set(ALGO "${CMAKE_MATCH_1}")
    string(TOLOWER "${CMAKE_MATCH_2}" EXPECT_VALUE)
  else()
    set(ALGO "")
    set(EXPECT_VALUE "")
  endif()

  set(TLS_VERIFY_CODE "")
  set(TLS_CAINFO_CODE "")

  # check for curl globals in the project
  if(DEFINED CMAKE_TLS_VERIFY)
    set(TLS_VERIFY_CODE "set(CMAKE_TLS_VERIFY ${CMAKE_TLS_VERIFY})")
  endif()
  if(DEFINED CMAKE_TLS_CAINFO)
    set(TLS_CAINFO_CODE "set(CMAKE_TLS_CAINFO \"${CMAKE_TLS_CAINFO}\")")
  endif()

  # now check for curl locals so that the local values
  # will override the globals

  # check for tls_verify argument
  string(LENGTH "${tls_verify}" tls_verify_len)
  if(tls_verify_len GREATER 0)
    set(TLS_VERIFY_CODE "set(CMAKE_TLS_VERIFY ${tls_verify})")
  endif()
  # check for tls_cainfo argument
  string(LENGTH "${tls_cainfo}" tls_cainfo_len)
  if(tls_cainfo_len GREATER 0)
    set(TLS_CAINFO_CODE "set(CMAKE_TLS_CAINFO \"${tls_cainfo}\")")
  endif()

  if(userpwd STREQUAL ":")
    set(USERPWD_ARGS)
  else()
    set(USERPWD_ARGS USERPWD "${userpwd}")
  endif()

  set(HTTP_HEADERS_ARGS "")
  if(NOT http_headers STREQUAL "")
    foreach(header ${http_headers})
      set(
          HTTP_HEADERS_ARGS
          "HTTPHEADER \"${header}\"\n        ${HTTP_HEADERS_ARGS}"
      )
    endforeach()
  endif()

  # Used variables:
  # * TLS_VERIFY_CODE
  # * TLS_CAINFO_CODE
  # * ALGO
  # * EXPECT_VALUE
  # * REMOTE
  # * LOCAL
  # * SHOW_PROGRESS
  # * TIMEOUT_ARGS
  # * TIMEOUT_MSG
  # * USERPWD_ARGS
  # * HTTP_HEADERS_ARGS
  configure_file(
      "${_ExternalProject_SELF_DIR}/ExternalProject-download.cmake.in"
      "${script_filename}"
      @ONLY
  )
endfunction()

function(_ep_write_verifyfile_script script_filename LOCAL hash)
  if("${hash}" MATCHES "${_ep_hash_regex}")
    set(ALGO "${CMAKE_MATCH_1}")
    string(TOLOWER "${CMAKE_MATCH_2}" EXPECT_VALUE)
  else()
    set(ALGO "")
    set(EXPECT_VALUE "")
  endif()

  # Used variables:
  # * ALGO
  # * EXPECT_VALUE
  # * LOCAL
  configure_file(
      "${_ExternalProject_SELF_DIR}/ExternalProject-verify.cmake.in"
      "${script_filename}"
      @ONLY
  )
endfunction()


function(_ep_write_extractfile_script script_filename name filename directory)
  set(args "")

  if(filename MATCHES "(\\.|=)(7z|tar\\.bz2|tar\\.gz|tar\\.xz|tbz2|tgz|txz|zip)$")
    set(args xfz)
  endif()

  if(filename MATCHES "(\\.|=)tar$")
    set(args xf)
  endif()

  if(args STREQUAL "")
    message(SEND_ERROR "error: do not know how to extract '${filename}' -- known types are .7z, .tar, .tar.bz2, .tar.gz, .tar.xz, .tbz2, .tgz, .txz and .zip")
    return()
  endif()

  file(WRITE ${script_filename}
"# Make file names absolute:
#
get_filename_component(filename \"${filename}\" ABSOLUTE)
get_filename_component(directory \"${directory}\" ABSOLUTE)

message(STATUS \"extracting...
     src='\${filename}'
     dst='\${directory}'\")

if(NOT EXISTS \"\${filename}\")
  message(FATAL_ERROR \"error: file to extract does not exist: '\${filename}'\")
endif()

# Prepare a space for extracting:
#
set(i 1234)
while(EXISTS \"\${directory}/../ex-${name}\${i}\")
  math(EXPR i \"\${i} + 1\")
endwhile()
set(ut_dir \"\${directory}/../ex-${name}\${i}\")
file(MAKE_DIRECTORY \"\${ut_dir}\")

# Extract it:
#
message(STATUS \"extracting... [tar ${args}]\")
execute_process(COMMAND \${CMAKE_COMMAND} -E tar ${args} \${filename}
  WORKING_DIRECTORY \${ut_dir}
  RESULT_VARIABLE rv)

if(NOT rv EQUAL 0)
  message(STATUS \"extracting... [error clean up]\")
  file(REMOVE_RECURSE \"\${ut_dir}\")
  message(FATAL_ERROR \"error: extract of '\${filename}' failed\")
endif()

# Analyze what came out of the tar file:
#
message(STATUS \"extracting... [analysis]\")
file(GLOB contents \"\${ut_dir}/*\")
list(REMOVE_ITEM contents \"\${ut_dir}/.DS_Store\")
list(LENGTH contents n)
if(NOT n EQUAL 1 OR NOT IS_DIRECTORY \"\${contents}\")
  set(contents \"\${ut_dir}\")
endif()

# Move \"the one\" directory to the final directory:
#
message(STATUS \"extracting... [rename]\")
file(REMOVE_RECURSE \${directory})
get_filename_component(contents \${contents} ABSOLUTE)
file(RENAME \${contents} \${directory})

# Clean up:
#
message(STATUS \"extracting... [clean up]\")
file(REMOVE_RECURSE \"\${ut_dir}\")

message(STATUS \"extracting... done\")
"
)

endfunction()


function(_ep_set_directories name)
  get_property(prefix TARGET ${name} PROPERTY _EP_PREFIX)
  if(NOT prefix)
    get_property(prefix DIRECTORY PROPERTY EP_PREFIX)
    if(NOT prefix)
      get_property(base DIRECTORY PROPERTY EP_BASE)
      if(NOT base)
        set(prefix "${name}-prefix")
      endif()
    endif()
  endif()
  if(prefix)
    set(tmp_default "${prefix}/tmp")
    set(download_default "${prefix}/src")
    set(source_default "${prefix}/src/${name}")
    set(binary_default "${prefix}/src/${name}-build")
    set(stamp_default "${prefix}/src/${name}-stamp")
    set(install_default "${prefix}")
  else()
    set(tmp_default "${base}/tmp/${name}")
    set(download_default "${base}/Download/${name}")
    set(source_default "${base}/Source/${name}")
    set(binary_default "${base}/Build/${name}")
    set(stamp_default "${base}/Stamp/${name}")
    set(install_default "${base}/Install/${name}")
  endif()
  get_property(build_in_source TARGET ${name} PROPERTY _EP_BUILD_IN_SOURCE)
  if(build_in_source)
    get_property(have_binary_dir TARGET ${name} PROPERTY _EP_BINARY_DIR SET)
    if(have_binary_dir)
      message(FATAL_ERROR
        "External project ${name} has both BINARY_DIR and BUILD_IN_SOURCE!")
    endif()
  endif()
  set(top "${CMAKE_CURRENT_BINARY_DIR}")
  set(places stamp download source binary install tmp)
  foreach(var ${places})
    string(TOUPPER "${var}" VAR)
    get_property(${var}_dir TARGET ${name} PROPERTY _EP_${VAR}_DIR)
    if(NOT ${var}_dir)
      set(${var}_dir "${${var}_default}")
    endif()
    if(NOT IS_ABSOLUTE "${${var}_dir}")
      get_filename_component(${var}_dir "${top}/${${var}_dir}" ABSOLUTE)
    endif()
    set_property(TARGET ${name} PROPERTY _EP_${VAR}_DIR "${${var}_dir}")
  endforeach()
  get_property(source_subdir TARGET ${name} PROPERTY _EP_SOURCE_SUBDIR)
  if(NOT source_subdir)
    set_property(TARGET ${name} PROPERTY _EP_SOURCE_SUBDIR "")
  elseif(IS_ABSOLUTE "${source_subdir}")
    message(FATAL_ERROR
      "External project ${name} has non-relative SOURCE_SUBDIR!")
  else()
    # Prefix with a slash so that when appended to the source directory, it
    # behaves as expected.
    set_property(TARGET ${name} PROPERTY _EP_SOURCE_SUBDIR "/${source_subdir}")
  endif()
  if(build_in_source)
    get_property(source_dir TARGET ${name} PROPERTY _EP_SOURCE_DIR)
    set_property(TARGET ${name} PROPERTY _EP_BINARY_DIR "${source_dir}")
  endif()

  # Make the directories at CMake configure time *and* add a custom command
  # to make them at build time. They need to exist at makefile generation
  # time for Borland make and wmake so that CMake may generate makefiles
  # with "cd C:\short\paths\with\no\spaces" commands in them.
  #
  # Additionally, the add_custom_command is still used in case somebody
  # removes one of the necessary directories and tries to rebuild without
  # re-running cmake.
  foreach(var ${places})
    string(TOUPPER "${var}" VAR)
    get_property(dir TARGET ${name} PROPERTY _EP_${VAR}_DIR)
    file(MAKE_DIRECTORY "${dir}")
    if(NOT EXISTS "${dir}")
      message(FATAL_ERROR "dir '${dir}' does not exist after file(MAKE_DIRECTORY)")
    endif()
  endforeach()
endfunction()


# IMPORTANT: this MUST be a macro and not a function because of the
# in-place replacements that occur in each ${var}
#
macro(_ep_replace_location_tags target_name)
  set(vars ${ARGN})
  foreach(var ${vars})
    if(${var})
      foreach(dir SOURCE_DIR SOURCE_SUBDIR BINARY_DIR INSTALL_DIR TMP_DIR DOWNLOADED_FILE)
        get_property(val TARGET ${target_name} PROPERTY _EP_${dir})
        string(REPLACE "<${dir}>" "${val}" ${var} "${${var}}")
      endforeach()
    endif()
  endforeach()
endmacro()


function(_ep_command_line_to_initial_cache var args force)
  set(script_initial_cache "")
  set(regex "^([^:]+):([^=]+)=(.*)$")
  set(setArg "")
  set(forceArg "")
  if(force)
    set(forceArg "FORCE")
  endif()
  foreach(line ${args})
    if("${line}" MATCHES "^-D(.*)")
      set(line "${CMAKE_MATCH_1}")
      if(setArg)
        # This is required to build up lists in variables, or complete an entry
        string(APPEND setArg "${accumulator}\" CACHE ${type} \"Initial cache\" ${forceArg})")
        string(APPEND script_initial_cache "\n${setArg}")
        set(accumulator "")
        set(setArg "")
      endif()
      if("${line}" MATCHES "${regex}")
        set(name "${CMAKE_MATCH_1}")
        set(type "${CMAKE_MATCH_2}")
        set(value "${CMAKE_MATCH_3}")
        set(setArg "set(${name} \"${value}")
      else()
        message(WARNING "Line '${line}' does not match regex. Ignoring.")
      endif()
    else()
      # Assume this is a list to append to the last var
      string(APPEND accumulator ";${line}")
    endif()
  endforeach()
  # Catch the final line of the args
  if(setArg)
    string(APPEND setArg "${accumulator}\" CACHE ${type} \"Initial cache\" ${forceArg})")
    string(APPEND script_initial_cache "\n${setArg}")
  endif()
  set(${var} ${script_initial_cache} PARENT_SCOPE)
endfunction()


function(_ep_write_initial_cache target_name script_filename script_initial_cache)
  # Write out values into an initial cache, that will be passed to CMake with -C
  # Replace location tags.
  _ep_replace_location_tags(${target_name} script_initial_cache)
  _ep_replace_location_tags(${target_name} script_filename)
  # Write out the initial cache file to the location specified.
  file(GENERATE OUTPUT "${script_filename}" CONTENT "${script_initial_cache}")
endfunction()


function(ExternalProject_Get_Property name)
  foreach(var ${ARGN})
    string(TOUPPER "${var}" VAR)
    get_property(is_set TARGET ${name} PROPERTY _EP_${VAR} SET)
    if(NOT is_set)
      message(FATAL_ERROR "External project \"${name}\" has no ${var}")
    endif()
    get_property(${var} TARGET ${name} PROPERTY _EP_${VAR})
    set(${var} "${${var}}" PARENT_SCOPE)
  endforeach()
endfunction()


function(_ep_get_configure_command_id name cfg_cmd_id_var)
  get_target_property(cmd ${name} _EP_CONFIGURE_COMMAND)

  if(cmd STREQUAL "")
    # Explicit empty string means no configure step for this project
    set(${cfg_cmd_id_var} "none" PARENT_SCOPE)
  else()
    if(NOT cmd)
      # Default is "use cmake":
      set(${cfg_cmd_id_var} "cmake" PARENT_SCOPE)
    else()
      # Otherwise we have to analyze the value:
      if(cmd MATCHES "^[^;]*/configure")
        set(${cfg_cmd_id_var} "configure" PARENT_SCOPE)
      elseif(cmd MATCHES "^[^;]*/cmake" AND NOT cmd MATCHES ";-[PE];")
        set(${cfg_cmd_id_var} "cmake" PARENT_SCOPE)
      elseif(cmd MATCHES "config")
        set(${cfg_cmd_id_var} "configure" PARENT_SCOPE)
      else()
        set(${cfg_cmd_id_var} "unknown:${cmd}" PARENT_SCOPE)
      endif()
    endif()
  endif()
endfunction()


function(_ep_get_build_command name step cmd_var)
  set(cmd "")
  set(args)
  _ep_get_configure_command_id(${name} cfg_cmd_id)
  if(cfg_cmd_id STREQUAL "cmake")
    # CMake project.  Select build command based on generator.
    get_target_property(cmake_generator ${name} _EP_CMAKE_GENERATOR)
    if("${CMAKE_GENERATOR}" MATCHES "Make" AND
       ("${cmake_generator}" MATCHES "Make" OR NOT cmake_generator))
      # The project uses the same Makefile generator.  Use recursive make.
      set(cmd "$(MAKE)")
      if(step STREQUAL "INSTALL")
        set(args install)
      endif()
      if("x${step}x" STREQUAL "xTESTx")
        set(args test)
      endif()
    else()
      # Drive the project with "cmake --build".
      get_target_property(cmake_command ${name} _EP_CMAKE_COMMAND)
      if(cmake_command)
        set(cmd "${cmake_command}")
      else()
        set(cmd "${CMAKE_COMMAND}")
      endif()
      set(args --build ".")
      if(CMAKE_CONFIGURATION_TYPES)
        if (CMAKE_CFG_INTDIR AND
            NOT CMAKE_CFG_INTDIR STREQUAL "." AND
            NOT CMAKE_CFG_INTDIR MATCHES "\\$")
          # CMake 3.4 and below used the CMAKE_CFG_INTDIR placeholder value
          # provided by multi-configuration generators.  Some projects were
          # taking advantage of that undocumented implementation detail to
          # specify a specific configuration here.  They should use
          # BUILD_COMMAND to change the default command instead, but for
          # compatibility honor the value.
          set(config ${CMAKE_CFG_INTDIR})
          message(AUTHOR_WARNING "CMAKE_CFG_INTDIR should not be set by project code.\n"
            "To get a non-default build command, use the BUILD_COMMAND option.")
        else()
          set(config $<CONFIG>)
        endif()
        list(APPEND args --config ${config})
      endif()
      if(step STREQUAL "INSTALL")
        list(APPEND args --target install)
      endif()
      # But for "TEST" drive the project with corresponding "ctest".
      if("x${step}x" STREQUAL "xTESTx")
        string(REGEX REPLACE "^(.*/)cmake([^/]*)$" "\\1ctest\\2" cmd "${cmd}")
        set(args "")
        if(CMAKE_CONFIGURATION_TYPES)
          list(APPEND args -C ${config})
        endif()
      endif()
    endif()
  else()
    # Non-CMake project.  Guess "make" and "make install" and "make test".
    if("${CMAKE_GENERATOR}" MATCHES "Makefiles")
      # Try to get the parallel arguments
      set(cmd "$(MAKE)")
    else()
      set(cmd "make")
    endif()
    if(step STREQUAL "INSTALL")
      set(args install)
    endif()
    if("x${step}x" STREQUAL "xTESTx")
      set(args test)
    endif()
  endif()

  # Use user-specified arguments instead of default arguments, if any.
  get_property(have_args TARGET ${name} PROPERTY _EP_${step}_ARGS SET)
  if(have_args)
    get_target_property(args ${name} _EP_${step}_ARGS)
  endif()

  list(APPEND cmd ${args})
  set(${cmd_var} "${cmd}" PARENT_SCOPE)
endfunction()

function(_ep_write_log_script name step cmd_var)
  ExternalProject_Get_Property(${name} stamp_dir)
  set(command "${${cmd_var}}")

  set(make "")
  set(code_cygpath_make "")
  if(command MATCHES "^\\$\\(MAKE\\)")
    # GNU make recognizes the string "$(MAKE)" as recursive make, so
    # ensure that it appears directly in the makefile.
    string(REGEX REPLACE "^\\$\\(MAKE\\)" "\${make}" command "${command}")
    set(make "-Dmake=$(MAKE)")

    if(WIN32 AND NOT CYGWIN)
      set(code_cygpath_make "
if(\${make} MATCHES \"^/\")
  execute_process(
    COMMAND cygpath -w \${make}
    OUTPUT_VARIABLE cygpath_make
    ERROR_VARIABLE cygpath_make
    RESULT_VARIABLE cygpath_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(NOT cygpath_error)
    set(make \${cygpath_make})
  endif()
endif()
")
    endif()
  endif()

  set(config "")
  if("${CMAKE_CFG_INTDIR}" MATCHES "^\\$")
    string(REPLACE "${CMAKE_CFG_INTDIR}" "\${config}" command "${command}")
    set(config "-Dconfig=${CMAKE_CFG_INTDIR}")
  endif()

  # Wrap multiple 'COMMAND' lines up into a second-level wrapper
  # script so all output can be sent to one log file.
  if(command MATCHES "(^|;)COMMAND;")
    set(code_execute_process "
${code_cygpath_make}
execute_process(COMMAND \${command} RESULT_VARIABLE result)
if(result)
  set(msg \"Command failed (\${result}):\\n\")
  foreach(arg IN LISTS command)
    set(msg \"\${msg} '\${arg}'\")
  endforeach()
  message(FATAL_ERROR \"\${msg}\")
endif()
")
    set(code "")
    set(cmd "")
    set(sep "")
    foreach(arg IN LISTS command)
      if("x${arg}" STREQUAL "xCOMMAND")
        if(NOT "x${cmd}" STREQUAL "x")
          string(APPEND code "set(command \"${cmd}\")${code_execute_process}")
        endif()
        set(cmd "")
        set(sep "")
      else()
        string(APPEND cmd "${sep}${arg}")
        set(sep ";")
      endif()
    endforeach()
    string(APPEND code "set(command \"${cmd}\")${code_execute_process}")
    file(GENERATE OUTPUT "${stamp_dir}/${name}-${step}-$<CONFIG>-impl.cmake" CONTENT "${code}")
    set(command ${CMAKE_COMMAND} "-Dmake=\${make}" "-Dconfig=\${config}" -P ${stamp_dir}/${name}-${step}-$<CONFIG>-impl.cmake)
  endif()

  # Wrap the command in a script to log output to files.
  set(script ${stamp_dir}/${name}-${step}-$<CONFIG>.cmake)
  set(logbase ${stamp_dir}/${name}-${step})
  set(code "
${code_cygpath_make}
set(command \"${command}\")
execute_process(
  COMMAND \${command}
  RESULT_VARIABLE result
  OUTPUT_FILE \"${logbase}-out.log\"
  ERROR_FILE \"${logbase}-err.log\"
  )
if(result)
  set(msg \"Command failed: \${result}\\n\")
  foreach(arg IN LISTS command)
    set(msg \"\${msg} '\${arg}'\")
  endforeach()
  set(msg \"\${msg}\\nSee also\\n  ${logbase}-*.log\")
  message(FATAL_ERROR \"\${msg}\")
else()
  set(msg \"${name} ${step} command succeeded.  See also ${logbase}-*.log\")
  message(STATUS \"\${msg}\")
endif()
")
  file(GENERATE OUTPUT "${script}" CONTENT "${code}")
  set(command ${CMAKE_COMMAND} ${make} ${config} -P ${script})
  set(${cmd_var} "${command}" PARENT_SCOPE)
endfunction()

# This module used to use "/${CMAKE_CFG_INTDIR}" directly and produced
# makefiles with "/./" in paths for custom command dependencies. Which
# resulted in problems with parallel make -j invocations.
#
# This function was added so that the suffix (search below for ${cfgdir}) is
# only set to "/${CMAKE_CFG_INTDIR}" when ${CMAKE_CFG_INTDIR} is not going to
# be "." (multi-configuration build systems like Visual Studio and Xcode...)
#
function(_ep_get_configuration_subdir_suffix suffix_var)
  set(suffix "")
  if(CMAKE_CONFIGURATION_TYPES)
    set(suffix "/${CMAKE_CFG_INTDIR}")
  endif()
  set(${suffix_var} "${suffix}" PARENT_SCOPE)
endfunction()


function(_ep_get_step_stampfile name step stampfile_var)
  ExternalProject_Get_Property(${name} stamp_dir)

  _ep_get_configuration_subdir_suffix(cfgdir)
  set(stampfile "${stamp_dir}${cfgdir}/${name}-${step}")

  set(${stampfile_var} "${stampfile}" PARENT_SCOPE)
endfunction()


function(ExternalProject_Add_StepTargets name)
  set(steps ${ARGN})
  if(ARGC GREATER 1 AND "${ARGV1}" STREQUAL "NO_DEPENDS")
    set(no_deps 1)
    list(REMOVE_AT steps 0)
  endif()
  foreach(step ${steps})
    if(no_deps  AND  "${step}" MATCHES "^(configure|build|install|test)$")
      message(AUTHOR_WARNING "Using NO_DEPENDS for \"${step}\" step  might break parallel builds")
    endif()
    _ep_get_step_stampfile(${name} ${step} stamp_file)
    add_custom_target(${name}-${step}
      DEPENDS ${stamp_file})
    set_property(TARGET ${name}-${step} PROPERTY _EP_IS_EXTERNAL_PROJECT_STEP 1)
    set_property(TARGET ${name}-${step} PROPERTY LABELS ${name})
    set_property(TARGET ${name}-${step} PROPERTY FOLDER "ExternalProjectTargets/${name}")

    # Depend on other external projects (target-level).
    if(NOT no_deps)
      get_property(deps TARGET ${name} PROPERTY _EP_DEPENDS)
      foreach(arg IN LISTS deps)
        add_dependencies(${name}-${step} ${arg})
      endforeach()
    endif()
  endforeach()
endfunction()


function(ExternalProject_Add_Step name step)
  set(cmf_dir ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles)
  _ep_get_configuration_subdir_suffix(cfgdir)

  set(complete_stamp_file "${cmf_dir}${cfgdir}/${name}-complete")
  _ep_get_step_stampfile(${name} ${step} stamp_file)

  _ep_parse_arguments(ExternalProject_Add_Step
                      ${name} _EP_${step}_ "${ARGN}")

  get_property(exclude_from_main TARGET ${name} PROPERTY _EP_${step}_EXCLUDE_FROM_MAIN)
  if(NOT exclude_from_main)
    add_custom_command(APPEND
      OUTPUT ${complete_stamp_file}
      DEPENDS ${stamp_file}
      )
  endif()

  # Steps depending on this step.
  get_property(dependers TARGET ${name} PROPERTY _EP_${step}_DEPENDERS)
  foreach(depender IN LISTS dependers)
    _ep_get_step_stampfile(${name} ${depender} depender_stamp_file)
    add_custom_command(APPEND
      OUTPUT ${depender_stamp_file}
      DEPENDS ${stamp_file}
      )
  endforeach()

  # Dependencies on files.
  get_property(depends TARGET ${name} PROPERTY _EP_${step}_DEPENDS)

  # Byproducts of the step.
  get_property(byproducts TARGET ${name} PROPERTY _EP_${step}_BYPRODUCTS)

  # Dependencies on steps.
  get_property(dependees TARGET ${name} PROPERTY _EP_${step}_DEPENDEES)
  foreach(dependee IN LISTS dependees)
    _ep_get_step_stampfile(${name} ${dependee} dependee_stamp_file)
    list(APPEND depends ${dependee_stamp_file})
  endforeach()

  # The command to run.
  get_property(command TARGET ${name} PROPERTY _EP_${step}_COMMAND)
  if(command)
    set(comment "Performing ${step} step for '${name}'")
  else()
    set(comment "No ${step} step for '${name}'")
  endif()
  get_property(work_dir TARGET ${name} PROPERTY _EP_${step}_WORKING_DIRECTORY)

  # Replace list separators.
  get_property(sep TARGET ${name} PROPERTY _EP_LIST_SEPARATOR)
  if(sep AND command)
    string(REPLACE "${sep}" "\\;" command "${command}")
  endif()

  # Replace location tags.
  _ep_replace_location_tags(${name} comment command work_dir byproducts)

  # Custom comment?
  get_property(comment_set TARGET ${name} PROPERTY _EP_${step}_COMMENT SET)
  if(comment_set)
    get_property(comment TARGET ${name} PROPERTY _EP_${step}_COMMENT)
  endif()

  # Uses terminal?
  get_property(uses_terminal TARGET ${name} PROPERTY _EP_${step}_USES_TERMINAL)
  if(uses_terminal)
    set(uses_terminal USES_TERMINAL)
  else()
    set(uses_terminal "")
  endif()

  # Run every time?
  get_property(always TARGET ${name} PROPERTY _EP_${step}_ALWAYS)
  if(always)
    set_property(SOURCE ${stamp_file} PROPERTY SYMBOLIC 1)
    set(touch)
    # Remove any existing stamp in case the option changed in an existing tree.
    if(CMAKE_CONFIGURATION_TYPES)
      foreach(cfg ${CMAKE_CONFIGURATION_TYPES})
        string(REPLACE "/${CMAKE_CFG_INTDIR}" "/${cfg}" stamp_file_config "${stamp_file}")
        file(REMOVE ${stamp_file_config})
      endforeach()
    else()
      file(REMOVE ${stamp_file})
    endif()
  else()
    set(touch ${CMAKE_COMMAND} -E touch ${stamp_file})
  endif()

  # Wrap with log script?
  get_property(log TARGET ${name} PROPERTY _EP_${step}_LOG)
  if(command AND log)
    _ep_write_log_script(${name} ${step} command)
  endif()

  if("${command}" STREQUAL "")
    # Some generators (i.e. Xcode) will not generate a file level target
    # if no command is set, and therefore the dependencies on this
    # target will be broken.
    # The empty command is replaced by an echo command here in order to
    # avoid this issue.
    set(command ${CMAKE_COMMAND} -E echo_append)
  endif()

  add_custom_command(
    OUTPUT ${stamp_file}
    BYPRODUCTS ${byproducts}
    COMMENT ${comment}
    COMMAND ${command}
    COMMAND ${touch}
    DEPENDS ${depends}
    WORKING_DIRECTORY ${work_dir}
    VERBATIM
    ${uses_terminal}
    )
  set_property(TARGET ${name} APPEND PROPERTY _EP_STEPS ${step})

  # Add custom "step target"?
  get_property(step_targets TARGET ${name} PROPERTY _EP_STEP_TARGETS)
  if(NOT step_targets)
    get_property(step_targets DIRECTORY PROPERTY EP_STEP_TARGETS)
  endif()
  foreach(st ${step_targets})
    if("${st}" STREQUAL "${step}")
      ExternalProject_Add_StepTargets(${name} ${step})
      break()
    endif()
  endforeach()

  get_property(independent_step_targets TARGET ${name} PROPERTY _EP_INDEPENDENT_STEP_TARGETS)
  if(NOT independent_step_targets)
    get_property(independent_step_targets DIRECTORY PROPERTY EP_INDEPENDENT_STEP_TARGETS)
  endif()
  foreach(st ${independent_step_targets})
    if("${st}" STREQUAL "${step}")
      ExternalProject_Add_StepTargets(${name} NO_DEPENDS ${step})
      break()
    endif()
  endforeach()
endfunction()


function(ExternalProject_Add_StepDependencies name step)
  set(dependencies ${ARGN})

  # Sanity checks on "name" and "step".
  if(NOT TARGET ${name})
    message(FATAL_ERROR "Cannot find target \"${name}\". Perhaps it has not yet been created using ExternalProject_Add.")
  endif()

  get_property(type TARGET ${name} PROPERTY TYPE)
  if(NOT type STREQUAL "UTILITY")
    message(FATAL_ERROR "Target \"${name}\" was not generated by ExternalProject_Add.")
  endif()

  get_property(is_ep TARGET ${name} PROPERTY _EP_IS_EXTERNAL_PROJECT)
  if(NOT is_ep)
    message(FATAL_ERROR "Target \"${name}\" was not generated by ExternalProject_Add.")
  endif()

  get_property(steps TARGET ${name} PROPERTY _EP_STEPS)
  list(FIND steps ${step} is_step)
  if(NOT is_step)
    message(FATAL_ERROR "External project \"${name}\" does not have a step \"${step}\".")
  endif()

  if(TARGET ${name}-${step})
    get_property(type TARGET ${name}-${step} PROPERTY TYPE)
    if(NOT type STREQUAL "UTILITY")
      message(FATAL_ERROR "Target \"${name}-${step}\" was not generated by ExternalProject_Add_StepTargets.")
    endif()
    get_property(is_ep_step TARGET ${name}-${step} PROPERTY _EP_IS_EXTERNAL_PROJECT_STEP)
    if(NOT is_ep_step)
      message(FATAL_ERROR "Target \"${name}-${step}\" was not generated by ExternalProject_Add_StepTargets.")
    endif()
  endif()

  # Always add file-level dependency, but add target-level dependency
  # only if the target exists for that step.
  _ep_get_step_stampfile(${name} ${step} stamp_file)
  foreach(dep ${dependencies})
    add_custom_command(APPEND
      OUTPUT ${stamp_file}
      DEPENDS ${dep})
    if(TARGET ${name}-${step})
      foreach(dep ${dependencies})
        add_dependencies(${name}-${step} ${dep})
      endforeach()
    endif()
  endforeach()

endfunction()


function(_ep_add_mkdir_command name)
  ExternalProject_Get_Property(${name}
    source_dir binary_dir install_dir stamp_dir download_dir tmp_dir)

  _ep_get_configuration_subdir_suffix(cfgdir)

  ExternalProject_Add_Step(${name} mkdir
    COMMENT "Creating directories for '${name}'"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${source_dir}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${binary_dir}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${install_dir}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${tmp_dir}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${stamp_dir}${cfgdir}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${download_dir}
    )
endfunction()


function(_ep_is_dir_empty dir empty_var)
  file(GLOB gr "${dir}/*")
  if("${gr}" STREQUAL "")
    set(${empty_var} 1 PARENT_SCOPE)
  else()
    set(${empty_var} 0 PARENT_SCOPE)
  endif()
endfunction()


function(_ep_add_download_command name)
  ExternalProject_Get_Property(${name} source_dir stamp_dir download_dir tmp_dir)

  get_property(cmd_set TARGET ${name} PROPERTY _EP_DOWNLOAD_COMMAND SET)
  get_property(cmd TARGET ${name} PROPERTY _EP_DOWNLOAD_COMMAND)
  get_property(cvs_repository TARGET ${name} PROPERTY _EP_CVS_REPOSITORY)
  get_property(svn_repository TARGET ${name} PROPERTY _EP_SVN_REPOSITORY)
  get_property(git_repository TARGET ${name} PROPERTY _EP_GIT_REPOSITORY)
  get_property(hg_repository  TARGET ${name} PROPERTY _EP_HG_REPOSITORY )
  get_property(url TARGET ${name} PROPERTY _EP_URL)
  get_property(fname TARGET ${name} PROPERTY _EP_DOWNLOAD_NAME)

  # TODO: Perhaps file:// should be copied to download dir before extraction.
  string(REGEX REPLACE "file://" "" url "${url}")

  set(depends)
  set(comment)
  set(work_dir)

  if(cmd_set)
    set(work_dir ${download_dir})
  elseif(cvs_repository)
    find_package(CVS QUIET)
    if(NOT CVS_EXECUTABLE)
      message(FATAL_ERROR "error: could not find cvs for checkout of ${name}")
    endif()

    get_target_property(cvs_module ${name} _EP_CVS_MODULE)
    if(NOT cvs_module)
      message(FATAL_ERROR "error: no CVS_MODULE")
    endif()

    get_property(cvs_tag TARGET ${name} PROPERTY _EP_CVS_TAG)

    set(repository ${cvs_repository})
    set(module ${cvs_module})
    set(tag ${cvs_tag})
    configure_file(
      "${CMAKE_ROOT}/Modules/RepositoryInfo.txt.in"
      "${stamp_dir}/${name}-cvsinfo.txt"
      @ONLY
      )

    get_filename_component(src_name "${source_dir}" NAME)
    get_filename_component(work_dir "${source_dir}" PATH)
    set(comment "Performing download step (CVS checkout) for '${name}'")
    set(cmd ${CVS_EXECUTABLE} -d ${cvs_repository} -q co ${cvs_tag} -d ${src_name} ${cvs_module})
    list(APPEND depends ${stamp_dir}/${name}-cvsinfo.txt)
  elseif(svn_repository)
    find_package(Subversion QUIET)
    if(NOT Subversion_SVN_EXECUTABLE)
      message(FATAL_ERROR "error: could not find svn for checkout of ${name}")
    endif()

    get_property(svn_revision TARGET ${name} PROPERTY _EP_SVN_REVISION)
    get_property(svn_username TARGET ${name} PROPERTY _EP_SVN_USERNAME)
    get_property(svn_password TARGET ${name} PROPERTY _EP_SVN_PASSWORD)
    get_property(svn_trust_cert TARGET ${name} PROPERTY _EP_SVN_TRUST_CERT)

    set(repository "${svn_repository} user=${svn_username} password=${svn_password}")
    set(module)
    set(tag ${svn_revision})
    configure_file(
      "${CMAKE_ROOT}/Modules/RepositoryInfo.txt.in"
      "${stamp_dir}/${name}-svninfo.txt"
      @ONLY
      )

    get_filename_component(src_name "${source_dir}" NAME)
    get_filename_component(work_dir "${source_dir}" PATH)
    set(comment "Performing download step (SVN checkout) for '${name}'")
    set(svn_user_pw_args "")
    if(DEFINED svn_username)
      set(svn_user_pw_args ${svn_user_pw_args} "--username=${svn_username}")
    endif()
    if(DEFINED svn_password)
      set(svn_user_pw_args ${svn_user_pw_args} "--password=${svn_password}")
    endif()
    if(svn_trust_cert)
      set(svn_trust_cert_args --trust-server-cert)
    endif()
    set(cmd ${Subversion_SVN_EXECUTABLE} co ${svn_repository} ${svn_revision}
      --non-interactive ${svn_trust_cert_args} ${svn_user_pw_args} ${src_name})
    list(APPEND depends ${stamp_dir}/${name}-svninfo.txt)
  elseif(git_repository)
    unset(CMAKE_MODULE_PATH) # Use CMake builtin find module
    find_package(Git QUIET)
    if(NOT GIT_EXECUTABLE)
      message(FATAL_ERROR "error: could not find git for clone of ${name}")
    endif()

    # The git submodule update '--recursive' flag requires git >= v1.6.5
    #
    if(GIT_VERSION_STRING VERSION_LESS 1.6.5)
      message(FATAL_ERROR "error: git version 1.6.5 or later required for 'git submodule update --recursive': GIT_VERSION_STRING='${GIT_VERSION_STRING}'")
    endif()

    get_property(git_tag TARGET ${name} PROPERTY _EP_GIT_TAG)
    if(NOT git_tag)
      set(git_tag "master")
    endif()
    get_property(git_submodules TARGET ${name} PROPERTY _EP_GIT_SUBMODULES)

    get_property(git_remote_name TARGET ${name} PROPERTY _EP_GIT_REMOTE_NAME)
    if(NOT git_remote_name)
      set(git_remote_name "origin")
    endif()

    get_property(tls_verify TARGET ${name} PROPERTY _EP_TLS_VERIFY)
    if("x${tls_verify}" STREQUAL "x" AND DEFINED CMAKE_TLS_VERIFY)
      set(tls_verify "${CMAKE_TLS_VERIFY}")
    endif()
    get_property(git_shallow TARGET ${name} PROPERTY _EP_GIT_SHALLOW)
    get_property(git_progress TARGET ${name} PROPERTY _EP_GIT_PROGRESS)
    get_property(git_config TARGET ${name} PROPERTY _EP_GIT_CONFIG)

    # For the download step, and the git clone operation, only the repository
    # should be recorded in a configured RepositoryInfo file. If the repo
    # changes, the clone script should be run again. But if only the tag
    # changes, avoid running the clone script again. Let the 'always' running
    # update step checkout the new tag.
    #
    set(repository ${git_repository})
    set(module)
    set(tag)
    configure_file(
      "${CMAKE_ROOT}/Modules/RepositoryInfo.txt.in"
      "${stamp_dir}/${name}-gitinfo.txt"
      @ONLY
      )

    get_filename_component(src_name "${source_dir}" NAME)
    get_filename_component(work_dir "${source_dir}" PATH)

    # Since git clone doesn't succeed if the non-empty source_dir exists,
    # create a cmake script to invoke as download command.
    # The script will delete the source directory and then call git clone.
    #
    _ep_write_gitclone_script(${tmp_dir}/${name}-gitclone.cmake ${source_dir}
      ${GIT_EXECUTABLE} ${git_repository} ${git_tag} ${git_remote_name} "${git_submodules}" "${git_shallow}" "${git_progress}" "${git_config}" ${src_name} ${work_dir}
      ${stamp_dir}/${name}-gitinfo.txt ${stamp_dir}/${name}-gitclone-lastrun.txt "${tls_verify}"
      )
    set(comment "Performing download step (git clone) for '${name}'")
    set(cmd ${CMAKE_COMMAND} -P ${tmp_dir}/${name}-gitclone.cmake)
    list(APPEND depends ${stamp_dir}/${name}-gitinfo.txt)
  elseif(hg_repository)
    find_package(Hg QUIET)
    if(NOT HG_EXECUTABLE)
      message(FATAL_ERROR "error: could not find hg for clone of ${name}")
    endif()

    get_property(hg_tag TARGET ${name} PROPERTY _EP_HG_TAG)
    if(NOT hg_tag)
      set(hg_tag "tip")
    endif()

    # For the download step, and the hg clone operation, only the repository
    # should be recorded in a configured RepositoryInfo file. If the repo
    # changes, the clone script should be run again. But if only the tag
    # changes, avoid running the clone script again. Let the 'always' running
    # update step checkout the new tag.
    #
    set(repository ${hg_repository})
    set(module)
    set(tag)
    configure_file(
      "${CMAKE_ROOT}/Modules/RepositoryInfo.txt.in"
      "${stamp_dir}/${name}-hginfo.txt"
      @ONLY
      )

    get_filename_component(src_name "${source_dir}" NAME)
    get_filename_component(work_dir "${source_dir}" PATH)

    # Since hg clone doesn't succeed if the non-empty source_dir exists,
    # create a cmake script to invoke as download command.
    # The script will delete the source directory and then call hg clone.
    #
    _ep_write_hgclone_script(${tmp_dir}/${name}-hgclone.cmake ${source_dir}
      ${HG_EXECUTABLE} ${hg_repository} ${hg_tag} ${src_name} ${work_dir}
      ${stamp_dir}/${name}-hginfo.txt ${stamp_dir}/${name}-hgclone-lastrun.txt
      )
    set(comment "Performing download step (hg clone) for '${name}'")
    set(cmd ${CMAKE_COMMAND} -P ${tmp_dir}/${name}-hgclone.cmake)
    list(APPEND depends ${stamp_dir}/${name}-hginfo.txt)
  elseif(url)
    get_filename_component(work_dir "${source_dir}" PATH)
    get_property(hash TARGET ${name} PROPERTY _EP_URL_HASH)
    if(hash AND NOT "${hash}" MATCHES "${_ep_hash_regex}")
      message(FATAL_ERROR "URL_HASH is set to\n  ${hash}\n"
        "but must be ALGO=value where ALGO is\n  ${_ep_hash_algos}\n"
        "and value is a hex string.")
    endif()
    get_property(md5 TARGET ${name} PROPERTY _EP_URL_MD5)
    if(md5 AND NOT "MD5=${md5}" MATCHES "${_ep_hash_regex}")
      message(FATAL_ERROR "URL_MD5 is set to\n  ${md5}\nbut must be a hex string.")
    endif()
    if(md5 AND NOT hash)
      set(hash "MD5=${md5}")
    endif()
    set(repository "external project URL")
    set(module "${url}")
    set(tag "${hash}")
    configure_file(
      "${CMAKE_ROOT}/Modules/RepositoryInfo.txt.in"
      "${stamp_dir}/${name}-urlinfo.txt"
      @ONLY
      )
    list(APPEND depends ${stamp_dir}/${name}-urlinfo.txt)

    list(LENGTH url url_list_length)
    if(NOT "${url_list_length}" STREQUAL "1")
      foreach(entry ${url})
        if(NOT "${entry}" MATCHES "^[a-z]+://")
          message(FATAL_ERROR "At least one entry of URL is a path (invalid in a list)")
        endif()
      endforeach()
      if("x${fname}" STREQUAL "x")
        list(GET url 0 fname)
      endif()
    endif()

    if(IS_DIRECTORY "${url}")
      get_filename_component(abs_dir "${url}" ABSOLUTE)
      set(comment "Performing download step (DIR copy) for '${name}'")
      set(cmd   ${CMAKE_COMMAND} -E remove_directory ${source_dir}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${abs_dir} ${source_dir})
    else()
      get_property(no_extract TARGET "${name}" PROPERTY _EP_DOWNLOAD_NO_EXTRACT SET)
      if("${url}" MATCHES "^[a-z]+://")
        # TODO: Should download and extraction be different steps?
        if("x${fname}" STREQUAL "x")
          set(fname "${url}")
        endif()
        if("${fname}" MATCHES [[([^/\?#]+(\.|=)(7z|tar|tar\.bz2|tar\.gz|tar\.xz|tbz2|tgz|txz|zip))([/?#].*)?$]])
          set(fname "${CMAKE_MATCH_1}")
        elseif(no_extract)
          get_filename_component(fname "${fname}" NAME)
        else()
          # Fall back to a default file name.  The actual file name does not
          # matter because it is used only internally and our extraction tool
          # inspects the file content directly.  If it turns out the wrong URL
          # was given that will be revealed during the build which is an easier
          # place for users to diagnose than an error here anyway.
          set(fname "archive.tar")
        endif()
        string(REPLACE ";" "-" fname "${fname}")
        set(file ${download_dir}/${fname})
        get_property(timeout TARGET ${name} PROPERTY _EP_TIMEOUT)
        get_property(no_progress TARGET ${name} PROPERTY _EP_DOWNLOAD_NO_PROGRESS)
        get_property(tls_verify TARGET ${name} PROPERTY _EP_TLS_VERIFY)
        get_property(tls_cainfo TARGET ${name} PROPERTY _EP_TLS_CAINFO)
        get_property(http_username TARGET ${name} PROPERTY _EP_HTTP_USERNAME)
        get_property(http_password TARGET ${name} PROPERTY _EP_HTTP_PASSWORD)
        get_property(http_headers TARGET ${name} PROPERTY _EP_HTTP_HEADER)
        set(download_script "${stamp_dir}/download-${name}.cmake")
        _ep_write_downloadfile_script("${download_script}" "${url}" "${file}" "${timeout}" "${no_progress}" "${hash}" "${tls_verify}" "${tls_cainfo}" "${http_username}:${http_password}" "${http_headers}")
        set(cmd ${CMAKE_COMMAND} -P "${download_script}"
          COMMAND)
        if (no_extract)
          set(steps "download and verify")
        else ()
          set(steps "download, verify and extract")
        endif ()
        set(comment "Performing download step (${steps}) for '${name}'")
        file(WRITE "${stamp_dir}/verify-${name}.cmake" "") # already verified by 'download_script'
      else()
        set(file "${url}")
        if (no_extract)
          set(steps "verify")
        else ()
          set(steps "verify and extract")
        endif ()
        set(comment "Performing download step (${steps}) for '${name}'")
        _ep_write_verifyfile_script("${stamp_dir}/verify-${name}.cmake" "${file}" "${hash}")
      endif()
      list(APPEND cmd ${CMAKE_COMMAND} -P ${stamp_dir}/verify-${name}.cmake
        COMMAND)
      if (NOT no_extract)
        _ep_write_extractfile_script("${stamp_dir}/extract-${name}.cmake" "${name}" "${file}" "${source_dir}")
        list(APPEND cmd ${CMAKE_COMMAND} -P ${stamp_dir}/extract-${name}.cmake)
      else ()
        set_property(TARGET ${name} PROPERTY _EP_DOWNLOADED_FILE ${file})
      endif ()
    endif()
  else()
    _ep_is_dir_empty("${source_dir}" empty)
    if(${empty})
      message(SEND_ERROR
        "No download info given for '${name}' and its source directory:\n"
        " ${source_dir}\n"
        "is not an existing non-empty directory.  Please specify one of:\n"
        " * SOURCE_DIR with an existing non-empty directory\n"
        " * URL\n"
        " * GIT_REPOSITORY\n"
        " * HG_REPOSITORY\n"
        " * CVS_REPOSITORY and CVS_MODULE\n"
        " * SVN_REVISION\n"
        " * DOWNLOAD_COMMAND"
        )
    endif()
  endif()

  get_property(log TARGET ${name} PROPERTY _EP_LOG_DOWNLOAD)
  if(log)
    set(log LOG 1)
  else()
    set(log "")
  endif()

  get_property(uses_terminal TARGET ${name} PROPERTY
    _EP_USES_TERMINAL_DOWNLOAD)
  if(uses_terminal)
    set(uses_terminal USES_TERMINAL 1)
  else()
    set(uses_terminal "")
  endif()

  ExternalProject_Add_Step(${name} download
    COMMENT ${comment}
    COMMAND ${cmd}
    WORKING_DIRECTORY ${work_dir}
    DEPENDS ${depends}
    DEPENDEES mkdir
    ${log}
    ${uses_terminal}
    )
endfunction()


function(_ep_add_update_command name)
  ExternalProject_Get_Property(${name} source_dir tmp_dir)

  get_property(cmd_set TARGET ${name} PROPERTY _EP_UPDATE_COMMAND SET)
  get_property(cmd TARGET ${name} PROPERTY _EP_UPDATE_COMMAND)
  get_property(cvs_repository TARGET ${name} PROPERTY _EP_CVS_REPOSITORY)
  get_property(svn_repository TARGET ${name} PROPERTY _EP_SVN_REPOSITORY)
  get_property(git_repository TARGET ${name} PROPERTY _EP_GIT_REPOSITORY)
  get_property(hg_repository  TARGET ${name} PROPERTY _EP_HG_REPOSITORY )
  get_property(update_disconnected_set TARGET ${name} PROPERTY _EP_UPDATE_DISCONNECTED SET)
  if(update_disconnected_set)
    get_property(update_disconnected TARGET ${name} PROPERTY _EP_UPDATE_DISCONNECTED)
  else()
    get_property(update_disconnected DIRECTORY PROPERTY EP_UPDATE_DISCONNECTED)
  endif()

  set(work_dir)
  set(comment)
  set(always)

  if(cmd_set)
    set(work_dir ${source_dir})
    if(NOT "x${cmd}" STREQUAL "x")
      set(always 1)
    endif()
  elseif(cvs_repository)
    if(NOT CVS_EXECUTABLE)
      message(FATAL_ERROR "error: could not find cvs for update of ${name}")
    endif()
    set(work_dir ${source_dir})
    set(comment "Performing update step (CVS update) for '${name}'")
    get_property(cvs_tag TARGET ${name} PROPERTY _EP_CVS_TAG)
    set(cmd ${CVS_EXECUTABLE} -d ${cvs_repository} -q up -dP ${cvs_tag})
    set(always 1)
  elseif(svn_repository)
    if(NOT Subversion_SVN_EXECUTABLE)
      message(FATAL_ERROR "error: could not find svn for update of ${name}")
    endif()
    set(work_dir ${source_dir})
    set(comment "Performing update step (SVN update) for '${name}'")
    get_property(svn_revision TARGET ${name} PROPERTY _EP_SVN_REVISION)
    get_property(svn_username TARGET ${name} PROPERTY _EP_SVN_USERNAME)
    get_property(svn_password TARGET ${name} PROPERTY _EP_SVN_PASSWORD)
    get_property(svn_trust_cert TARGET ${name} PROPERTY _EP_SVN_TRUST_CERT)
    set(svn_user_pw_args "")
    if(DEFINED svn_username)
      set(svn_user_pw_args ${svn_user_pw_args} "--username=${svn_username}")
    endif()
    if(DEFINED svn_password)
      set(svn_user_pw_args ${svn_user_pw_args} "--password=${svn_password}")
    endif()
    if(svn_trust_cert)
      set(svn_trust_cert_args --trust-server-cert)
    endif()
    set(cmd ${Subversion_SVN_EXECUTABLE} up ${svn_revision}
      --non-interactive ${svn_trust_cert_args} ${svn_user_pw_args})
    set(always 1)
  elseif(git_repository)
    unset(CMAKE_MODULE_PATH) # Use CMake builtin find module
    find_package(Git QUIET)
    if(NOT GIT_EXECUTABLE)
      message(FATAL_ERROR "error: could not find git for fetch of ${name}")
    endif()
    set(work_dir ${source_dir})
    set(comment "Performing update step for '${name}'")
    get_property(git_tag TARGET ${name} PROPERTY _EP_GIT_TAG)
    if(NOT git_tag)
      set(git_tag "master")
    endif()
    get_property(git_remote_name TARGET ${name} PROPERTY _EP_GIT_REMOTE_NAME)
    if(NOT git_remote_name)
      set(git_remote_name "origin")
    endif()
    get_property(git_submodules TARGET ${name} PROPERTY _EP_GIT_SUBMODULES)
    _ep_write_gitupdate_script(${tmp_dir}/${name}-gitupdate.cmake
      ${GIT_EXECUTABLE} ${git_tag} ${git_remote_name} "${git_submodules}" ${git_repository} ${work_dir}
      )
    set(cmd ${CMAKE_COMMAND} -P ${tmp_dir}/${name}-gitupdate.cmake)
    set(always 1)
  elseif(hg_repository)
    if(NOT HG_EXECUTABLE)
      message(FATAL_ERROR "error: could not find hg for pull of ${name}")
    endif()
    set(work_dir ${source_dir})
    set(comment "Performing update step (hg pull) for '${name}'")
    get_property(hg_tag TARGET ${name} PROPERTY _EP_HG_TAG)
    if(NOT hg_tag)
      set(hg_tag "tip")
    endif()
    if("${HG_VERSION_STRING}" STREQUAL "2.1")
      message(WARNING "Mercurial 2.1 does not distinguish an empty pull from a failed pull:
 http://mercurial.selenic.com/wiki/UpgradeNotes#A2.1.1:_revert_pull_return_code_change.2C_compile_issue_on_OS_X
 http://thread.gmane.org/gmane.comp.version-control.mercurial.devel/47656
Update to Mercurial >= 2.1.1.
")
    endif()
    set(cmd ${HG_EXECUTABLE} pull
      COMMAND ${HG_EXECUTABLE} update ${hg_tag}
      )
    set(always 1)
  endif()

  get_property(log TARGET ${name} PROPERTY _EP_LOG_UPDATE)
  if(log)
    set(log LOG 1)
  else()
    set(log "")
  endif()

  get_property(uses_terminal TARGET ${name} PROPERTY
    _EP_USES_TERMINAL_UPDATE)
  if(uses_terminal)
    set(uses_terminal USES_TERMINAL 1)
  else()
    set(uses_terminal "")
  endif()

  ExternalProject_Add_Step(${name} update
    COMMENT ${comment}
    COMMAND ${cmd}
    ALWAYS ${always}
    EXCLUDE_FROM_MAIN ${update_disconnected}
    WORKING_DIRECTORY ${work_dir}
    DEPENDEES download
    ${log}
    ${uses_terminal}
    )

  if(update_disconnected)
    _ep_get_step_stampfile(${name} skip-update skip-update_stamp_file)
    string(REPLACE "Performing" "Skipping" comment "${comment}")
    ExternalProject_Add_Step(${name} skip-update
      COMMENT ${comment}
      ALWAYS ${always}
      EXCLUDE_FROM_MAIN 1
      WORKING_DIRECTORY ${work_dir}
      DEPENDEES download
      ${log}
      ${uses_terminal}
    )
    set_property(SOURCE ${skip-update_stamp_file} PROPERTY SYMBOLIC 1)
  endif()

endfunction()


function(_ep_add_patch_command name)
  ExternalProject_Get_Property(${name} source_dir)

  get_property(cmd_set TARGET ${name} PROPERTY _EP_PATCH_COMMAND SET)
  get_property(cmd TARGET ${name} PROPERTY _EP_PATCH_COMMAND)

  set(work_dir)

  if(cmd_set)
    set(work_dir ${source_dir})
  endif()

  ExternalProject_Add_Step(${name} patch
    COMMAND ${cmd}
    WORKING_DIRECTORY ${work_dir}
    DEPENDEES download
    )
endfunction()


function(_ep_extract_configure_command var name)
  get_property(cmd_set TARGET ${name} PROPERTY _EP_CONFIGURE_COMMAND SET)
  if(cmd_set)
    get_property(cmd TARGET ${name} PROPERTY _EP_CONFIGURE_COMMAND)
  else()
    get_target_property(cmake_command ${name} _EP_CMAKE_COMMAND)
    if(cmake_command)
      set(cmd "${cmake_command}")
    else()
      set(cmd "${CMAKE_COMMAND}")
    endif()

    get_property(cmake_args TARGET ${name} PROPERTY _EP_CMAKE_ARGS)
    list(APPEND cmd ${cmake_args})

    # If there are any CMAKE_CACHE_ARGS or CMAKE_CACHE_DEFAULT_ARGS,
    # write an initial cache and use it
    get_property(cmake_cache_args TARGET ${name} PROPERTY _EP_CMAKE_CACHE_ARGS)
    get_property(cmake_cache_default_args TARGET ${name} PROPERTY _EP_CMAKE_CACHE_DEFAULT_ARGS)

    if(cmake_cache_args OR cmake_cache_default_args)
      set(_ep_cache_args_script "<TMP_DIR>/${name}-cache-$<CONFIG>.cmake")
      if(cmake_cache_args)
        _ep_command_line_to_initial_cache(script_initial_cache_force "${cmake_cache_args}" 1)
      endif()
      if(cmake_cache_default_args)
        _ep_command_line_to_initial_cache(script_initial_cache_default "${cmake_cache_default_args}" 0)
      endif()
      _ep_write_initial_cache(${name} "${_ep_cache_args_script}" "${script_initial_cache_force}${script_initial_cache_default}")
      list(APPEND cmd "-C${_ep_cache_args_script}")
    endif()

    get_target_property(cmake_generator ${name} _EP_CMAKE_GENERATOR)
    get_target_property(cmake_generator_platform ${name} _EP_CMAKE_GENERATOR_PLATFORM)
    get_target_property(cmake_generator_toolset ${name} _EP_CMAKE_GENERATOR_TOOLSET)
    if(cmake_generator)
      list(APPEND cmd "-G${cmake_generator}")
      if(cmake_generator_platform)
        list(APPEND cmd "-A${cmake_generator_platform}")
      endif()
      if(cmake_generator_toolset)
        list(APPEND cmd "-T${cmake_generator_toolset}")
      endif()
    else()
      if(CMAKE_EXTRA_GENERATOR)
        list(APPEND cmd "-G${CMAKE_EXTRA_GENERATOR} - ${CMAKE_GENERATOR}")
      else()
        list(APPEND cmd "-G${CMAKE_GENERATOR}")
      endif()
      if(cmake_generator_platform)
        message(FATAL_ERROR "Option CMAKE_GENERATOR_PLATFORM not allowed without CMAKE_GENERATOR.")
      endif()
      if(CMAKE_GENERATOR_PLATFORM)
        list(APPEND cmd "-A${CMAKE_GENERATOR_PLATFORM}")
      endif()
      if(cmake_generator_toolset)
        message(FATAL_ERROR "Option CMAKE_GENERATOR_TOOLSET not allowed without CMAKE_GENERATOR.")
      endif()
      if(CMAKE_GENERATOR_TOOLSET)
        list(APPEND cmd "-T${CMAKE_GENERATOR_TOOLSET}")
      endif()
    endif()

    list(APPEND cmd "<SOURCE_DIR><SOURCE_SUBDIR>")
  endif()

  set("${var}" "${cmd}" PARENT_SCOPE)
endfunction()

# TODO: Make sure external projects use the proper compiler
function(_ep_add_configure_command name)
  ExternalProject_Get_Property(${name} binary_dir tmp_dir)

  # Depend on other external projects (file-level).
  set(file_deps)
  get_property(deps TARGET ${name} PROPERTY _EP_DEPENDS)
  foreach(dep IN LISTS deps)
    get_property(dep_type TARGET ${dep} PROPERTY TYPE)
    if(dep_type STREQUAL "UTILITY")
      get_property(is_ep TARGET ${dep} PROPERTY _EP_IS_EXTERNAL_PROJECT)
      if(is_ep)
        _ep_get_step_stampfile(${dep} "done" done_stamp_file)
        list(APPEND file_deps ${done_stamp_file})
      endif()
    endif()
  endforeach()

  _ep_extract_configure_command(cmd ${name})

  # If anything about the configure command changes, (command itself, cmake
  # used, cmake args or cmake generator) then re-run the configure step.
  # Fixes issue https://gitlab.kitware.com/cmake/cmake/issues/10258
  #
  if(NOT EXISTS ${tmp_dir}/${name}-cfgcmd.txt.in)
    file(WRITE ${tmp_dir}/${name}-cfgcmd.txt.in "cmd='\@cmd\@'\n")
  endif()
  configure_file(${tmp_dir}/${name}-cfgcmd.txt.in ${tmp_dir}/${name}-cfgcmd.txt)
  list(APPEND file_deps ${tmp_dir}/${name}-cfgcmd.txt)
  list(APPEND file_deps ${_ep_cache_args_script})

  get_property(log TARGET ${name} PROPERTY _EP_LOG_CONFIGURE)
  if(log)
    set(log LOG 1)
  else()
    set(log "")
  endif()

  get_property(uses_terminal TARGET ${name} PROPERTY
    _EP_USES_TERMINAL_CONFIGURE)
  if(uses_terminal)
    set(uses_terminal USES_TERMINAL 1)
  else()
    set(uses_terminal "")
  endif()

  get_property(update_disconnected_set TARGET ${name} PROPERTY _EP_UPDATE_DISCONNECTED SET)
  if(update_disconnected_set)
    get_property(update_disconnected TARGET ${name} PROPERTY _EP_UPDATE_DISCONNECTED)
  else()
    get_property(update_disconnected DIRECTORY PROPERTY EP_UPDATE_DISCONNECTED)
  endif()
  if(update_disconnected)
    set(update_dep skip-update)
  else()
    set(update_dep update)
  endif()

  ExternalProject_Add_Step(${name} configure
    COMMAND ${cmd}
    WORKING_DIRECTORY ${binary_dir}
    DEPENDEES ${update_dep} patch
    DEPENDS ${file_deps}
    ${log}
    ${uses_terminal}
    )
endfunction()


function(_ep_add_build_command name)
  ExternalProject_Get_Property(${name} binary_dir)

  get_property(cmd_set TARGET ${name} PROPERTY _EP_BUILD_COMMAND SET)
  if(cmd_set)
    get_property(cmd TARGET ${name} PROPERTY _EP_BUILD_COMMAND)
  else()
    _ep_get_build_command(${name} BUILD cmd)
  endif()

  get_property(log TARGET ${name} PROPERTY _EP_LOG_BUILD)
  if(log)
    set(log LOG 1)
  else()
    set(log "")
  endif()

  get_property(uses_terminal TARGET ${name} PROPERTY
    _EP_USES_TERMINAL_BUILD)
  if(uses_terminal)
    set(uses_terminal USES_TERMINAL 1)
  else()
    set(uses_terminal "")
  endif()

  get_property(build_always TARGET ${name} PROPERTY _EP_BUILD_ALWAYS)
  if(build_always)
    set(always 1)
  else()
    set(always 0)
  endif()

  get_property(build_byproducts TARGET ${name} PROPERTY _EP_BUILD_BYPRODUCTS)

  ExternalProject_Add_Step(${name} build
    COMMAND ${cmd}
    BYPRODUCTS ${build_byproducts}
    WORKING_DIRECTORY ${binary_dir}
    DEPENDEES configure
    ALWAYS ${always}
    ${log}
    ${uses_terminal}
    )
endfunction()


function(_ep_add_install_command name)
  ExternalProject_Get_Property(${name} binary_dir)

  get_property(cmd_set TARGET ${name} PROPERTY _EP_INSTALL_COMMAND SET)
  if(cmd_set)
    get_property(cmd TARGET ${name} PROPERTY _EP_INSTALL_COMMAND)
  else()
    _ep_get_build_command(${name} INSTALL cmd)
  endif()

  get_property(log TARGET ${name} PROPERTY _EP_LOG_INSTALL)
  if(log)
    set(log LOG 1)
  else()
    set(log "")
  endif()

  get_property(uses_terminal TARGET ${name} PROPERTY
    _EP_USES_TERMINAL_INSTALL)
  if(uses_terminal)
    set(uses_terminal USES_TERMINAL 1)
  else()
    set(uses_terminal "")
  endif()

  ExternalProject_Add_Step(${name} install
    COMMAND ${cmd}
    WORKING_DIRECTORY ${binary_dir}
    DEPENDEES build
    ${log}
    ${uses_terminal}
    )
endfunction()


function(_ep_add_test_command name)
  ExternalProject_Get_Property(${name} binary_dir)

  get_property(before TARGET ${name} PROPERTY _EP_TEST_BEFORE_INSTALL)
  get_property(after TARGET ${name} PROPERTY _EP_TEST_AFTER_INSTALL)
  get_property(exclude TARGET ${name} PROPERTY _EP_TEST_EXCLUDE_FROM_MAIN)
  get_property(cmd_set TARGET ${name} PROPERTY _EP_TEST_COMMAND SET)

  # Only actually add the test step if one of the test related properties is
  # explicitly set. (i.e. the test step is omitted unless requested...)
  #
  if(cmd_set OR before OR after OR exclude)
    if(cmd_set)
      get_property(cmd TARGET ${name} PROPERTY _EP_TEST_COMMAND)
    else()
      _ep_get_build_command(${name} TEST cmd)
    endif()

    if(before)
      set(dependees_args DEPENDEES build)
    else()
      set(dependees_args DEPENDEES install)
    endif()

    if(exclude)
      set(dependers_args "")
      set(exclude_args EXCLUDE_FROM_MAIN 1)
    else()
      if(before)
        set(dependers_args DEPENDERS install)
      else()
        set(dependers_args "")
      endif()
      set(exclude_args "")
    endif()

    get_property(log TARGET ${name} PROPERTY _EP_LOG_TEST)
    if(log)
      set(log LOG 1)
    else()
      set(log "")
    endif()

    get_property(uses_terminal TARGET ${name} PROPERTY
      _EP_USES_TERMINAL_TEST)
    if(uses_terminal)
      set(uses_terminal USES_TERMINAL 1)
    else()
      set(uses_terminal "")
    endif()

    ExternalProject_Add_Step(${name} test
      COMMAND ${cmd}
      WORKING_DIRECTORY ${binary_dir}
      ${dependees_args}
      ${dependers_args}
      ${exclude_args}
      ${log}
      ${uses_terminal}
      )
  endif()
endfunction()


function(ExternalProject_Add name)
  _ep_get_configuration_subdir_suffix(cfgdir)

  # Add a custom target for the external project.
  set(cmf_dir ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles)
  set(complete_stamp_file "${cmf_dir}${cfgdir}/${name}-complete")

  # The "ALL" option to add_custom_target just tells it to not set the
  # EXCLUDE_FROM_ALL target property. Later, if the EXCLUDE_FROM_ALL
  # argument was passed, we explicitly set it for the target.
  add_custom_target(${name} ALL DEPENDS ${complete_stamp_file})
  set_property(TARGET ${name} PROPERTY _EP_IS_EXTERNAL_PROJECT 1)
  set_property(TARGET ${name} PROPERTY LABELS ${name})
  set_property(TARGET ${name} PROPERTY FOLDER "ExternalProjectTargets/${name}")

  _ep_parse_arguments(ExternalProject_Add ${name} _EP_ "${ARGN}")
  _ep_set_directories(${name})
  _ep_get_step_stampfile(${name} "done" done_stamp_file)
  _ep_get_step_stampfile(${name} "install" install_stamp_file)

  # Set the EXCLUDE_FROM_ALL target property if required.
  get_property(exclude_from_all TARGET ${name} PROPERTY _EP_EXCLUDE_FROM_ALL)
  if(exclude_from_all)
    set_property(TARGET ${name} PROPERTY EXCLUDE_FROM_ALL TRUE)
  endif()

  # The 'complete' step depends on all other steps and creates a
  # 'done' mark.  A dependent external project's 'configure' step
  # depends on the 'done' mark so that it rebuilds when this project
  # rebuilds.  It is important that 'done' is not the output of any
  # custom command so that CMake does not propagate build rules to
  # other external project targets, which may cause problems during
  # parallel builds.  However, the Ninja generator needs to see the entire
  # dependency graph, and can cope with custom commands belonging to
  # multiple targets, so we add the 'done' mark as an output for Ninja only.
  set(complete_outputs ${complete_stamp_file})
  if(${CMAKE_GENERATOR} MATCHES "Ninja")
    set(complete_outputs ${complete_outputs} ${done_stamp_file})
  endif()

  add_custom_command(
    OUTPUT ${complete_outputs}
    COMMENT "Completed '${name}'"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${cmf_dir}${cfgdir}
    COMMAND ${CMAKE_COMMAND} -E touch ${complete_stamp_file}
    COMMAND ${CMAKE_COMMAND} -E touch ${done_stamp_file}
    DEPENDS ${install_stamp_file}
    VERBATIM
    )


  # Depend on other external projects (target-level).
  get_property(deps TARGET ${name} PROPERTY _EP_DEPENDS)
  foreach(arg IN LISTS deps)
    add_dependencies(${name} ${arg})
  endforeach()

  # Set up custom build steps based on the target properties.
  # Each step depends on the previous one.
  #
  # The target depends on the output of the final step.
  # (Already set up above in the DEPENDS of the add_custom_target command.)
  #
  _ep_add_mkdir_command(${name})
  _ep_add_download_command(${name})
  _ep_add_update_command(${name})
  _ep_add_patch_command(${name})
  _ep_add_configure_command(${name})
  _ep_add_build_command(${name})
  _ep_add_install_command(${name})

  # Test is special in that it might depend on build, or it might depend
  # on install.
  #
  _ep_add_test_command(${name})
endfunction()
