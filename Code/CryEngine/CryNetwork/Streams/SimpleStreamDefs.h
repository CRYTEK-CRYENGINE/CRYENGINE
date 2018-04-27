// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SIMPLESTREAMDEFS_H__
#define __SIMPLESTREAMDEFS_H__

#pragma once

struct SStreamRecord
{
	static const size_t KEY_SIZE = 32;
	static const size_t VALUE_SIZE = 96;

	SStreamRecord(const char* key, const char* value)
	{
		memset(this->key, 0, sizeof(this->key));
		memset(this->value, 0, sizeof(this->value));
		cry_strcpy(this->key, key);
		cry_strcpy(this->value, value);
	}

	SStreamRecord(const char* key)
	{
		memset(this->key, 0, sizeof(this->key));
		memset(this->value, 0, sizeof(this->value));
		cry_strcpy(this->key, key);
	}

	SStreamRecord()
	{
		memset(this->key, 0, sizeof(this->key));
		memset(this->value, 0, sizeof(this->value));
	}

	char key[KEY_SIZE];
	char value[VALUE_SIZE];
};

#endif
