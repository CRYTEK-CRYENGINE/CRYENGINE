cmake_minimum_required(VERSION 3.6)

start_sources()
# -- tiff.waf_files --

sources_platform(WIN32 or WIN64)
add_sources("NoUberFile"
	SOURCE_GROUP "Config"
	    "include_win/tif_config.h"
		"include_win/tiffconf.h"
		)

sources_platform(LINUX)
add_sources("NoUberFile"
	SOURCE_GROUP "Config"
	    "include_linux/tif_config.h"
		"include_linux/tiffconf.h"
		)

sources_platform(ALL)
add_sources("NoUberFile"
	SOURCE_GROUP "Root"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_aux.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_close.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_codec.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_color.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_compress.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_dir.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_dirinfo.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_dirread.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_dirwrite.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_dumpmode.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_error.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_extension.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_fax3.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_fax3sm.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_flush.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_getimage.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_jbig.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_jpeg.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_jpeg_12.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_luv.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_lzma.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_lzw.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_next.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_ojpeg.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_open.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_packbits.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_pixarlog.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_predict.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_print.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_read.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_strip.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_swab.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_thunder.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_tile.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_unix.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_version.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_warning.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_write.c"
		"${SDK_DIR}/tiff-4.0.4/libtiff/tif_zip.c"
)

end_sources()

if(WIN32 OR WIN64 OR LINUX)
	CryEngineStaticModule(tiff SOLUTION_FOLDER "Libs")
	target_include_directories(${THIS_PROJECT} PUBLIC "${SDK_DIR}/tiff-4.0.4/libtiff")
	target_link_libraries(${THIS_PROJECT} PRIVATE zlib)
else()
	CryFileContainer(tiff SOLUTION_FOLDER "Libs")
endif()

if(WIN32 OR WIN64)
	target_include_directories(${THIS_PROJECT} PUBLIC "${CRY_LIBS_DIR}/tiff/include_win")
elseif(LINUX)
	target_compile_options(${THIS_PROJECT} PRIVATE -w)
	target_include_directories(${THIS_PROJECT} PUBLIC "${CRY_LIBS_DIR}/tiff/include_linux")
endif()
