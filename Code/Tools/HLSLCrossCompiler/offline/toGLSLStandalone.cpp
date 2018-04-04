
#include "hlslcc.hpp"
#include "stdlib.h"
#include "stdio.h"
#include <algorithm>
#include <string>
#include <string.h>
#include "hash.h"
#include "serializeReflection.h"
#include "hlslcc_bin.hpp"

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include "timer.h"

#if defined(_WIN32) && !defined(PORTABLE)
#undef VALIDATE_OUTPUT
#endif

#if defined(_DEBUG)
#define VALIDATE_OUTPUT
#endif

#if defined(VALIDATE_OUTPUT)
#if defined(_WIN32)
#include <windows.h>
#include <gl/GL.h>

 #pragma comment(lib, "opengl32.lib")

	typedef char GLcharARB;		/* native character */
	typedef unsigned int GLhandleARB;	/* shader object handle */
#define GL_OBJECT_COMPILE_STATUS_ARB      0x8B81
#define GL_OBJECT_LINK_STATUS_ARB         0x8B82
#define GL_OBJECT_INFO_LOG_LENGTH_ARB        0x8B84
	typedef void (WINAPI * PFNGLDELETEOBJECTARBPROC) (GLhandleARB obj);
	typedef GLhandleARB (WINAPI * PFNGLCREATESHADEROBJECTARBPROC) (GLenum shaderType);
	typedef void (WINAPI * PFNGLSHADERSOURCEARBPROC) (GLhandleARB shaderObj, GLsizei count, const GLcharARB* *string, const GLint *length);
	typedef void (WINAPI * PFNGLCOMPILESHADERARBPROC) (GLhandleARB shaderObj);
	typedef void (WINAPI * PFNGLGETINFOLOGARBPROC) (GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *infoLog);
	typedef void (WINAPI * PFNGLGETOBJECTPARAMETERIVARBPROC) (GLhandleARB obj, GLenum pname, GLint *params);
	typedef GLhandleARB (WINAPI * PFNGLCREATEPROGRAMOBJECTARBPROC) (void);
	typedef void (WINAPI * PFNGLATTACHOBJECTARBPROC) (GLhandleARB containerObj, GLhandleARB obj);
	typedef void (WINAPI * PFNGLLINKPROGRAMARBPROC) (GLhandleARB programObj);
	typedef void (WINAPI * PFNGLUSEPROGRAMOBJECTARBPROC) (GLhandleARB programObj);
	typedef void (WINAPI * PFNGLGETSHADERINFOLOGPROC) (GLuint shader, GLsizei bufSize, GLsizei* length, GLcharARB* infoLog);

	static PFNGLDELETEOBJECTARBPROC glDeleteObjectARB;
	static PFNGLCREATESHADEROBJECTARBPROC glCreateShaderObjectARB;
	static PFNGLSHADERSOURCEARBPROC glShaderSourceARB;
	static PFNGLCOMPILESHADERARBPROC glCompileShaderARB;
	static PFNGLGETINFOLOGARBPROC glGetInfoLogARB;
	static PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameterivARB;
	static PFNGLCREATEPROGRAMOBJECTARBPROC glCreateProgramObjectARB;
	static PFNGLATTACHOBJECTARBPROC glAttachObjectARB;
	static PFNGLLINKPROGRAMARBPROC glLinkProgramARB;
	static PFNGLUSEPROGRAMOBJECTARBPROC glUseProgramObjectARB;
	static PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;

#define WGL_CONTEXT_DEBUG_BIT_ARB 0x0001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x0002
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB 0x2093
#define WGL_CONTEXT_FLAGS_ARB 0x2094
#define ERROR_INVALID_VERSION_ARB 0x2095
#define ERROR_INVALID_PROFILE_ARB 0x2096

#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126

typedef HGLRC (WINAPI * PFNWGLCREATECONTEXTATTRIBSARBPROC) (HDC hDC, HGLRC hShareContext, const int* attribList);
static PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;

extern "C" { const char* GetVersionString(GLLang language); }

