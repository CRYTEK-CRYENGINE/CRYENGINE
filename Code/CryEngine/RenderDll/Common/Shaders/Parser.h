// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   Parser.h : Script parser declarations.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#ifndef PARSER_H
#define PARSER_H

struct STokenDesc
{
	int   id;
	const char* token;
};
int shGetObject(char** buf, STokenDesc* tokens, char** name, char** data);

extern const char* kWhiteSpace;
extern FXMacro sStaticMacros;

bool   SkipChar(unsigned int ch);

void   fxParserInit(void);

void   SkipCharacters(char** buf, const char* toSkip);
void   RemoveCR(char* buf);
void   SkipComments(char** buf, bool bSkipWhiteSpace);

bool   fxIsFirstPass(char* buf);
void   fxRegisterEnv(const char* szStr);

int    shFill(char** buf, char* dst, int nSize = -1);
int    fxFill(char** buf, char* dst, int nSize = -1);
char*  fxFillPr(char** buf, char* dst);
char*  fxFillPrC(char** buf, char* dst);
char*  fxFillNumber(char** buf, char* dst);
int    fxFillCR(char** buf, char* dst);

bool   shGetBool(char* buf);
float  shGetFloat(const char* buf);
void   shGetFloat(const char* buf, float* v1, float* v2);
int    shGetInt(const char* buf);
int    shGetHex(const char* buf);
uint64 shGetHex64(const char* buf);
void   shGetVector(char* buf, Vec3& v);
void   shGetVector(char* buf, float v[3]);
void   shGetVector4(char* buf, vec4_t& v);
void   shGetColor(const char* buf, ColorF& v);
void   shGetColor(char* buf, float v[4]);
int    shGetVar(char** buf, char** vr, char** val);

#endif
