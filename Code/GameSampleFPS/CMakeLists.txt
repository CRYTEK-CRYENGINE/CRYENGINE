option(PROJECT_SampleFPS "Build SampleFPS project" ON)
if (NOT PROJECT_SampleFPS)
	return()
endif()

add_subdirectory(Module)
