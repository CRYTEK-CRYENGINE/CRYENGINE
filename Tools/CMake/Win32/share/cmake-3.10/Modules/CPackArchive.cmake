# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# CPackArchive
# ------------
#
# Archive CPack generator that supports packaging of sources and binaries in
# different formats:
#
#   - 7Z - 7zip - (.7z)
#   - TBZ2 (.tar.bz2)
#   - TGZ (.tar.gz)
#   - TXZ (.tar.xz)
#   - TZ (.tar.Z)
#   - ZIP (.zip)
#
# Variables specific to CPack Archive generator
# ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#
# .. variable:: CPACK_ARCHIVE_FILE_NAME
#               CPACK_ARCHIVE_<component>_FILE_NAME
#
#  Package file name without extension which is added automatically depending
#  on the archive format.
#
#  * Mandatory : YES
#  * Default   : ``<CPACK_PACKAGE_FILE_NAME>[-<component>].<extension>`` with
#                spaces replaced by '-'
#
# .. variable:: CPACK_ARCHIVE_COMPONENT_INSTALL
#
#  Enable component packaging for CPackArchive
#
#  * Mandatory : NO
#  * Default   : OFF
#
#  If enabled (ON) multiple packages are generated. By default a single package
#  containing files of all components is generated.