void InitOpenGL()
{
	HGLRC rc;

	// setup minimal required GL
	HWND wnd = CreateWindowA(
		"STATIC",
		"GL",
		WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS |	WS_CLIPCHILDREN,
		0, 0, 16, 16,
		NULL, NULL,
		GetModuleHandle(NULL), NULL );
	HDC dc = GetDC( wnd );

	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR), 1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL,
		PFD_TYPE_RGBA, 32,
		0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0,
		16, 0,
		0, PFD_MAIN_PLANE, 0, 0, 0, 0
	};

	int fmt = ChoosePixelFormat( dc, &pfd );
	SetPixelFormat( dc, fmt, &pfd );

	rc = wglCreateContext( dc );
	wglMakeCurrent( dc, rc );

	wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

	if(wglCreateContextAttribsARB)
	{
		const int OpenGLContextAttribs [] = {
			WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
			WGL_CONTEXT_MINOR_VERSION_ARB, 5,
#if defined(_DEBUG)
			//WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB | WGL_CONTEXT_DEBUG_BIT_ARB,
#else
			//WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
#endif
			//WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
			0, 0
		};

		const HGLRC OpenGLContext = wglCreateContextAttribsARB( dc, 0, OpenGLContextAttribs );

		wglMakeCurrent(dc, OpenGLContext);

		wglDeleteContext(rc);

		rc = OpenGLContext;
	}

	glDeleteObjectARB = (PFNGLDELETEOBJECTARBPROC)wglGetProcAddress("glDeleteObjectARB");
	glCreateShaderObjectARB = (PFNGLCREATESHADEROBJECTARBPROC)wglGetProcAddress("glCreateShaderObjectARB");
	glShaderSourceARB = (PFNGLSHADERSOURCEARBPROC)wglGetProcAddress("glShaderSourceARB");
	glCompileShaderARB = (PFNGLCOMPILESHADERARBPROC)wglGetProcAddress("glCompileShaderARB");
	glGetInfoLogARB = (PFNGLGETINFOLOGARBPROC)wglGetProcAddress("glGetInfoLogARB");
	glGetObjectParameterivARB = (PFNGLGETOBJECTPARAMETERIVARBPROC)wglGetProcAddress("glGetObjectParameterivARB");
	glCreateProgramObjectARB = (PFNGLCREATEPROGRAMOBJECTARBPROC)wglGetProcAddress("glCreateProgramObjectARB");
	glAttachObjectARB = (PFNGLATTACHOBJECTARBPROC)wglGetProcAddress("glAttachObjectARB");
	glLinkProgramARB = (PFNGLLINKPROGRAMARBPROC)wglGetProcAddress("glLinkProgramARB");
	glUseProgramObjectARB = (PFNGLUSEPROGRAMOBJECTARBPROC)wglGetProcAddress("glUseProgramObjectARB");
	glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)wglGetProcAddress("glGetShaderInfoLog");
}
#endif

void PrintSingleLineError(FILE* pFile, char* error)
{
	while (*error != '\0')
	{
		char* pLineEnd = strchr(error, '\n');
		if (pLineEnd == 0)
			pLineEnd = error + strlen(error) - 1;
		fwrite(error, 1, pLineEnd - error, pFile);
		fwrite("\r", 1, 1, pFile);
		error = pLineEnd + 1;
	}
}

int TryCompileShader(GLenum eGLSLShaderType, const char* inFilename, char* shader, double* pCompileTime, int useStdErr)
{
	GLint iCompileStatus;
	GLuint hShader;
	Timer_t timer;

	InitTimer(&timer);

	InitOpenGL();

	hShader = glCreateShaderObjectARB(eGLSLShaderType);
	glShaderSourceARB(hShader, 1, (const char **)&shader, NULL);

	ResetTimer(&timer);
	glCompileShaderARB(hShader);
	*pCompileTime = ReadTimer(&timer);

	/* Check it compiled OK */
	glGetObjectParameterivARB (hShader, GL_OBJECT_COMPILE_STATUS_ARB, &iCompileStatus);

	FILE* errorFile = NULL;
	GLint iInfoLogLength = 0;
	char* pszInfoLog;

	glGetObjectParameterivARB (hShader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &iInfoLogLength);

	if (iInfoLogLength > 1)
	{
		pszInfoLog = new char[iInfoLogLength];

		if (iCompileStatus != GL_TRUE)
			printf("Error: Failed to compile GLSL shader\n");

		glGetInfoLogARB (hShader, iInfoLogLength, NULL, pszInfoLog);

		printf(pszInfoLog);

		if (!useStdErr)
		{
			std::string filename;
			filename += inFilename;
			filename += "_compileErrors.txt";

			//Dump to file
			errorFile = fopen(filename.c_str(), "w");

			fprintf(errorFile, pszInfoLog);

			fclose(errorFile);
		}
		else
		{
			// Present error to stderror with no "new lines" as required by remote shader compiler
			fprintf(stderr, "%s(-) error: ", inFilename);
			PrintSingleLineError(stderr, pszInfoLog);
			fprintf(stderr, "\rshader: ");
			PrintSingleLineError(stderr, shader);
		}

		delete [] pszInfoLog;
	}

	return iCompileStatus == GL_TRUE ? 1 : 0;
}
#endif

int fileExists(const char* path)
{
	FILE* shaderFile;
	shaderFile = fopen(path, "rb");

	if(shaderFile)
	{
		fclose(shaderFile);
		return 1;
	}
	return 0;
}

GLLang LanguageFromString(const char* str)
{
	if(strcmp(str, "es100")==0)
	{
		return LANG_ES_100;
	}
	if(strcmp(str, "es300")==0)
	{
		return LANG_ES_300;
	}
	if(strcmp(str, "es310")==0)
	{
		return LANG_ES_310;
	}
	if(strcmp(str, "120")==0)
	{
		return LANG_120;
	}
	if(strcmp(str, "130")==0)
	{
		return LANG_130;
	}
	if(strcmp(str, "140")==0)
	{
		return LANG_140;
	}
	if(strcmp(str, "150")==0)
	{
		return LANG_150;
	}
	if(strcmp(str, "330")==0)
	{
		return LANG_330;
	}
	if(strcmp(str, "400")==0)
	{
		return LANG_400;
	}
	if(strcmp(str, "410")==0)
	{
		return LANG_410;
	}
	if(strcmp(str, "420")==0)
	{
		return LANG_420;
	}
	if(strcmp(str, "430")==0)
	{
		return LANG_430;
	}
	if(strcmp(str, "440")==0)
	{
		return LANG_440;
	}
	if (strcmp(str, "450") == 0)
	{
		return LANG_450;
	}
	return LANG_DEFAULT;
}

#define MAX_PATH_CHARS 256
#define MAX_FXC_CMD_CHARS 512

typedef struct
{
	GLLang language;

	int flags;

	const char* shaderFile;
	char* outputShaderFile;

	int numLinkShaders;
	char linkIn[5][MAX_PATH_CHARS];
	char linkOut[5][MAX_PATH_CHARS];

	char* reflectPath;

	char cacheKey[MAX_PATH_CHARS];

	int bGenerateDXBC;
	int bUseFxc;
	char fxcCmdLine[MAX_FXC_CMD_CHARS];
} Options;

void InitOptions(Options* psOptions)
{
	psOptions->bGenerateDXBC = true;
	psOptions->language = LANG_DEFAULT;
	psOptions->flags = 0;
	psOptions->numLinkShaders = 0;
	psOptions->reflectPath = NULL;

	psOptions->shaderFile = NULL;

	psOptions->bUseFxc = 0;
	psOptions->linkIn[0][0] = 0;
	psOptions->linkIn[1][0] = 0;
	psOptions->linkIn[2][0] = 0;
	psOptions->linkIn[3][0] = 0;
	psOptions->linkIn[4][0] = 0;

	psOptions->linkOut[0][0] = 0;
	psOptions->linkOut[1][0] = 0;
	psOptions->linkOut[2][0] = 0;
	psOptions->linkOut[3][0] = 0;
	psOptions->linkOut[4][0] = 0;
}

void PrintHelp()
{
	printf("Command line options:\n");

	printf("\t-lang=X \t GLSL language to use. e.g. es100 or 140.\n");
	printf("\t-dxbc \t Generate a joint DXBC blob with DirectX and OpenGL shader and reflection embedded.\n");
	printf("\t-no-dxbc \t Don't generate a joint DXBC blob with DirectX and OpenGL shader and reflection embedded.\n");
	printf("\t-flags=X \t The integer value of the HLSLCC_FLAGS to used.\n");
	printf("\t-reflect=X \t File to write reflection JSON to.\n");
	printf("\t-in=X \t Shader file to compile.\n");
	printf("\t-out=X \t File to write the compiled shader from -in to.\n");

	printf("\t-linkin=X Semicolon-separated shader list. Max one per stage. Pixel shader should be given first. Hull (if present) should come before domain.\n");
	printf("\t-linkout=X Semicolon-separated list of output files to write the compilition result of -linkin shaders to.\n");
	printf("\t-hashlinkout=X Semicolon-separated list of output files to write the compilition result of -linkin shaders to. The basepath is a hash of all the file names given.\n");

	printf("\t-hashout=[dir/]out-file-name \t Output file name is a hash of 'out-file-name', put in the directory 'dir'.\n");

	printf("\t-fxc=\"CMD\" HLSL compiler command line. If specified the input shader will be first compiled through this command first and then the resulting bytecode translated.\n");

	printf("\n");
}

int GetOptions(int argc, char** argv, Options* psOptions)
{
	int i;
	int fullShaderChain = -1;
	int hashOut = 0;

	InitOptions(psOptions);

	for(i=1; i<argc; i++)
	{
		char *option;

		option = strstr(argv[i],"-help");
		if(option != NULL) 
		{
			PrintHelp();
			return 0;
		}

		option = strstr(argv[i],"-reflect=");
		if(option != NULL) 
		{
			psOptions->reflectPath = option + strlen("-reflect=");
		}

		option = strstr(argv[i],"-lang=");
		if(option != NULL)
		{
			psOptions->language = LanguageFromString((&option[strlen("-lang=")]));
		}

		option = strstr(argv[i],"-flags=");
		if(option != NULL)
		{
			psOptions->flags = atol(&option[strlen("-flags=")]);
		}

		option = strstr(argv[i],"-in=");
		if(option != NULL) 
		{
			fullShaderChain = 0;
			psOptions->shaderFile = option + strlen("-in=");
			if(!fileExists(psOptions->shaderFile))
			{
				printf("Invalid path: %s\n", psOptions->shaderFile);
				return 0;
			}
		}

		option = strstr(argv[i],"-out=");
		if(option != NULL) 
		{
			fullShaderChain = 0;
			psOptions->outputShaderFile = option + strlen("-out=");
		}

		option = strstr(argv[i],"-hashout");
		if(option != NULL) 
		{
			fullShaderChain = 0;
			psOptions->outputShaderFile = option + strlen("-hashout=");

			char* dir;
			int64_t length;

			uint64_t hash = hash64((const uint8_t*)psOptions->outputShaderFile, (uint32_t)strlen(psOptions->outputShaderFile), 0);

			uint32_t high = (uint32_t)( hash >> 32 );
			uint32_t low = (uint32_t)( hash & 0x00000000FFFFFFFF );

			dir = strrchr(psOptions->outputShaderFile, '\\');

			if(!dir)
			{
				dir = strrchr(psOptions->outputShaderFile, '//');
			}

			if(!dir)
			{
				length = 0;
			}
			else
			{
				length = (int)(dir-psOptions->outputShaderFile) + 1;
			}

			for(i=0; i< length;++i)
			{
				psOptions->cacheKey[i] = psOptions->outputShaderFile[i];
			}

			//sprintf(psOptions->cacheKey, "%x%x", high, low);
			sprintf(&psOptions->cacheKey[i], "%010llX", hash);

			psOptions->outputShaderFile = psOptions->cacheKey;
		}

		//Semicolon-separated list of shader files to compile
		//Any dependencies between these shaders will be handled
		//so that they can be linked together into a monolithic program.
		//If using separate_shader_objects with GLSL 430 or later then -linkin
		//is only needed for hull and domain tessellation shaders - not doing
		//so risks incorrect layout qualifiers in domain shaders.
		option = strstr(argv[i],"-linkin=");
		if(option != NULL) 
		{
			const char* cter;
			int shaderIndex = 0;
			int writeIndex = 0;

			cter = option + strlen("-linkin=");

			while(cter[0] != '\0')
			{
				if(cter[0] == ';')
				{
					psOptions->linkIn[shaderIndex][writeIndex] = '\0';
					shaderIndex++;
					writeIndex = 0;
					cter++;
				}

				if(shaderIndex < 5 && writeIndex < MAX_PATH_CHARS)
				{
					psOptions->linkIn[shaderIndex][writeIndex++] = cter[0];
				}

				cter++;
			}

			psOptions->linkIn[shaderIndex][writeIndex] = '\0';

			psOptions->numLinkShaders = shaderIndex+1;

			fullShaderChain = 1;
		}

		option = strstr(argv[i],"-linkout=");
		if(option != NULL) 
		{
			const char* cter;
			int shaderIndex = 0;
			int writeIndex = 0;

			cter = option + strlen("-linkout=");

			while(cter[0] != '\0')
			{
				if(cter[0] == ';')
				{
					psOptions->linkOut[shaderIndex][writeIndex] = '\0';
					shaderIndex++;
					writeIndex = 0;
					cter++;
				}

				if(shaderIndex < 5 && writeIndex < MAX_PATH_CHARS)
				{
					psOptions->linkOut[shaderIndex][writeIndex++] = cter[0];
				}

				cter++;
			}

			psOptions->linkOut[shaderIndex][writeIndex] = '\0';
		}

		option = strstr(argv[i],"-hashlinkout=");
		if(option != NULL) 
		{
			const char* cter;
			const char* fullList;
			int shaderIndex = 0;
			int writeIndex = 0;

			char files[5][MAX_PATH_CHARS];

			fullList = option + strlen("-hashlinkout=");
			cter = fullList;

			while(cter[0] != '\0')
			{
				if(cter[0] == ';')
				{
					files[shaderIndex][writeIndex] = '\0';
					shaderIndex++;
					writeIndex = 0;
					cter++;
				}

				if(shaderIndex < 5 && writeIndex < MAX_PATH_CHARS)
				{
					files[shaderIndex][writeIndex++] = cter[0];
				}

				cter++;
			}

			files[shaderIndex][writeIndex] = '\0';


			uint64_t hash = hash64((const uint8_t*)fullList, (uint32_t)strlen(fullList), 0);

			uint32_t high = (uint32_t)( hash >> 32 );
			uint32_t low = (uint32_t)( hash & 0x00000000FFFFFFFF );

			for(int i=0; i < shaderIndex+1; ++i)
			{
				char dir[MAX_PATH_CHARS];
				char fullDir[MAX_PATH_CHARS]; //includes hash

				char* separatorPtr = strrchr(&files[i][0], '\\');

				if(!separatorPtr)
				{
					separatorPtr = strrchr(&files[i][0], '//');
				}

				char* file = separatorPtr + 1;

				size_t k = 0;
				const size_t  dirChars = (size_t)(separatorPtr-&files[i][0]);
				for(; k < dirChars; ++k)
				{
					dir[k] = files[i][k];
				}

				dir[k] = '\0';

				sprintf(&fullDir[0], "%s//%010llX", dir, hash);

#if defined(_WIN32)
				_mkdir(fullDir);
#else 
				mkdir(fullDir, 0777);
#endif

				//outdir/hash/fileA...fileB
				sprintf(&psOptions->linkOut[i][0], "%s//%s", fullDir, file);
			}
		}

		option = strstr(argv[i], "-fxc=");
		if(option != NULL) 
		{
			char* cmdLine = option + strlen("-fxc=");
			size_t cmdLineLen = strlen(cmdLine);
			if (cmdLineLen == 0 || cmdLineLen + 1 >= MAX_FXC_CMD_CHARS)
				return 0;
			memcpy(psOptions->fxcCmdLine, cmdLine, cmdLineLen);
			psOptions->fxcCmdLine[cmdLineLen] = '\0';
			psOptions->bUseFxc = 1;
		}

		option = strstr(argv[i], "-dxbc");
		if (option != NULL)
		{
			psOptions->bGenerateDXBC = 1;
		}

		option = strstr(argv[i], "-no-dxbc");
		if (option != NULL)
		{
			psOptions->bGenerateDXBC = 0;
		}
	}

	return 1;
}

void *malloc_hook(size_t size)
{
	return malloc(size);
}
void *calloc_hook(size_t num,size_t size)
{
	return calloc(num,size);
}
void *realloc_hook(void *p,size_t size)
{
	return realloc(p,size);
}
void free_hook(void *p)
{
	free(p);
}

