/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <limits>
#include <math.h>
#include <float.h>
#include <CrySerialization/yasli/STL.h>
#include <CrySerialization/yasli/BlackBox.h>
#include <CrySerialization/yasli/Config.h>
#include "JSONIArchive.h"
#include "MemoryReader.h"
#include "MemoryWriter.h"

#ifdef _MSC_VER
#ifndef NAN
	static unsigned long g_nan[2] = {0xffffffff, 0x7fffffff};
	#define NAN (*(double*)g_nan)
#endif
#endif

#if 0
# define DEBUG_TRACE(fmt, ...) printf(fmt "\n", __VA_ARGS__)
# define DEBUG_TRACE_TOKENIZER(fmt, ...) printf(fmt "\n", __VA_ARGS__)
#else
# define DEBUG_TRACE(...)
# define DEBUG_TRACE_TOKENIZER(...)
#endif

namespace yasli{

const int MAX_BYTES_TO_ALLOC_ON_STACK = 16384;

inline bool isDigit(int ch) 
{
	return unsigned(ch - '0') <= 9;
}

inline double parseFloat(const char* s)
{
	double res = 0, f = 1, sign = 1;
	while(*s && (*s == ' ' || *s == '\t')) 
		s++;

	if(*s == '-') 
		sign=-1, s++; 
	else if (*s == '+') 
		s++;

	for(; isDigit(*s); s++)
		res = res * 10 + (*s - '0');

	if(*s == '.')
		for (s++; isDigit(*s); s++)
			res += (f *= 0.1)*(*s - '0');
	return res*sign;
}


static char hexValueTable[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,

    0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static int unescapeString(char* pHead, const char* begin, const char* end)
{
	if (begin >= end)
	{
		return 0;
	}

	char* pTail = pHead;
	while (begin != end) {
		if (*begin != '\\') {
			*pTail = *begin;
			++pTail;
		}
		else {
			++begin;
			if (begin == end)
				break;

			switch (*begin) {
			case '0':  *pTail = '\0'; ++pTail; break;
			case 't':  *pTail = '\t'; ++pTail; break;
			case 'n':  *pTail = '\n'; ++pTail; break;
			case 'r':  *pTail = '\r'; ++pTail; break;
			case '\\': *pTail = '\\'; ++pTail; break;
			case '\"': *pTail = '\"'; ++pTail; break;
			case '\'': *pTail = '\''; ++pTail; break;
			case 'x':
				if (begin + 2 < end) {
					*pTail = (hexValueTable[int(begin[1])] << 4) + hexValueTable[int(begin[2])];
					++pTail;
					begin += 2;
					break;
				}
			default:
				*pTail = *begin;
				++pTail;
				break;
			}
		}
		++begin;
	}
	return static_cast<int>(pTail - pHead);
}

struct SScopedStackStringHelper
{
	SScopedStackStringHelper(char* pString, int nByteSize, bool isStackString)
		: bIsStackString(isStackString)
		, pStr(pString)
	{
		memset(pStr, '\0', nByteSize);
	}

	~SScopedStackStringHelper()
	{
		if (!bIsStackString)
		{
			free(pStr);
		}
	}

	const bool bIsStackString;
	char* const  pStr;
};

// ---------------------------------------------------------------------------

class JSONTokenizer{
public:
    inline JSONTokenizer();

    inline Token operator()(const char* text) const;
private:
    inline static bool isSpace(char c);
    inline static bool isWordPart(unsigned char c);
    inline static bool isComment(char c);
    inline static bool isQuoteOpen(int& quoteIndex, char c);
    inline static bool isQuoteClose(int quoteIndex, char c);
    inline static bool isQuote(char c);
};

JSONTokenizer::JSONTokenizer()
{
}

inline bool JSONTokenizer::isSpace(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

inline bool JSONTokenizer::isComment(char c)
{
	return c == '#';
}

inline bool JSONTokenizer::isQuote(char c)
{
	return c == '\"';
}

static const char charTypes[256] = {
	0 /* 0x00: */,
	0 /* 0x01: */,
	0 /* 0x02: */,
	0 /* 0x03: */,
	0 /* 0x04: */,
	0 /* 0x05: */,
	0 /* 0x06: */,
	0 /* 0x07: */,
	0 /* 0x08: */,
	0 /* 0x09: \t */,
	0 /* 0x0A: \n */,
	0 /* 0x0B: */,
	0 /* 0x0C: */,
	0 /* 0x0D: */,
	0 /* 0x0E: */,
	0 /* 0x0F: */,


	0 /* 0x10: */,
	0 /* 0x11: */,
	0 /* 0x12: */,
	0 /* 0x13: */,
	0 /* 0x14: */,
	0 /* 0x15: */,
	0 /* 0x16: */,
	0 /* 0x17: */,
	0 /* 0x18: */,
	0 /* 0x19: */,
	0 /* 0x1A: */,
	0 /* 0x1B: */,
	0 /* 0x1C: */,
	0 /* 0x1D: */,
	0 /* 0x1E: */,
	0 /* 0x1F: */,


	0 /* 0x20:   */,
	0 /* 0x21: ! */,
	0 /* 0x22: " */,
	0 /* 0x23: # */,
	0 /* 0x24: $ */,
	0 /* 0x25: % */,
	0 /* 0x26: & */,
	0 /* 0x27: ' */,
	0 /* 0x28: ( */,
	0 /* 0x29: ) */,
	0 /* 0x2A: * */,
	0 /* 0x2B: + */,
	0 /* 0x2C: , */,
	1 /* 0x2D: - */,
	1 /* 0x2E: . */,
	0 /* 0x2F: / */,


	1 /* 0x30: 0 */,
	1 /* 0x31: 1 */,
	1 /* 0x32: 2 */,
	1 /* 0x33: 3 */,
	1 /* 0x34: 4 */,
	1 /* 0x35: 5 */,
	1 /* 0x36: 6 */,
	1 /* 0x37: 7 */,
	1 /* 0x38: 8 */,
	1 /* 0x39: 9 */,
	0 /* 0x3A: : */,
	0 /* 0x3B: ; */,
	0 /* 0x3C: < */,
	0 /* 0x3D: = */,
	0 /* 0x3E: > */,
	0 /* 0x3F: ? */,


	0 /* 0x40: @ */,
	1 /* 0x41: A */,
	1 /* 0x42: B */,
	1 /* 0x43: C */,
	1 /* 0x44: D */,
	1 /* 0x45: E */,
	1 /* 0x46: F */,
	1 /* 0x47: G */,
	1 /* 0x48: H */,
	1 /* 0x49: I */,
	1 /* 0x4A: J */,
	1 /* 0x4B: K */,
	1 /* 0x4C: L */,
	1 /* 0x4D: M */,
	1 /* 0x4E: N */,
	1 /* 0x4F: O */,


	1 /* 0x50: P */,
	1 /* 0x51: Q */,
	1 /* 0x52: R */,
	1 /* 0x53: S */,
	1 /* 0x54: T */,
	1 /* 0x55: U */,
	1 /* 0x56: V */,
	1 /* 0x57: W */,
	1 /* 0x58: X */,
	1 /* 0x59: Y */,
	1 /* 0x5A: Z */,
	0 /* 0x5B: [ */,
	0 /* 0x5C: \ */,
	0 /* 0x5D: ] */,
	0 /* 0x5E: ^ */,
	1 /* 0x5F: _ */,


	0 /* 0x60: ` */,
	1 /* 0x61: a */,
	1 /* 0x62: b */,
	1 /* 0x63: c */,
	1 /* 0x64: d */,
	1 /* 0x65: e */,
	1 /* 0x66: f */,
	1 /* 0x67: g */,
	1 /* 0x68: h */,
	1 /* 0x69: i */,
	1 /* 0x6A: j */,
	1 /* 0x6B: k */,
	1 /* 0x6C: l */,
	1 /* 0x6D: m */,
	1 /* 0x6E: n */,
	1 /* 0x6F: o */,


	1 /* 0x70: p */,
	1 /* 0x71: q */,
	1 /* 0x72: r */,
	1 /* 0x73: s */,
	1 /* 0x74: t */,
	1 /* 0x75: u */,
	1 /* 0x76: v */,
	1 /* 0x77: w */,
	1 /* 0x78: x */,
	1 /* 0x79: y */,
	1 /* 0x7A: z */,
	0 /* 0x7B: { */,
	0 /* 0x7C: | */,
	0 /* 0x7D: } */,
	0 /* 0x7E: ~ */,
	0 /* 0x7F: */,


	0 /* 0x80: */,
	0 /* 0x81: */,
	0 /* 0x82: */,
	0 /* 0x83: */,
	0 /* 0x84: */,
	0 /* 0x85: */,
	0 /* 0x86: */,
	0 /* 0x87: */,
	0 /* 0x88: */,
	0 /* 0x89: */,
	0 /* 0x8A: */,
	0 /* 0x8B: */,
	0 /* 0x8C: */,
	0 /* 0x8D: */,
	0 /* 0x8E: */,
	0 /* 0x8F: */,


	0 /* 0x90: */,
	0 /* 0x91: */,
	0 /* 0x92: */,
	0 /* 0x93: */,
	0 /* 0x94: */,
	0 /* 0x95: */,
	0 /* 0x96: */,
	0 /* 0x97: */,
	0 /* 0x98: */,
	0 /* 0x99: */,
	0 /* 0x9A: */,
	0 /* 0x9B: */,
	0 /* 0x9C: */,
	0 /* 0x9D: */,
	0 /* 0x9E: */,
	0 /* 0x9F: */,


	0 /* 0xA0: */,
	0 /* 0xA1: */,
	0 /* 0xA2: */,
	0 /* 0xA3: */,
	0 /* 0xA4: */,
	0 /* 0xA5: */,
	0 /* 0xA6: */,
	0 /* 0xA7: */,
	0 /* 0xA8: */,
	0 /* 0xA9: */,
	0 /* 0xAA: */,
	0 /* 0xAB: */,
	0 /* 0xAC: */,
	0 /* 0xAD: ­*/,
	0 /* 0xAE: */,
	0 /* 0xAF: */,


	0 /* 0xB0: */,
	0 /* 0xB1: */,
	0 /* 0xB2: */,
	0 /* 0xB3: */,
	0 /* 0xB4: */,
	0 /* 0xB5: */,
	0 /* 0xB6: */,
	0 /* 0xB7: */,
	0 /* 0xB8: */,
	0 /* 0xB9: */,
	0 /* 0xBA: */,
	0 /* 0xBB: */,
	0 /* 0xBC: */,
	0 /* 0xBD: */,
	0 /* 0xBE: */,
	0 /* 0xBF: */,


	0 /* 0xC0: */,
	0 /* 0xC1: */,
	0 /* 0xC2: */,
	0 /* 0xC3: */,
	0 /* 0xC4: */,
	0 /* 0xC5: */,
	0 /* 0xC6: */,
	0 /* 0xC7: */,
	0 /* 0xC8: */,
	0 /* 0xC9: */,
	0 /* 0xCA: */,
	0 /* 0xCB: */,
	0 /* 0xCC: */,
	0 /* 0xCD: */,
	0 /* 0xCE: */,
	0 /* 0xCF: */,


	0 /* 0xD0: */,
	0 /* 0xD1: */,
	0 /* 0xD2: */,
	0 /* 0xD3: */,
	0 /* 0xD4: */,
	0 /* 0xD5: */,
	0 /* 0xD6: */,
	0 /* 0xD7: */,
	0 /* 0xD8: */,
	0 /* 0xD9: */,
	0 /* 0xDA: */,
	0 /* 0xDB: */,
	0 /* 0xDC: */,
	0 /* 0xDD: ­*/,
	0 /* 0xDE: */,
	0 /* 0xDF: */,


	0 /* 0xE0: */,
	0 /* 0xE1: */,
	0 /* 0xE2: */,
	0 /* 0xE3: */,
	0 /* 0xE4: */,
	0 /* 0xE5: */,
	0 /* 0xE6: */,
	0 /* 0xE7: */,
	0 /* 0xE8: */,
	0 /* 0xE9: */,
	0 /* 0xEA: */,
	0 /* 0xEB: */,
	0 /* 0xEC: */,
	0 /* 0xED: */,
	0 /* 0xEE: */,
	0 /* 0xEF: */,


	0 /* 0xF0: */,
	0 /* 0xF1: */,
	0 /* 0xF2: */,
	0 /* 0xF3: */,
	0 /* 0xF4: */,
	0 /* 0xF5: */,
	0 /* 0xF6: */,
	0 /* 0xF7: */,
	0 /* 0xF8: */,
	0 /* 0xF9: */,
	0 /* 0xFA: */,
	0 /* 0xFB: */,
	0 /* 0xFC: */,
	0 /* 0xFD: */,
	0 /* 0xFE: */,
	0 /* 0xFF: */
};

inline bool JSONTokenizer::isWordPart(unsigned char c)
{
		return charTypes[c] != 0;
}

Token JSONTokenizer::operator()(const char* ptr) const
{
	while(isSpace(*ptr))
		++ptr;
	Token cur(ptr, ptr);
	while(!cur && *ptr != '\0'){
		while(isComment(*cur.end)){
			const char* commentStart = ptr;
			while(*cur.end && *cur.end != '\n')
				++cur.end;
			while(isSpace(*cur.end))
				++cur.end;
			DEBUG_TRACE_TOKENIZER("Got comment: '%s'", string(commentStart, cur.end).c_str());
			cur.start = cur.end;
		}
		YASLI_ASSERT(!isSpace(*cur.end));
		if(isQuote(*cur.end)){
			++cur.end;
			while(*cur.end){ 
				if(*cur.end == '\\'){
					++cur.end;
					if(*cur.end ){
						if(*cur.end != 'x' && *cur.end != 'X')
							++cur.end;
						else{
							++cur.end;
							if(*cur.end)
								++cur.end;
						}
					}
                    continue;
				}
				if(isQuote(*cur.end)){
					++cur.end;
					DEBUG_TRACE_TOKENIZER("Tokenizer result: '%s'", cur.str().c_str());
					return cur;
				}
				else
					++cur.end;
			}
		}
		else{
			//YASLI_ASSERT(*cur.end);
			if(!*cur.end)
				return cur;

			DEBUG_TRACE_TOKENIZER("%c", *cur.end);
			if(isWordPart(*cur.end))
			{
				do{
					++cur.end;
				} while(isWordPart(*cur.end) != 0);
			}
			else
			{
				++cur.end;
				return cur;
			}
			DEBUG_TRACE_TOKENIZER("Tokenizer result: '%s'", cur.str().c_str());
			return cur;
		}
	}
	DEBUG_TRACE_TOKENIZER("Tokenizer result: '%s'", cur.str().c_str());
	return cur;
}


// ---------------------------------------------------------------------------

JSONIArchive::JSONIArchive()
: Archive(INPUT | TEXT)
, buffer_(0)
{
}

JSONIArchive::~JSONIArchive()
{
	if (buffer_) {
		free(buffer_);
		buffer_ = 0;
	}
	stack_.clear();
	reader_.reset();
}

bool JSONIArchive::open(const char* buffer, size_t length, bool free)
{
	if(!length)
		return false;

	if(buffer)
		reader_.reset(new MemoryReader(buffer, length, free));
	buffer_ = 0;

	token_ = Token(reader_->begin(), reader_->begin());
	stack_.clear();

	stack_.push_back(Level());
	readToken();
	putToken();
	stack_.back().start = token_.end;
	return true;
}


bool JSONIArchive::load(const char* filename)
{
	if(FILE* file = fopen(filename, "rb"))
	{
		fseek(file, 0, SEEK_END);
		long fileSize = ftell(file);
		fseek(file, 0, SEEK_SET);

		void* buffer = 0;
		if(fileSize > 0){
			buffer = malloc(fileSize + 1);
			YASLI_ASSERT(buffer != 0);
			memset(buffer, 0, fileSize + 1);
			size_t elementsRead = fread(buffer, fileSize, 1, file);
			YASLI_ASSERT(((char*)(buffer))[fileSize] == '\0');
			if(elementsRead != 1){
				free(buffer);
				fclose(file);
				return false;
			}
		}
		fclose(file);

		filename_ = filename;
		buffer_ = buffer;
		if (fileSize > 0)
			return open((char*)buffer, fileSize, true);
		else
			return false; 
	}
	else{
		return false;
	}
}

void JSONIArchive::readToken()
{
	JSONTokenizer tokenizer;
	token_ = tokenizer(token_.end);
	DEBUG_TRACE(" ~ read token '%s' at %i", token_.str().c_str(), int(token_.start - reader_->begin()));
}

void JSONIArchive::putToken()
{
	DEBUG_TRACE(" putToken: '%s'", token_.str().c_str());
    token_ = Token(token_.start, token_.start);
}

int JSONIArchive::line(const char* position) const
{
	return int(std::count(reader_->begin(), position, '\n') + 1);
}

bool JSONIArchive::isName(Token token) const
{
	if(!token)
		return false;
	char firstChar = token.start[0];
	if (firstChar == '"')
		return true;
	return false;
}


bool JSONIArchive::expect(char token)
{
	if(token_ != token){
		const char* lineEnd = token_.start;
		while (lineEnd && *lineEnd != '\0' && *lineEnd != '\r' && *lineEnd != '\n')
			++lineEnd;

		MemoryWriter msg;
		msg << "Error parsing file, expected ':' at line " << line(token_.start) << ":\n"
		<< string(token_.start, lineEnd).c_str();
		YASLI_ASSERT_STR(0, msg.c_str());
		return false;
	}
	return true;
}

void JSONIArchive::skipBlock()
{
	DEBUG_TRACE("Skipping block from %i ...", int(token_.end - reader_->begin()));
    if(openBracket() || openContainerBracket())
        closeBracket(); // Skipping entire block
    else
        readToken(); // Skipping value
	readToken();
	if (token_ != ',')
		putToken();
	DEBUG_TRACE(" ...till %i", int(token_.end - reader_->begin()));
}

bool JSONIArchive::findName(const char* name, Token* outName)
{
	DEBUG_TRACE(" * finding name '%s'", name);
	DEBUG_TRACE("   started at byte %i", int(token_.start - reader_->begin()));
	if(stack_.empty()) {
		// TODO: diagnose
		return false;
	}
	if (stack_.back().isKeyValue)
		return true;
	const char* start = 0;
	const char* blockBegin = stack_.back().start;
	if(*blockBegin == '\0')
		return false;

	readToken();
	if (token_ == ',')
		readToken();
	if(!token_){
		start = blockBegin;
		token_.set(blockBegin, blockBegin);
		readToken();
	}

	if(stack_.size() == 1 || stack_.back().isContainer || outName != 0){
		if(token_ == ']' || token_ == '}'){
			DEBUG_TRACE("Got close bracket...");
			putToken();
			return false;
		}
		else{
			DEBUG_TRACE("Got unnamed value: '%s'", token_.str().c_str());
			putToken();
			return true;
		}
	}
	else{
		if(isName(token_)){
			DEBUG_TRACE("Seems to be a name '%s'", token_.str().c_str());
			Token nameContent(token_.start+1, token_.end-1);
			if(nameContent == name){
				readToken();
				expect(':');
				DEBUG_TRACE("Got one");
				return true;
			}
			else{
				start = token_.start;

				readToken();
				expect(':');
				skipBlock();
			}
		}
		else{
			start = token_.start;
			if(token_ == ']' || token_ == '}')
				token_ = Token(blockBegin, blockBegin);
			else{
				putToken();
				skipBlock();
			}
		}
	}

	while(true){
		readToken();
		if(!token_){
			token_.set(blockBegin, blockBegin);
			continue;
		}
		//return false; // Reached end of file while searching for name
		DEBUG_TRACE("'%s'", token_.str().c_str());
		DEBUG_TRACE("Checking for loop: %i and %i", token_.start - reader_->begin(), start - reader_->begin());
		YASLI_ASSERT(start != 0);
		if(token_.start == start){
			putToken();
			DEBUG_TRACE("unable to find...");
			return false; // Reached a full circle: unable to find name
		}

		if(token_ == '}' || token_ == ']'){ // CONVERSION
			DEBUG_TRACE("Going to begin of block, from %i", token_.start - reader_->begin());
			token_ = Token(blockBegin, blockBegin);
			DEBUG_TRACE(" to %i", token_.start - reader_->begin());
			continue; // Reached '}' or ']' while searching for name, continue from begin of block
		}

		if(name[0] == '\0'){
			if(isName(token_)){
				readToken();
				if(!token_)
					return false; // Reached end of file while searching for name
				expect(':');
				skipBlock();
			}
			else{
				putToken(); // Not a name - put it back
				return true;
			}
		}
		else{
			if(isName(token_)){
				Token nameContent(token_.start+1, token_.end-1);
				readToken();
				expect(':');
				if(nameContent == name)
					return true;
				else
					skipBlock();
			}
			else{
				putToken();
				skipBlock();
			}
		}

	}

	return false;
}

bool JSONIArchive::openBracket()
{
	readToken();
	if (token_ == '{')
		return true;
	putToken();
    return false;
}

bool JSONIArchive::closeBracket()
{
    int relativeLevel = 0;
    while(true){
        readToken();
		if (token_ == ',')
			readToken();
        if(!token_){
            MemoryWriter msg;
            YASLI_ASSERT(!stack_.empty());
            const char* start = stack_.back().start;
            msg << filename_.c_str() << ": " << line(start) << " line";
            msg << ": End of file while no matching bracket found";
			YASLI_ASSERT_STR(0, msg.c_str());
			return false;
        }
		else if(token_ == '}' || token_ == ']'){ // CONVERSION
            if(relativeLevel == 0)
				return true;
            else
                --relativeLevel;
        }
		else if(token_ == '{' || token_ == '['){ // CONVERSION
            ++relativeLevel;
        }
    }
    return false;
}

bool JSONIArchive::openContainerBracket()
{
	readToken();
	if(token_ == '[')
		return true;
	putToken();
	return false;
}

bool JSONIArchive::closeContainerBracket()
{
    readToken();
    if(token_ == ']'){
		DEBUG_TRACE("closeContainerBracket(): ok");
        return true;
    }
    else{
		DEBUG_TRACE("closeContainerBracket(): failed ('%s')", token_.str().c_str());
        putToken();
        return false;
    }
}

bool JSONIArchive::operator()(const Serializer& ser, const char* name, const char* label)
{
    if(findName(name)){
        if(openBracket()){
            stack_.push_back(Level());
            stack_.back().start = token_.end;
		}
		else if (openContainerBracket()) {
            stack_.push_back(Level());
            stack_.back().start = token_.end;
            stack_.back().isContainer = true;
		}
		else 
			return false;

        if (ser)
            ser(*this);
        YASLI_ASSERT(!stack_.empty());
        stack_.pop_back();
        bool closed = closeBracket();
        YASLI_ASSERT(closed);
        return true;
    }
    return false;
}

bool JSONIArchive::operator()(BlackBox& box, const char* name, const char* label)
{
	if(findName(name)){
		if(openBracket() || openContainerBracket()){
			const char* start = token_.start;
			putToken();
			skipBlock();
			const char* end = token_.start;
			if (end < start) {
				YASLI_ASSERT(0);
				return false;
			}
			while (end > start && 
				(*(end - 1) == ' '
				|| *(end - 1) == '\r'
				|| *(end - 1) == '\n'
				|| *(end - 1) == '\t'))
				--end;
			// box has to be const in the interface so we can serialize
			// temporary variables (i.e. function call result or structures
			// constructed on the stack)
			box.set("json", (void*)start, end - start);
			return true;
		}
	}
    return false;
}

bool JSONIArchive::operator()(KeyValueInterface& keyValue, const char* name, const char* label)
{
	Token nextName;
	if (!stack_.empty() && stack_.back().isContainer) {
		readToken();
		if (isName(token_) && checkStringValueToken()) {

			const int nMaxBytesRequired = std::max(1, (int)(token_.end - token_.start) - 1) * sizeof(char);			
			const bool bIsStackString = nMaxBytesRequired < MAX_BYTES_TO_ALLOC_ON_STACK;
			char* pKeyTmpBuf = bIsStackString ? (char*)alloca(nMaxBytesRequired) : (char*)malloc(nMaxBytesRequired);
			SScopedStackStringHelper strTemp(pKeyTmpBuf, nMaxBytesRequired, bIsStackString);

			unescapeString(pKeyTmpBuf, token_.start + 1, token_.end - 1);
			keyValue.set(pKeyTmpBuf);	

			readToken();
			if (!expect(':'))
				return false;
			if(!keyValue.serializeValue(*this, "", 0))
				return false;
			return true;
		}
		else {
			putToken();
			return false;
		}
	}
	else if (findName("", &nextName)) {

		const int nMaxBytesRequired = std::max(1, (int)(token_.end - token_.start) - 1) * sizeof(char);
		const bool bIsStackString = nMaxBytesRequired < MAX_BYTES_TO_ALLOC_ON_STACK;
		char* pKeyTmpBuf = bIsStackString ? (char*)alloca(nMaxBytesRequired) : (char*)malloc(nMaxBytesRequired);
		SScopedStackStringHelper strTemp(pKeyTmpBuf, nMaxBytesRequired, bIsStackString);

		unescapeString(pKeyTmpBuf, nextName.start + 1, nextName.end - 1);
		keyValue.set(pKeyTmpBuf);

		stack_.push_back(Level());
		stack_.back().isKeyValue = true;

		bool result = keyValue.serializeValue(*this, "", 0);
		if (stack_.empty()) {
			// TODO: diagnose
			return false;
		}
		stack_.pop_back();
		return result;
	}
	return false;
}


bool JSONIArchive::operator()(PointerInterface& ser, const char* name, const char* label)
{
	if (findName(name)) {
		if(openBracket()){
			stack_.push_back(Level());
			stack_.back().start = token_.end;
			stack_.back().isKeyValue = true;

			readToken();
			if (isName(token_)) {
				if(checkStringValueToken()){

					const int nMaxBytesRequired = std::max(1, (int)(token_.end - token_.start) - 1) * sizeof(char);
					const bool bIsStackString = nMaxBytesRequired < MAX_BYTES_TO_ALLOC_ON_STACK;
					char* pTypeNameTmpBuf = bIsStackString ? (char*)alloca(nMaxBytesRequired) : (char*)malloc(nMaxBytesRequired);
					SScopedStackStringHelper strTemp(pTypeNameTmpBuf, nMaxBytesRequired, bIsStackString);

					unescapeString(pTypeNameTmpBuf, token_.start + 1, token_.end - 1);

					if (strcmp(pTypeNameTmpBuf, ser.registeredTypeName()) != 0)
					{
						ser.create(pTypeNameTmpBuf);
					}

					readToken();
					expect(':');
					operator()(ser.serializer(), "", 0);
				}
			}
			else {
				putToken();

				ser.create("");				
			}
			closeBracket();
			stack_.pop_back();
			return true;
		}
	}
	return false;
}


bool JSONIArchive::operator()(ContainerInterface& ser, const char* name, const char* label)
{
    if(findName(name)){
		bool containerBracket = openContainerBracket();
		bool dictionaryBracket = false;
		if (!containerBracket)
			dictionaryBracket = openBracket();
		if(containerBracket || dictionaryBracket){
			stack_.push_back(Level());
			stack_.back().isContainer = true;
			stack_.back().start = token_.end;

			std::size_t size = ser.size();
			std::size_t index = 0;

			while(true){
				readToken();
				if (token_ == ',')
					readToken();
				if(token_ == '}')
					break;
				if(token_ == ']')
					break;
				else if(!token_)
				{
					YASLI_ASSERT(0 && "Reached end of file while reading container!");
					return false;
				}
				putToken();
				if(index == size)
					size = index + 1;
				if(index < size){
					if (!ser(*this, "", "")) {
						// We've got a named item within a container, 
						// i.e. looks like a dictionary but not a container.
						// Bail out, it is nothing we can do here.
						closeBracket();
						break;
					}
				}
				else{
					skipBlock();
				}
				ser.next();
				++index;
			}
			if(size > index)
				ser.resize(index);

			YASLI_ASSERT(!stack_.empty());
			stack_.pop_back();
			return true;
		}
    }
    return false;
}

void JSONIArchive::checkValueToken()
{
    if(!token_){
        YASLI_ASSERT(!stack_.empty());
        MemoryWriter msg;
        const char* start = stack_.back().start;
        msg << filename_.c_str() << ": " << line(start) << " line";
        msg << ": End of file while reading element's value";
        YASLI_ASSERT_STR(0, msg.c_str());
    }
}

bool JSONIArchive::checkStringValueToken()
{
    if(!token_){
        return false;
        MemoryWriter msg;
        const char* start = stack_.back().start;
        msg << filename_.c_str() << ": " << line(start) << " line";
        msg << ": End of file while reading element's value";
        YASLI_ASSERT_STR(0, msg.c_str());
		return false;
    }
    if(token_.start[0] != '"' || token_.end[-1] != '"'){
        return false;
        MemoryWriter msg;
        const char* start = stack_.back().start;
        msg << filename_.c_str() << ": " << line(start) << " line";
        msg << ": Expected string";
        YASLI_ASSERT_STR(0, msg.c_str());
		return false;
    }
    return true;
}

bool JSONIArchive::operator()(i32& value, const char* name, const char* label)
{
    if(findName(name)){
        readToken();
        checkValueToken();
        value = strtol(token_.start, 0, 10);
        return true;
    }
    return false;
}

bool JSONIArchive::operator()(u32& value, const char* name, const char* label)
{
    if(findName(name)){
        readToken();
        checkValueToken();
        value = strtoul(token_.start, 0, 10);
        return true;
    }
    return false;
}


bool JSONIArchive::operator()(i16& value, const char* name, const char* label)
{
    if(findName(name)){
        readToken();
        checkValueToken();
        value = (short)strtol(token_.start, 0, 10);
        return true;
    }
    return false;
}

bool JSONIArchive::operator()(u16& value, const char* name, const char* label)
{
    if(findName(name)){
        readToken();
        checkValueToken();
        value = (unsigned short)strtoul(token_.start, 0, 10);
        return true;
    }
    return false;
}

bool JSONIArchive::operator()(i64& value, const char* name, const char* label)
{
    if(findName(name)){
        readToken();
        checkValueToken();
#ifdef _MSC_VER
		value = _strtoi64(token_.start, 0, 10);
#else
		value = strtoll(token_.start, 0, 10);
#endif
        return true;
    }
    return false;
}

bool JSONIArchive::operator()(u64& value, const char* name, const char* label)
{
    if(findName(name)){
        readToken();
        checkValueToken();
#ifdef _MSC_VER
		value = _strtoui64(token_.start, 0, 10);
#else
		value = strtoull(token_.start, 0, 10);
#endif
        return true;
    }
    return false;
}

bool JSONIArchive::operator()(float& value, const char* name, const char* label)
{
    if(findName(name)){
        readToken();
        checkValueToken();
		if (*token_.start != '\"') {
#ifdef _MSC_VER
			value = (float)std::atof(token_.start);
#else
			value = strtof(token_.start, 0);
#endif
		}
		else if (strncmp(token_.start, "\"Infinity\"", 10) == 0)
			value = std::numeric_limits<float>::infinity();
		else if (strncmp(token_.start, "\"-Infinity\"", 11) == 0)
			value = -std::numeric_limits<float>::infinity();
		else if (strncmp(token_.start, "\"NaN\"", 5) == 0)
			value = (float)NAN;
		else
			return false;
		return true;
    }
    return false;
}

bool JSONIArchive::operator()(double& value, const char* name, const char* label)
{
    if(findName(name)){
        readToken();
        checkValueToken();
		if (*token_.start != '\"') {
#ifdef _MSC_VER
			value = std::atof(token_.start);
#else
			value = strtod(token_.start, 0);
#endif
		}
		else if (strncmp(token_.start, "\"Infinity\"", 10) == 0)
			value = std::numeric_limits<double>::infinity();
		else if (strncmp(token_.start, "\"-Infinity\"", 11) == 0)
			value = -std::numeric_limits<double>::infinity();
		else if (strncmp(token_.start, "\"NaN\"", 5) == 0)
			value = NAN;
		else
			return false;
		return true;
    }
    return false;
}

bool JSONIArchive::operator()(StringInterface& value, const char* name, const char* label)
{
    if(findName(name)){
        readToken();
        if(checkStringValueToken()){

			const int nMaxBytesRequired = std::max(1, (int)(token_.end - token_.start) - 1) * sizeof(char);
			const bool bIsStackString = nMaxBytesRequired < MAX_BYTES_TO_ALLOC_ON_STACK;
			char* pTmpStr = bIsStackString ? (char*)alloca(nMaxBytesRequired) : (char*)malloc(nMaxBytesRequired);
			SScopedStackStringHelper strTemp(pTmpStr, nMaxBytesRequired, bIsStackString);

			unescapeString(pTmpStr, token_.start + 1, token_.end - 1);
			value.set(pTmpStr);			
		}
		else
			return false;
        return true;
    }
    return false;
}


inline size_t utf8InUtf16Len(const char* p)
{
  size_t result = 0;

  for(; *p; ++p)
  {
    unsigned char ch = (unsigned char)(*p);

    if(ch < 0x80 || (ch >= 0xC0 && ch < 0xFC))
      ++result;
  }

  return result;
}

inline const char* readUtf16FromUtf8(unsigned int* ch, const char* s)
{
  const unsigned char byteMark = 0x80;
  const unsigned char byteMaskRead = 0x3F;

  const unsigned char* str = (const unsigned char*)s;

  size_t len;
  if(*str < byteMark)
  {
    *ch = *str;
    return s + 1;
  }
  else if(*str < 0xC0)
  {
    *ch = ' ';
    return s + 1;
  }
  else if(*str < 0xE0)
    len = 2;
  else if(*str < 0xF0)
    len = 3;
  else if(*str < 0xF8)
    len = 4;
  else if(*str < 0xFC)
    len = 5;
  else{
    *ch = ' ';
    return s + 1;
  }

  const unsigned char firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
  *ch = (*str++ & ~firstByteMark[len]);

  switch(len) 
  {
  case 5:
    (*ch) <<= 6;
    (*ch) += (*str++ & byteMaskRead);
  case 4:
    (*ch) <<= 6;
    (*ch) += (*str++ & byteMaskRead);
  case 3:
    (*ch) <<= 6;
    (*ch) += (*str++ & byteMaskRead);
  case 2:
    (*ch) <<= 6;
    (*ch) += (*str++ & byteMaskRead);
  }
  
  return (const char*)str;
}


inline void utf8ToUtf16(wstring* out, const char* in)
{
  out->clear();
  out->reserve(utf8InUtf16Len(in));

  for (; *in;)
  {
    unsigned int character;
    in = readUtf16FromUtf8(&character, in);
    (*out) += (wchar_t)character;
  }
}


bool JSONIArchive::operator()(WStringInterface& value, const char* name, const char* label)
{
	if(findName(name)){
		readToken();
		if(checkStringValueToken()){

			const int nMaxBytesRequired = std::max(1, (int)(token_.end - token_.start) - 1) * sizeof(char);
			const bool bIsStackString = nMaxBytesRequired < MAX_BYTES_TO_ALLOC_ON_STACK;
			char* pTmpStr = bIsStackString ? (char*)alloca(nMaxBytesRequired) : (char*)malloc(nMaxBytesRequired);
			SScopedStackStringHelper strTemp(pTmpStr, nMaxBytesRequired, bIsStackString);

			unescapeString(pTmpStr, token_.start + 1, token_.end - 1);

			wstring wbuf;
			utf8ToUtf16(&wbuf, pTmpStr);
			value.set(wbuf.c_str());
		}
		else
			return false;
		return true;
	}
	return false;

}

bool JSONIArchive::operator()(bool& value, const char* name, const char* label)
{
    if(findName(name)){
        readToken();
        checkValueToken();
        if(token_ == "true")
            value = true;
        else if(token_ == "false")
            value = false;
		else 
			YASLI_ASSERT(0 && "Invalid boolean value");
        return true;
    }
    return false;
}

bool JSONIArchive::operator()(i8& value, const char* name, const char* label)
{
    if(findName(name)){
        readToken();
        checkValueToken();
        value = (i8)strtol(token_.start, 0, 10);
        return true;
    }
    return false;
}

bool JSONIArchive::operator()(u8& value, const char* name, const char* label)
{
    if(findName(name)){
        readToken();
        checkValueToken();
        value = (u8)strtol(token_.start, 0, 10);
        return true;
    }
    return false;
}

bool JSONIArchive::operator()(char& value, const char* name, const char* label)
{
    if(findName(name)){
        readToken();
        checkValueToken();
        value = (char)strtol(token_.start, 0, 10);
        return true;
    }
    return false;
}

}
// vim:ts=4 sw=4:
