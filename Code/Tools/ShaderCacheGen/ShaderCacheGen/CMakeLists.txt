cmake_minimum_required(VERSION 3.6)

start_sources()
sources_platform(ALL)
add_sources("NoUberFile"
	SOURCE_GROUP "Source Files"
		"ShaderCacheGen.cpp"
		"stdafx.cpp"
		"stdafx.h"
)
end_sources()

CryConsoleApplication(ShaderCacheGen PCH "stdafx.cpp" SOLUTION_FOLDER "ShaderCacheGen")