int Run(const char* srcPath, const char* destPath, GLLang language, int flags, const char* reflectPath, GLSLShader* shader, GLSLCrossDependencyData* dependencies, int useStdErr)
{
	FILE* outputFile;
	GLSLShader tempShader;
	GLSLShader* result = shader ? shader : &tempShader;
	Timer_t timer;
	int compiledOK = 0;
	double crossCompileTime = 0;
	double glslCompileTime = 0;

	HLSLcc_SetMemoryFunctions(malloc_hook,calloc_hook,free_hook,realloc_hook);
	
	InitTimer(&timer);

	ResetTimer(&timer);
	GlExtensions ext;
	ext.ARB_explicit_attrib_location  = 0;
	ext.ARB_explicit_uniform_location = 0;
	ext.ARB_shading_language_420pack  = 0;
	compiledOK = TranslateHLSLFromFile(srcPath, flags, language, &ext, dependencies, result);
	crossCompileTime = ReadTimer(&timer);

	if(compiledOK)
	{
		printf("cc time: %.2f us\n", crossCompileTime);

		if (destPath && (outputFile = fopen(destPath, "w")))
		{
			//Dump to file
			fwrite(result->sourceCode, sizeof(char), strlen(result->sourceCode), outputFile);
			fclose(outputFile);
		}
		else if (destPath && useStdErr)
		{
			fprintf(stderr, "Destination %s not writable", destPath);
		}

		if (reflectPath && (outputFile = fopen(reflectPath, "w")))
		{
			const char* jsonString = SerializeReflection(&result->reflection);
			fwrite(jsonString, sizeof(char), strlen(jsonString), outputFile);
			fclose(outputFile);
		}
		else if (reflectPath && useStdErr)
		{
			fprintf(stderr, "Reflection %s not writable", reflectPath);
		}

#if defined(VALIDATE_OUTPUT)
		{
			char* sourceCode = result->sourceCode;

			if ((flags & HLSLCC_FLAG_NO_VERSION_STRING))
			{
				const char* sourceVersion = GetVersionString(language);

				char* sourceCodeVersioned = (char*)malloc(strlen(sourceVersion) + strlen(sourceCode) + 1);
				strcpy(sourceCodeVersioned, sourceVersion);
				strcat(sourceCodeVersioned, sourceCode);
				sourceCode = sourceCodeVersioned;
			}

			compiledOK = TryCompileShader(result->shaderType, destPath ? destPath : "", sourceCode, &glslCompileTime, useStdErr);

			if ((flags & HLSLCC_FLAG_NO_VERSION_STRING))
			{
				free(sourceCode);
			}

			if (compiledOK)
			{
				printf("glsl time: %.2f us\n", glslCompileTime);
			}
		}
#endif

		if (!shader)
			FreeGLSLShader(result);
	}
	else if (useStdErr)
	{
		fprintf(stderr, "TranslateHLSLFromFile failed");
	}

	return compiledOK;
}

struct SDXBCFile
{
	FILE* m_pFile;

	bool Read(void* pElements, size_t uSize)
	{
		return fread(pElements, 1, uSize, m_pFile) == uSize;
	}

	bool Write(const void* pElements, size_t uSize)
	{
		return fwrite(pElements, 1, uSize, m_pFile) == uSize;
	}

	bool SeekRel(int32_t iOffset)
	{
		return fseek(m_pFile, iOffset, SEEK_CUR) == 0; 
	}

	bool SeekAbs(uint32_t uPosition)
	{
		return fseek(m_pFile, uPosition, SEEK_SET) == 0;
	}
};

int CombineDXBCWithGLSL(char* dxbcFileName, char* outputFileName, GLSLShader* shader)
{
	SDXBCFile dxbcFile = { fopen(dxbcFileName, "rb") };
	SDXBCFile outputFile = { fopen(outputFileName, "wb") };

	bool result = 
		dxbcFile.m_pFile != NULL && outputFile.m_pFile != NULL &&
		DXBCCombineWithGLSL(dxbcFile, outputFile, shader);

	if (dxbcFile.m_pFile != NULL)
		fclose(dxbcFile.m_pFile);
	if (outputFile.m_pFile != NULL)
		fclose(outputFile.m_pFile);

	return result;
}

