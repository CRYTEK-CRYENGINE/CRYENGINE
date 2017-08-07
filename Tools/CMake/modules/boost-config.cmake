
add_library(boost INTERFACE IMPORTED)

set (BOOST_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../Code/SDKs/boost")
#target_include_directories(boost INTERFACE "D:/code/ce_main/Code/SDKs/boost")
set_target_properties(boost PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${BOOST_SOURCE_DIR}")

