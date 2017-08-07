#include <stdlib.h>
#include <string.h>

static char *strmove(char *dst, const char *src)
{
	return (char*)memmove(dst, src, strlen(src) + 1);
}

static void FormatVariableName(char* Name, uint32_t bMangle)
{
	char* pRemove;
//	int i;

	//array syntax [X] becomes _0_
	//Otherwise declarations could end up as:
	//uniform sampler2D SomeTextures[0];
	//uniform sampler2D SomeTextures[1];
	while ((pRemove = strpbrk(Name, "[]")) != NULL)
	{
		strcpy(pRemove, pRemove + 1);
	}

	/* MSDN http://msdn.microsoft.com/en-us/library/windows/desktop/bb944006(v=vs.85).aspx
	The uniform function parameters appear in the
	constant table prepended with a dollar sign ($),
	unlike the global variables. The dollar sign is
	required to avoid name collisions between local
	uniform inputs and global variables of the same name.*/

	/* Leave $ThisPointer, $Element and $Globals as-is.
	Otherwise remove $ character ($ is not a valid character for GLSL variable names). */
	if (Name[0] == '$')
	{
		if (strcmp(Name, "$Element") != 0 &&
			strcmp(Name, "$Globals") != 0 &&
			strcmp(Name, "$ThisPointer") != 0)
		{
			pRemove = Name;

#if !LOSSY
			if (bMangle)
			{
				// Substitute by escape-symbol 'Wx' -> "WQ", ...
				strmove(pRemove + 2, pRemove + 1);
				pRemove[1] = pRemove[0] - '_' + 'Q';
				pRemove[0] = 'W';
			}
			else
#endif
			{
				// Bomb
				*pRemove = '_';
			}
		}
	}

	if (bMangle)
	{
		while ((pRemove = strpbrk(Name, "0123456789")) != NULL)
		{
	#if LOSSY
			// Bomb
			*pRemove = *pRemove - '0' + 'A';
	#else
			// Substitute by escape-symbol 'Qx' -> "QF", "QG", ...
			static char eLUT[10+1] = 
				/*    0123456789*/
				/*Q*/"FGJKQVWXYZ";

			strmove(pRemove + 2, pRemove + 1);
			pRemove[1] = eLUT[pRemove[0] - '0'];
			pRemove[0] = 'Q';
	#endif
		}

		while ((pRemove = strpbrk(Name, "_")) != NULL)
		{
	#if LOSSY
			// Bomb
			*pRemove = 'X';
	#else
			// Substitute by escape-symbol 'Jx' -> "JQ", ...
			strmove(pRemove + 2, pRemove + 1);
			pRemove[1] = pRemove[0] - '_' + 'Q';
			pRemove[0] = 'J';
	#endif
		}
	}
}

static void UnformatVariableName(char* Name)
{
	char* pRemove;

	// Substitute by escape-symbol 'Jx' -> "JQ", ...
	while ((pRemove = strstr(Name, "JQ")) != NULL)
	{
		strmove(pRemove + 1, pRemove + 2);
		pRemove[0] = '_';
	}

	// Substitute by escape-symbol 'Qx' -> "QF", "QG", ...
	static char eLUT[10+1] =
		/*    0123456789*/
		/*Q*/"FGJKQVWXYZ";

	pRemove = Name;
	while ((pRemove = strpbrk(pRemove, "Q")) != NULL)
	{
		for (int i = 0; i < 10; ++i)
		{
			if (pRemove[1] == eLUT[i])
			{
				strmove(pRemove + 1, pRemove + 2);
				pRemove[0] = '0' + i;
				break;
			}
		}

		++pRemove;
	}

	// Substitute by escape-symbol 'Wx' -> "WQ", ...
	while ((pRemove = strstr(Name, "WQ")) != NULL)
	{
		strmove(pRemove + 1, pRemove + 2);
		pRemove[0] = '$';
	}
}
