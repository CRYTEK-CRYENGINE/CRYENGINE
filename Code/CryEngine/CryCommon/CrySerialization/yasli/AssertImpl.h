/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once
#include "MemoryWriter.h"
#include "BinArchive.h"
#include <stdio.h>
#include <stdarg.h>

namespace yasli{

static LogHandler logHandler;
void setLogHandler(LogHandler handler)
{
	logHandler = handler;
}

#ifndef NDEBUG
static bool interactiveAssertion = true;
static bool testMode = false;

void setInteractiveAssertion(bool interactive)
{
    interactiveAssertion = interactive;
}

void setTestMode(bool _testMode)
{
	interactiveAssertion = false;
    testMode = _testMode;
}

bool assertionDialog(const char* function, const char* fileName, int line, const char* expr, const char* str, ...)
{
	MemoryWriter text;
	text << fileName << "(" << line << "): error: " << "Assertion in " << function << "(): " << expr; // output in msvc error format

#ifdef WIN32
	int hash = calcHash(text.c_str());

	const int hashesMax = 1000;
	static int hashes[hashesMax];
	int index = 0;
	for(; index < hashesMax; ++index)
		if(hashes[index] == hash)
			return false;
		else if(!hashes[index])
			break;
#endif

	char buffer[4000];
	va_list args;
	va_start(args, str);
#ifdef _MSC_VER
	vsprintf_s(buffer, sizeof(buffer), str, args);
#else
	vsnprintf(buffer, sizeof(buffer) - 1, str, args);
	buffer[sizeof(buffer)-1] = '\0';
#endif
	va_end(args);

	text << " ( " << buffer << " )";
	text << "\n";

	if(logHandler)
		logHandler(text.c_str());

#ifdef WIN32
    if(interactiveAssertion || IsDebuggerPresent()){
        int result = CryMessageBox( text.c_str(), "Debug Assertion Triggered", eMB_Error);
        switch(result){
        case IDRETRY: 
			return false;
        case IDIGNORE: 
			hashes[index] = hash;
			return false;
        case IDABORT: 
			return true;
        }
    }
    else{
        fwrite(text.c_str(), text.size(), 1, stderr);
		fflush(stderr);
		if(testMode)
			exit(-1);
    }
#else
	fprintf(stderr, "%s", text.c_str()); 
#endif
	return false; 
}

#endif

}
