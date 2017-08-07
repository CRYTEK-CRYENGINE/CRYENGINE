add_library(Boost INTERFACE IMPORTED)
set_target_properties(Boost PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${SDK_DIR}/boost")

if(WIN64)
	add_library(BoostPython INTERFACE IMPORTED)
	set_target_properties(BoostPython PROPERTIES INTERFACE_COMPILE_DEFINITIONS BOOST_PYTHON_STATIC_LIB)
	set_property(TARGET BoostPython APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS $<$<CONFIG:Debug>:BOOST_DEBUG_PYTHON>)
	set_property(TARGET BoostPython APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS $<$<CONFIG:Debug>:BOOST_LINKING_PYTHON>)
	
	# Link flags can be passed to LINK_LIBRARIES only
	set_target_properties(BoostPython PROPERTIES INTERFACE_LINK_LIBRARIES -LIBPATH:${SDK_DIR}/boost/lib/64bit)
endif()

install(DIRECTORY "${SDK_DIR}/boost/boost" DESTINATION include/SDKs/boost)
install(FILES "${CMAKE_CURRENT_LIST_DIR}/BoostConfig.cmake" DESTINATION share/cmake)