int CopyCODEToGLSL(char* glslFileName, char* outputFileName)
{
	SDXBCFile glslFile = { fopen(glslFileName, "rb") };
	SDXBCFile outputFile = { fopen(outputFileName, "wb") };

	char* buf[65536];
	if ((glslFile.m_pFile != NULL) && (outputFile.m_pFile != NULL))
		while (fwrite(buf, 1, fread(buf, 1, 65536, glslFile.m_pFile), outputFile.m_pFile) != 0);

	if (glslFile.m_pFile != NULL)
		fclose(glslFile.m_pFile);
	if (outputFile.m_pFile != NULL)
		fclose(outputFile.m_pFile);

	return 1;
}

#if !defined(_MSC_VER)
#define sprintf_s(dest, size, ...) sprintf(dest, __VA_ARGS__)
#endif

#if defined(_WIN32) && defined(PORTABLE)

DWORD FilterException(DWORD uExceptionCode)
{
	const char* szExceptionName;
	char acTemp[10];
	switch (uExceptionCode)
	{
#define _CASE(_Name) \
	case _Name: \
		szExceptionName = #_Name; \
		break;
	_CASE(EXCEPTION_ACCESS_VIOLATION)
	_CASE(EXCEPTION_DATATYPE_MISALIGNMENT)
	_CASE(EXCEPTION_BREAKPOINT)
	_CASE(EXCEPTION_SINGLE_STEP)
	_CASE(EXCEPTION_ARRAY_BOUNDS_EXCEEDED)
	_CASE(EXCEPTION_FLT_DENORMAL_OPERAND)
	_CASE(EXCEPTION_FLT_DIVIDE_BY_ZERO)
	_CASE(EXCEPTION_FLT_INEXACT_RESULT)
	_CASE(EXCEPTION_FLT_INVALID_OPERATION)
	_CASE(EXCEPTION_FLT_OVERFLOW)
	_CASE(EXCEPTION_FLT_STACK_CHECK)
	_CASE(EXCEPTION_FLT_UNDERFLOW)
	_CASE(EXCEPTION_INT_DIVIDE_BY_ZERO)
	_CASE(EXCEPTION_INT_OVERFLOW)
	_CASE(EXCEPTION_PRIV_INSTRUCTION)
	_CASE(EXCEPTION_IN_PAGE_ERROR)
	_CASE(EXCEPTION_ILLEGAL_INSTRUCTION)
	_CASE(EXCEPTION_NONCONTINUABLE_EXCEPTION)
	_CASE(EXCEPTION_STACK_OVERFLOW)
	_CASE(EXCEPTION_INVALID_DISPOSITION)
	_CASE(EXCEPTION_GUARD_PAGE)
	_CASE(EXCEPTION_INVALID_HANDLE)
	//_CASE(EXCEPTION_POSSIBLE_DEADLOCK)
#undef _CASE
	default:
		sprintf_s(acTemp, "0x%08X", uExceptionCode);
		szExceptionName = acTemp;
	}

	fprintf(stderr, "Hardware exception thrown (%s)\n", szExceptionName);
	return 1;
}

#endif

