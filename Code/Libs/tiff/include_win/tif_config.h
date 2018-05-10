#define AVOID_WIN32_FILEIO 1
#define CHECK_JPEG_YCBCR_SUBSAMPLING 1
#define DEFAULT_EXTRASAMPLE_AS_ALPHA 1
#define STRIPCHOP_DEFAULT TIFF_STRIPCHOP
#define STRIP_SIZE_DEFAULT 8192
#define LOGLUV_SUPPORT 1
#define NEXT_SUPPORT 1
#define THUNDER_SUPPORT 1
#define LZW_SUPPORT 1
#define PACKBITS_SUPPORT 1
#define CCITT_SUPPORT 1
#define TIF_PLATFORM_CONSOLE 1
#define FILLODER_LSB2MSB 1
// the following two items require zlib to be available
#define ZIP_SUPPORT 1
#define PIXARLOG_SUPPORT 1
 
// We must disable some warnings (level 3) to fulfill CryEngine's
// "warnings are not allowed" requirement.
// Examples of disabled warnings:
//    libtiff/tif_write.c(236):    warning C4018: '<=' : signed/unsigned mismatch
//    libtiff/tif_pixarlog.c(909): warning C4244: '=' : conversion from 'tmsize_t' to 'uInt', possible loss of data
//    libtiff/tif_print.c(399):    warning C4244: 'initializing' : conversion from '__int64' to 'int', possible loss of data
//    libtiff/tif_print.c(678):    warning C4267: 'function' : conversion from 'size_t' to 'int', possible loss of data
//    libtiff/tif_unix.c(67):      warning C4267: 'function' : conversion from 'size_t' to 'unsigned int', possible loss of data
#if defined(_MSC_VER)
# pragma warning(disable: 4018)
# pragma warning(disable: 4244)
# pragma warning(disable: 4267)
#endif
 
#include "tif_config.vc.h"
