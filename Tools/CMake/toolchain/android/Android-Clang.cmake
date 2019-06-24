include ("${CMAKE_CURRENT_LIST_DIR}/../../CrossPlatformSetup.cmake")
set(ANDROID TRUE)

# CMake-required settings
set(CMAKE_SYSTEM_NAME "Android")
set(CMAKE_SYSTEM_VERSION 24)
set(CMAKE_ANDROID_ARCH_ABI "arm64-v8a")

# Define a separate variable to have a value consistent with Nsight Tegra builds
set(CMAKE_ANDROID_ARCH_FOLDER "${CMAKE_ANDROID_ARCH}")

if (NOT CRYENGINE_DIR)
	set(CMAKE_ANDROID_NDK "${CMAKE_SOURCE_DIR}/Code/SDKs/android-ndk")
else()
	set(CMAKE_ANDROID_NDK "${CRYENGINE_DIR}/Code/SDKs/android-ndk")
endif()
set(CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION "clang")
set(CMAKE_ANDROID_STL_TYPE "c++_shared")

# Crytek-supplied settings
set(BUILD_PLATFORM "android-${CMAKE_ANDROID_ARCH_ABI}")
set(OUTPUT_DIRECTORY_NAME "android")
set(CMAKE_ANDROID_API_MIN 24)
set(CMAKE_CONFIGURATION_TYPES Debug Profile Release)
set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "Reset the configurations to what we need" FORCE)

include ("${CMAKE_CURRENT_LIST_DIR}/../../CRYENGINE-CLANG.cmake")
set(ANDROID_FLAGS "-DANDROID -D__ANDROID__ -DLINUX -D__linux__ -DHAS_STPCPY")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ANDROID_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ANDROID_FLAGS}")
set(CMAKE_LINKER_FLAGS "-Wl,--build-id,--warn-shared-textrel,--fatal-warnings,--gc-sections,-z,nocopyreloc,--fix-cortex-a8,--exclude-libs,libunwind.a -Qunused-arguments")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE CACHE BOOL "No RPATH on Android")