int main(int argc, char** argv)
{
	Options options;
	int i;

#if defined(_WIN32) && defined(PORTABLE)
	__try
	{
#endif

		if (!GetOptions(argc, argv, &options))
		{
			return 1;
		}

		printf("Enabled uniform buffer object: %s\n", options.flags & HLSLCC_FLAG_UNIFORM_BUFFER_OBJECT ? "on" : "off");
		printf("Put origin into upper left: %s\n", options.flags & HLSLCC_FLAG_ORIGIN_UPPER_LEFT ? "on" : "off");
		printf("Pixel center integers: %s\n", options.flags & HLSLCC_FLAG_PIXEL_CENTER_INTEGER ? "on" : "off");
		printf("Don't put const uniforms into UBOs: %s\n", options.flags & HLSLCC_FLAG_GLOBAL_CONSTS_NEVER_IN_UBO ? "on" : "off");
		printf("Enable geometry shader: %s\n", options.flags & HLSLCC_FLAG_GS_ENABLED ? "on" : "off");
		printf("Enable tessellation shader: %s\n", options.flags & HLSLCC_FLAG_TESS_ENABLED ? "on" : "off");
		printf("Enable dual source blending: %s\n", options.flags & HLSLCC_FLAG_DUAL_SOURCE_BLENDING ? "on" : "off");
		printf("Use semantic names of in/out names: %s\n", options.flags & HLSLCC_FLAG_INOUT_SEMANTIC_NAMES ? "on" : "off");
		printf("Append semantic names of in/out names: %s\n", options.flags & HLSLCC_FLAG_INOUT_APPEND_SEMANTIC_NAMES ? "on" : "off");
		printf("Combine textures/samplers: %s\n", options.flags & HLSLCC_FLAG_COMBINE_TEXTURE_SAMPLERS ? "on" : "off");
		printf("Disable explicit locations: %s\n", options.flags & HLSLCC_FLAG_DISABLE_EXPLICIT_LOCATIONS ? "on" : "off");
		printf("Disable global structs: %s\n", options.flags & HLSLCC_FLAG_DISABLE_GLOBALS_STRUCT ? "on" : "off");

		printf("Convert clip space Y: %s\n", options.flags & HLSLCC_FLAG_INVERT_CLIP_SPACE_Y ? "on" : "off");
		printf("Convert clip space Z: %s\n", options.flags & HLSLCC_FLAG_CONVERT_CLIP_SPACE_Z ? "on" : "off");
		printf("Mangle identifiers per stage: %s\n", options.flags & HLSLCC_FLAG_MANGLE_IDENTIFIERS_PER_STAGE ? "on" : "off");
		printf("Avoid resource bindings: %s\n", options.flags & HLSLCC_FLAG_AVOID_RESOURCE_BINDINGS_AND_LOCATIONS ? "on" : "off");
		printf("Avoid register aliasing: %s\n", options.flags & HLSLCC_FLAG_AVOID_TEMP_REGISTER_ALIASING ? "on" : "off");
		printf("Enable instrumentation: %s\n", options.flags & HLSLCC_FLAG_TRACING_INSTRUMENTATION ? "on" : "off");
		printf("Hash input layouts: %s\n", options.flags & HLSLCC_FLAG_HASH_INPUT ? "on" : "off");
		printf("Add debug header: %s\n", options.flags & HLSLCC_FLAG_ADD_DEBUG_HEADER ? "on" : "off");
		printf("Disable version string: %s\n", options.flags & HLSLCC_FLAG_NO_VERSION_STRING ? "on" : "off");
		printf("Force precision qualifiers: %s\n", options.flags & HLSLCC_FLAG_FORCE_PRECISION_QUALIFIERS ? "on" : "off");

		if (options.bUseFxc)
		{
			char dxbcFileName  [MAX_PATH_CHARS    * 4];
			char glslFileName  [MAX_PATH_CHARS    * 4];
			char fullFxcCmdLine[MAX_FXC_CMD_CHARS * 4];
			int retValue;

			sprintf_s(dxbcFileName, sizeof(glslFileName), "%s.dxbc", options.outputShaderFile);
			sprintf_s(glslFileName, sizeof(glslFileName), "%s.code", options.outputShaderFile);

			sprintf_s(fullFxcCmdLine, sizeof(fullFxcCmdLine), "%s \"%s\" \"%s\"", options.fxcCmdLine, dxbcFileName, options.shaderFile);
			retValue = system(fullFxcCmdLine);

			if (retValue == 0)
			{
				GLSLShader shader;
				retValue = !Run(dxbcFileName, glslFileName, options.language, options.flags, options.reflectPath, &shader, NULL, 1);

				if (retValue == 0)
				{
					if (options.bGenerateDXBC)
						retValue = !CombineDXBCWithGLSL(dxbcFileName, options.outputShaderFile, &shader);
					else
						retValue = !CopyCODEToGLSL(glslFileName, options.outputShaderFile);

					FreeGLSLShader(&shader);
				}
			}

			remove(dxbcFileName);
			remove(glslFileName);

			return retValue;
		}

		if (options.shaderFile)
		{
			if (!Run(options.shaderFile, options.outputShaderFile, options.language, options.flags, options.reflectPath, NULL, NULL, 0))
			{
				return 1;
			}
		}

		GLSLCrossDependencyData depends;
		for (i = 0; i < options.numLinkShaders; ++i)
		{
			if (!Run(options.linkIn[i], options.linkOut[i][0] ? options.linkOut[i] : NULL, options.language, options.flags, options.reflectPath, NULL, &depends, 0))
			{
				return 1;
			}
		}

#if defined(_WIN32) && defined(PORTABLE)
	}
	__except(FilterException(GetExceptionCode()))
	{
		return 1;
	}
#endif


	return 0;
}
