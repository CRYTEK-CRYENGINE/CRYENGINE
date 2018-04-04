if(LINUX64)
	add_library(menuw SHARED IMPORTED GLOBAL)
	set_target_properties(menuw PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${SDK_DIR}/ncurses/include")
	set_target_properties(menuw PROPERTIES IMPORTED_LOCATION "${SDK_DIR}/ncurses/lib/libmenuw.so")

	add_library(formw SHARED IMPORTED GLOBAL)
	set_target_properties(formw PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${SDK_DIR}/ncurses/include")
	set_target_properties(formw PROPERTIES IMPORTED_LOCATION "${SDK_DIR}/ncurses/lib/libformw.so")

	add_library(ncursesw SHARED IMPORTED GLOBAL)
	set_target_properties(ncursesw PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${SDK_DIR}/ncurses/include")
	set_target_properties(ncursesw PROPERTIES IMPORTED_LOCATION "${SDK_DIR}/ncurses/lib/libncursesw.so")

	set_target_properties(ncursesw PROPERTIES LINK_INTERFACE_LIBRARIES "menuw;formw")
	
	deploy_runtime_files("${SDK_DIR}/ncurses/lib/libncursesw.so*")
	deploy_runtime_files("${SDK_DIR}/ncurses/lib/libmenuw.so*")
	deploy_runtime_files("${SDK_DIR}/ncurses/lib/libformw.so*")
endif()
