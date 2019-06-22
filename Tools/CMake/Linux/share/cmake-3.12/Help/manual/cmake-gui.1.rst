.. cmake-manual-description: CMake GUI Command-Line Reference

cmake-gui(1)
************

Synopsis
========

.. parsed-literal::

 cmake-gui [<options>]
 cmake-gui [<options>] {<path-to-source> | <path-to-existing-build>}

Description
===========

The "cmake-gui" executable is the CMake GUI.  Project configuration
settings may be specified interactively.  Brief instructions are
provided at the bottom of the window when the program is running.

CMake is a cross-platform build system generator.  Projects specify
their build process with platform-independent CMake listfiles included
in each directory of a source tree with the name CMakeLists.txt.
Users build a project by using CMake to generate a build system for a
native tool on their platform.

Options
=======

.. include:: OPTIONS_HELP.txt

See Also
========

.. include:: LINKS.txt
