#ifndef _SDL_EXTENSION_H_
#define _SDL_EXTENSION_H_

#include "begin_code.h"
#ifdef __cplusplus
extern "C" {
#endif

#if defined(__ANDROID__) && __ANDROID__
/* Return the primary external storage directory. This directory may not
   currently be accessible if it has been mounted by the user on their
   computer, has been removed from the device, or some other problem has
   happened.
   If external storage is current unavailable, this will return 0.
*/
extern DECLSPEC const char * SDLCALL SDLExt_AndroidGetExternalStorageDirectory();

/* Return the name of application package */
extern DECLSPEC const char * SDLCALL SDLExt_AndroidGetPackageName();

/* Return the asset manager of the application */
extern DECLSPEC AAssetManager * SDLCALL SDLExt_GetAssetManager();


/* Return the asset manager of the application */
extern DECLSPEC const char * SDLCALL SDLExt_GetSharedLibDirectory();
	
#endif /* __ANDROID__ */

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* _SDL_EXTENSION_H_ */
