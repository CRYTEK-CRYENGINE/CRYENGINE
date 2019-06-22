#START-FILE-LIST

start_sources()
sources_platform(ALL)
add_sources("NoUberFile"
	SOURCE_GROUP "Root"
		"${SDK_DIR}/qpOASES/src/BLASReplacement.cpp"
		"${SDK_DIR}/qpOASES/src/Bounds.cpp"
		"${SDK_DIR}/qpOASES/src/Constraints.cpp"
		"${SDK_DIR}/qpOASES/src/Flipper.cpp"
		"${SDK_DIR}/qpOASES/src/Indexlist.cpp"
		"${SDK_DIR}/qpOASES/src/LAPACKReplacement.cpp"
		"${SDK_DIR}/qpOASES/src/Matrices.cpp"
		"${SDK_DIR}/qpOASES/src/MessageHandling.cpp"
		"${SDK_DIR}/qpOASES/src/Options.cpp"
		"${SDK_DIR}/qpOASES/src/OQPinterface.cpp"
		"${SDK_DIR}/qpOASES/src/QProblem.cpp"
		"${SDK_DIR}/qpOASES/src/QProblemB.cpp"
		"${SDK_DIR}/qpOASES/src/SolutionAnalysis.cpp"
		"${SDK_DIR}/qpOASES/src/SQProblem.cpp"
		"${SDK_DIR}/qpOASES/src/SubjectTo.cpp"
		"${SDK_DIR}/qpOASES/src/Utils.cpp"
)

end_sources()
#END-FILE-LIST

CryEngineStaticModule(qpOASES SOLUTION_FOLDER "Libs")

if (ORBIS OR LINUX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_options(${THIS_PROJECT} PRIVATE
	-Wno-overloaded-virtual
	-Wno-dangling-else
	-Wno-misleading-indentation
	-Wno-enum-compare
	)
endif()
target_include_directories(${THIS_PROJECT} PUBLIC "${SDK_DIR}/qpOASES/include")
