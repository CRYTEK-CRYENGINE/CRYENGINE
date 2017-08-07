#include "SDL_stdinc.h"

#include <jni.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <SDL_error.h>

extern JNIEnv* Android_JNI_GetEnv(void);

static int s_active = 0;
struct LocalReferenceHolder
{
    JNIEnv *m_env;
    const char *m_func;
};

static jclass mActivityClass;
static jobject mAssetManager;
static AAssetManager *manager;

static struct LocalReferenceHolder LocalReferenceHolder_Setup(const char *func)
{
    struct LocalReferenceHolder refholder;
    refholder.m_env = NULL;
    refholder.m_func = func;
#ifdef DEBUG_JNI
    SDL_Log("Entering function %s", func);
#endif
    return refholder;
}

static SDL_bool LocalReferenceHolder_Init(struct LocalReferenceHolder *refholder, JNIEnv *env)
{
    const int capacity = 16;
    if ((*env)->PushLocalFrame(env, capacity) < 0) {
        SDL_SetError("Failed to allocate enough JVM local references");
        return SDL_FALSE;
    }
    ++s_active;
    refholder->m_env = env;
    return SDL_TRUE;
}

static void LocalReferenceHolder_Cleanup(struct LocalReferenceHolder *refholder)
{
#ifdef DEBUG_JNI
    SDL_Log("Leaving function %s", refholder->m_func);
#endif
    if (refholder->m_env) {
        JNIEnv* env = refholder->m_env;
        (*env)->PopLocalFrame(env, NULL);
        --s_active;
    }
}

static SDL_bool LocalReferenceHolder_IsActive()
{
    return s_active > 0;    
}

const char * SDLExt_AndroidGetExternalStorageDirectory();
const char * SDLExt_AndroidGetPackageName();
const char * SDLExt_GetSharedLibDirectory();

void SDLExt_Android_Init(JNIEnv* env, jclass cls, jobject mgr)
{
    mActivityClass = (jclass)((*env)->NewGlobalRef(env, cls));
    mAssetManager = (jobject)((*env)->NewGlobalRef(env, mgr));

    manager = AAssetManager_fromJava(env, mAssetManager);
    if (manager == NULL) {
	__android_log_print(ANDROID_LOG_ERROR, "SDLExt", "Error loading asset manager");
    }
    else {
	__android_log_print(ANDROID_LOG_VERBOSE, "SDLExt", "Asset manager loaded.");
    }

	SDLExt_AndroidGetExternalStorageDirectory();
	SDLExt_AndroidGetPackageName();
	SDLExt_GetSharedLibDirectory();
}

char *s_AndroidExternalStorageDirectory = NULL;
const char * SDLExt_AndroidGetExternalStorageDirectory()
{
    if (!s_AndroidExternalStorageDirectory) 
	{
		struct LocalReferenceHolder refs = LocalReferenceHolder_Setup(__FUNCTION__);
		jmethodID mid;
		jclass cls;
		jobject fileObject;
		jstring pathString;
		const char *path;

		JNIEnv *env = Android_JNI_GetEnv();
		if (!LocalReferenceHolder_Init(&refs, env)) {
			LocalReferenceHolder_Cleanup(&refs);
			return NULL;
		}

		cls = (*env)->FindClass(env, "android/os/Environment");
		mid = (*env)->GetStaticMethodID(env, cls,
						"getExternalStorageDirectory", "()Ljava/io/File;");
		fileObject = (*env)->CallStaticObjectMethod(env, cls, mid);
		if (!fileObject) {
			SDL_SetError("Couldn't get external storage directory");
			LocalReferenceHolder_Cleanup(&refs);
			return NULL;
		}

		mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, fileObject),
					  "getAbsolutePath", "()Ljava/lang/String;");
		pathString = (jstring)(*env)->CallObjectMethod(env, fileObject, mid);

		path = (*env)->GetStringUTFChars(env, pathString, NULL);
		s_AndroidExternalStorageDirectory = SDL_strdup(path);
		(*env)->ReleaseStringUTFChars(env, pathString, path);

		LocalReferenceHolder_Cleanup(&refs);
	}
	return s_AndroidExternalStorageDirectory;
}