macro(configure_android_build)
	set(options DEBUGGABLE)
	set(oneValueArgs PACKAGE APP_NAME VERSION_CODE VERSION_NAME)
	set(multiValueArgs PERMISSIONS)
	cmake_parse_arguments(ANDROID "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
	string(TOLOWER "${ANDROID_DEBUGGABLE}" ANDROID_DEBUGGABLE) 
	
	set(apk_folder "${CMAKE_BINARY_DIR}/AndroidLauncher/apk_data")
	file(REMOVE_RECURSE "${apk_folder}")
	file(MAKE_DIRECTORY "${apk_folder}/src")
	file(MAKE_DIRECTORY "${apk_folder}/lib/${CMAKE_ANDROID_ARCH}")
	file(MAKE_DIRECTORY "${apk_folder}/gen")
	file(MAKE_DIRECTORY "${apk_folder}/bin")
	file(MAKE_DIRECTORY "${apk_folder}/build")

		
	# TODO: implement process_android_java_files from waf compile rules

	#Generate APK manifest
	file(WRITE "${apk_folder}/AndroidManifest.xml" 
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
		"	<!-- BEGIN_INCLUDE(manifest) -->\n"
		"	<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\"\n"
		"		package=\"${ANDROID_PACKAGE}\"\n"
		"		android:versionCode=\"${ANDROID_VERSION_CODE}\"\n"
		"		android:debuggable=\"${ANDROID_DEBUGGABLE}\"\n"
		"		android:versionName=\"${ANDROID_VERSION_CODE}\" >\n"
		"\n"
		"		<!-- This is the platform API where NativeActivity was introduced. -->\n"
		"		<uses-sdk android:minSdkVersion=\"${CMAKE_ANDROID_API_MIN}\" />\n"
		"\n"
		"		<!-- OpenGL ES 3.0 -->\n"
		"		<uses-feature android:glEsVersion=\"0x00030000\" android:required=\"true\" /> \n"
		"\n"
		"		<!-- Required texture compression support -->\n"
		"		<supports-gl-texture android:name=\"GL_IMG_texture_compression_pvrtc\" />\n"
		"		<supports-gl-texture android:name=\"GL_EXT_texture_compression_s3tc\" />\n"
		"\n"
		"		<!-- Required permissions -->\n")
	foreach(permission ${ANDROID_PERMISSIONS})
		file(APPEND "${apk_folder}/AndroidManifest.xml" 
			"		<uses-permission android:name=\"android.permission.${permission}\" /> \n"
		)
	endforeach()
	file(APPEND "${apk_folder}/AndroidManifest.xml" 
		"\n"
		"		<application android:label=\"@string/app_name\">\n"
		"			<!-- Our activity loads the generated bootstrapping class in order\n"
		"				to prelaod any third party shared libraries -->\n"
		"			<activity android:name=\"CryEngineActivity\"\n"
		"				android:label=\"@string/app_name\"\n"
		"				android:theme=\"@android:style/Theme.NoTitleBar.Fullscreen\"\n"
		"				android:screenOrientation=\"landscape\"\n"
		"				android:configChanges=\"orientation|keyboardHidden\">\n"
		"\n"
		"				<!-- Tell NativeActivity the name of our .so -->\n"
		"				<meta-data android:name=\"android.app.lib_name\" android:value=\"AndroidLauncher\" />\n"
		"				<intent-filter>\n"
		"					<action android:name=\"android.intent.action.MAIN\" />\n"
		"					<category android:name=\"android.intent.category.LAUNCHER\" />\n"
		"				</intent-filter>\n"
		"			</activity>\n"
		"		</application>\n"
		"\n"
		"	</manifest> \n"
		"	<!-- END_INCLUDE(manifest) -->\n"
	)

	#Generate resources
	file(MAKE_DIRECTORY "${apk_folder}/res/values")
	file(WRITE "${apk_folder}/res/values/string.xml"
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
		"<resources>\n"
		"	<string name=\"app_name\">${ANDROID_APP_NAME}</string>\n"
		"</resources>\n"
	)

	#Generate gdb.setup
	file(WRITE "${OUTPUT_DIRECTORY}/gdb.setup" "set solib-search-path \"${OUTPUT_DIRECTORY}/lib_debug/${CMAKE_ANDROID_ARCH}\"")
	
	#Generate ANT files
	file(WRITE "${apk_folder}/build.xml"
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"	<project name=\"AndroidLauncher\" default=\"help\">\n"
		"\n"
		"		<property file=\"build.properties\" />\n"
		"\n"
		"		<!--REGION-BEGIN: The following properties allow Nsight Tegra to maintain build results per-configuration (debug, release, etc)-->\n"
		"		<target name=\"-pre-compile\">\n"
		"			<property name=\"nsight.tegra.project.all.jars.path\" value=\"\${toString:project.all.jars.path}\" />\n"
		"			<path id=\"project.all.jars.path\">\n"
		"				<path path=\"\${nsight.tegra.project.all.jars.path}\" />\n"
		"				<fileset dir=\"\${jar.libs.dir}\">\n"
		"					<include name=\"*.jar\" />\n"
		"				</fileset>\n"
		"			</path>\n"
		"			<echo>Ant Java version: \${ant.java.version}</echo>\n"
		"			<echo>Java Tools version: \${java.version}</echo>\n"
		"		</target>\n"
		"		<!--REGION-END-->\n"
		"\n"
		"		<!-- quick check on sdk.dir -->\n"
		"		<fail	message=\"sdk.dir is missing. Make sure to generate local.properties using 'android update project' or to inject it through the ANDROID_HOME environment variable.\"\n"
		"				unless=\"sdk.dir\" />\n"
		"\n"
		"		<!-- Import the actual build file.\n"
		"\n"
		"			To customize existing targets, there are two options:\n"
		"			1) Customize only one target:\n"
		"				- copy/paste the target into this file, *before* the <import> task.\n"
		"				#NAME?\n"
		"			2) Customize the whole content of build.xml\n"
		"				- copy/paste the content of the rules files (minus the top node) into this file, replacing the <import> task.\n"
		"				#NAME?\n"
		"\n"
		"			***********************\n"
		"			****** IMPORTANT ******\n"
		"			***********************\n"
		"			In all cases you must update the value of version-tag below to read 'custom' instead of an integer,\n"
		"			in order to avoid having your file be overridden by tools such as \"android update project\"\n"
		"		-->\n"
		"\n"
		"		<!-- version-tag: 1 -->\n"
		"		<import file=\"\${sdk.dir}/tools/ant/build.xml\" />\n"
		"	</project>\n"		
	)

	file(TO_CMAKE_PATH "$ENV{ANDROID_HOME}" ANDROID_SDK_DIR)

	file(WRITE "${apk_folder}/build.properties"
		"# This file is automatically generated by Android Tools.\n"
		"# Do not modify this file -- YOUR CHANGES WILL BE ERASED!\n"
		"\n"
		"target=android-${CMAKE_SYSTEM_VERSION}\n"
		"\n"
		"gen.absolute.dir=./gen\n"
		"out.dir=./bin\n"
		"jar.libs.dir=./libs\n"
		"native.libs.absolute.dir=./lib\n"
		"sdk.dir=${ANDROID_SDK_DIR}\n"
	)

endmacro()



macro(configure_android_launcher name)
	set(apk_folder "${CMAKE_BINARY_DIR}/${name}/apk_data")

	#Copy sources
	foreach(source_file ${SOURCE_FILES_JAVA})
		if (${source_file} MATCHES ".*\\.\\java$")
			file(COPY "${source_file}" DESTINATION "${apk_folder}/src")
		endif()
	endforeach()

	#Copy external libs
	file(COPY "${CMAKE_ANDROID_NDK}/sources/cxx-stl/llvm-libc++/libs/${CMAKE_ANDROID_ARCH_ABI}/libc++_shared.so" DESTINATION "${apk_folder}/lib/${CMAKE_ANDROID_ARCH_ABI}")
	file(COPY "${CMAKE_ANDROID_NDK}/prebuilt/android-${CMAKE_ANDROID_ARCH}/gdbserver/gdbserver" DESTINATION "${apk_folder}/lib/${CMAKE_ANDROID_ARCH_ABI}")
	file(COPY "${CRYENGINE_DIR}/Code/Tools/SDLExtension/lib/android-${CMAKE_ANDROID_ARCH_ABI}/libSDL2Ext.so" DESTINATION "${apk_folder}/lib/${CMAKE_ANDROID_ARCH_ABI}")
	file(COPY "${SDK_DIR}/SDL2/android/${CMAKE_ANDROID_ARCH_ABI}/libSDL2.so" DESTINATION "${apk_folder}/lib/${CMAKE_ANDROID_ARCH_ABI}")
	file(COPY "${ANDROID_SDK_DIR}/extras/android/support/v4/android-support-v4.jar" DESTINATION "${apk_folder}/libs")

	if (AUDIO_FMOD)
		file(COPY "${SDK_DIR}/Audio/fmod/android/api/core/lib/${CMAKE_ANDROID_ARCH_ABI}/libfmod.so" DESTINATION "${apk_folder}/lib/${CMAKE_ANDROID_ARCH_ABI}")
		file(COPY "${SDK_DIR}/Audio/fmod/android/api/studio/lib/${CMAKE_ANDROID_ARCH_ABI}/libfmodstudio.so" DESTINATION "${apk_folder}/lib/${CMAKE_ANDROID_ARCH_ABI}")
		file(COPY "${SDK_DIR}/Audio/fmod/android/api/core/lib/fmod.jar" DESTINATION "${apk_folder}/libs")
	endif()

	#Make ANT run
	file(TO_NATIVE_PATH "${OUTPUT_DIRECTORY}" NATIVE_OUTDIR)
	file(TO_NATIVE_PATH "${CMAKE_ANDROID_NDK}" NATIVE_NDKROOT)
	file(TO_NATIVE_PATH "${apk_folder}" apk_folder_native)

	set(shared_copy)
	if(NOT OPTION_STATIC_LINKING)
		foreach(mod ${SHARED_MODULES})
			set(shared_copy ${shared_copy} COMMAND copy "${NATIVE_OUTDIR}\\lib${mod}.so" "${apk_folder_native}\\lib\\${CMAKE_ANDROID_ARCH_ABI}" )
		endforeach()
	endif()

	add_custom_command(TARGET AndroidLauncher POST_BUILD
		COMMAND copy /Y "${NATIVE_OUTDIR}\\libAndroidLauncher.so" "${apk_folder_native}\\lib\\${CMAKE_ANDROID_ARCH_ABI}\\libAndroidLauncher.so"
		${shared_copy}
		COMMAND copy "${NATIVE_OUTDIR}\\lib${name}.so" ${so_paths} "${apk_folder_native}\\lib\\${CMAKE_ANDROID_ARCH_ABI}\\"
		COMMAND copy /Y "$<$<CONFIG:Release>:nul>$<$<NOT:$<CONFIG:Release>>:${NATIVE_NDKROOT}\\sources\\third_party\\vulkan\\src\\build-android\\jniLibs\\${CMAKE_ANDROID_ARCH_ABI}\\lib*>" "${apk_folder_native}\\lib\\${CMAKE_ANDROID_ARCH_ABI}"
		COMMAND call "$ENV{ANT_HOME}/bin/ant" clean
		COMMAND call "$ENV{ANT_HOME}/bin/ant" debug
		COMMAND copy "${apk_folder_native}\\bin\\${name}-debug.apk" "${NATIVE_OUTDIR}\\${name}.apk" WORKING_DIRECTORY "${apk_folder}")	
endmacro()
