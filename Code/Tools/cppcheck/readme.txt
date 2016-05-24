=========
Cppcheck
=========


About

    The original name of this program is "C++check" but it was later changed to "cppcheck".

Manual

    A manual is available online:
    http://cppcheck.sourceforge.net/manual.pdf

Compiling

    Any C++11 compiler should work. For compilers with partial C++11 support it may work. If
    your compiler has the C++11 features that are available in Visual Studio 2010 then it
    will work. If nullptr is not supported by your compiler then this can be emulated using
    the header lib/cxx11emu.h.

    To build the GUI, you need Qt.

    When building the command line tool, PCRE is optional. It is used if you build with rules.

    There are multiple compilation choices:
      * qmake - cross platform build tool
      * cmake - cross platform build tool
      * Windows: Visual Studio
      * Windows: Qt Creator + mingw
      * gnu make
      * g++ 4.6 (or later)
      * clang++

    qmake
    =====
        You can use the gui/gui.pro file to build the GUI.
            cd gui
            qmake
            make

    Visual Studio
    =============
        Use the cppcheck.sln file. The file is configured for Visual Studio 2015, but the platform
        toolset can be changed easily to older or newer versions. The solution contains platform
        targets for both x86 and x64.

        To compile with rules, select "Release-PCRE" or "Debug-PCRE" configuration.
        pcre.lib (pcre64.lib for x64 builds) and pcre.h are expected to be in /externals then.

    Qt Creator + mingw
    ==================
        The PCRE dll is needed to build the CLI. It can be downloaded here:
            http://software-download.name/pcre-library-windows/

    gnu make
    ========
        Simple build (no dependencies):
            make

        The recommended release build is:
            make SRCDIR=build CFGDIR=cfg HAVE_RULES=yes

        Flags:
        SRCDIR=build   : Python is used to optimise cppcheck
        CFGDIR=cfg     : Specify folder where .cfg files are found
        HAVE_RULES=yes : Enable rules (pcre is required if this is used)

    g++ (for experts)
    =================
        If you just want to build Cppcheck without dependencies then you can use this command:
            g++ -o cppcheck -std=c++0x -include lib/cxx11emu.h -Iexternals/tinyxml -Ilib cli/*.cpp lib/*.cpp externals/tinyxml/*.cpp

        If you want to use --rule and --rule-file then dependencies are needed:
            g++ -o cppcheck -std=c++0x -include lib/cxx11emu.h -lpcre -DHAVE_RULES -Ilib -Iexternals/tinyxml cli/*.cpp lib/*.cpp externals/tinyxml/*.cpp

    mingw
    =====
        The "LDFLAGS=-lshlwapi" is needed when building with mingw
            mingw32-make LDFLAGS=-lshlwapi

    other compilers/ide
    ===================

        1. Create a empty project file / makefile.
        2. Add all cpp files in the cppcheck cli and lib folders to the project file / makefile.
        3. Compile.

Cross compiling Win32 (CLI) version of Cppcheck in Linux

    sudo apt-get install mingw32
    make CXX=i586-mingw32msvc-g++ LDFLAGS="-lshlwapi"
    mv cppcheck cppcheck.exe

Webpage

    http://cppcheck.sourceforge.net/