char *s_AndroidPackageName = NULL;
const char * SDLExt_AndroidGetPackageName()
{
	if (!s_AndroidPackageName)
	{
		struct LocalReferenceHolder refs = LocalReferenceHolder_Setup(__FUNCTION__);
		jmethodID mid;
		jobject context;
		jstring nameString;
		const char *name;

		JNIEnv *env = Android_JNI_GetEnv();
		if (!LocalReferenceHolder_Init(&refs, env)) {
			LocalReferenceHolder_Cleanup(&refs);
			return NULL;
		}

		/* context = SDLActivity.getContext(); */
		mid = (*env)->GetStaticMethodID(env, mActivityClass,"getContext", "()Landroid/content/Context;");
		context = (*env)->CallStaticObjectMethod(env, mActivityClass, mid);

		/* nameString = context.getPackageName(); */
		mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, context),
					  "getPackageName", "()Ljava/lang/String;");
		nameString = (jstring)(*env)->CallObjectMethod(env, context, mid);

		name = (*env)->GetStringUTFChars(env, nameString, NULL);
		s_AndroidPackageName = SDL_strdup(name);
		(*env)->ReleaseStringUTFChars(env, nameString, name);

		LocalReferenceHolder_Cleanup(&refs);
    }

    return s_AndroidPackageName;
}

AAssetManager * SDLExt_GetAssetManager()
{
	return manager;
}

char *s_shareLibDirectory = NULL;
const char * SDLExt_GetSharedLibDirectory()
{
	if (!s_shareLibDirectory) 
	{
		struct LocalReferenceHolder refs = LocalReferenceHolder_Setup(__FUNCTION__);
		jmethodID mid;
		JNIEnv *env = Android_JNI_GetEnv();
		if (!LocalReferenceHolder_Init(&refs, env)) {
			LocalReferenceHolder_Cleanup(&refs);
			return NULL;
		}

		
		mid = (*env)->GetStaticMethodID(env, mActivityClass, "getContext", "()Landroid/content/Context;");
		jobject context = (*env)->CallStaticObjectMethod(env, mActivityClass, mid);

		//jclass PackageManager = env->FindClass("android/content/pm/PackageManager");
	//	jclass PackageItemInfo = env->FindClass("android/content/pm/PackageItemInfo");

		/* context = SDLActivity.getContext(); */
		jclass Context_class = (*env)->FindClass(env, "android/content/Context");
		//jclass PackageManager = (*env)->FindClass(env, "android/content/pm/PackageManager");
		//jclass PackageItemInfo = (*env)->FindClass("android/content/pm/PackageItemInfo");


		jmethodID midGetApplicationInfo = (*env)->GetMethodID(env, Context_class, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
		jobject appInfo = (*env)->CallObjectMethod(env, context, midGetApplicationInfo);



		//jobject context = (*env)->CallStaticObjectMethod(env, mActivityClass, mid);

		/* applicationInfo = context.GetApplicationInfo();*/
		//jmethodID getPackageManager = (*env)->GetMethodID(context, "getPackageManager", "()Landroid/content/pm/PackageManager;");
		//jmethodID getApplicationInfo = (*env)->GetMethodID(PackageManager, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");

		//jmethodID midGetApplicationInfo = (*env)->GetMethodID(env, context, "getApplicationInfo", "()Landroid/content/pm/PackageItemInfo;" );
		//jobject appInfo = (*env)->CallObjectMethod(env, context, midGetApplicationInfo);

		// ARM64
		// libPath = applicationInfo.nativeLibraryDir
		/* nameString = context.getPackageName(); */
		jfieldID fieldID = (*env)->GetFieldID(env, (*env)->GetObjectClass(env, appInfo), "nativeLibraryDir", "Ljava/lang/String;");
		jstring nativeLibraryDir = (jstring)(*env)->GetObjectField(env, appInfo, fieldID);
		const char* str = (*env)->GetStringUTFChars(env, nativeLibraryDir, NULL);
		s_shareLibDirectory = SDL_strdup(str);
		(*env)->ReleaseStringUTFChars(env, nativeLibraryDir, str);

		LocalReferenceHolder_Cleanup(&refs);
	}

	return s_shareLibDirectory;
}
