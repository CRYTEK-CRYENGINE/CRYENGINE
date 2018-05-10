// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ISAVEGAME_H__
#define __ISAVEGAME_H__

#pragma once

#include <CryNetwork/SerializeFwd.h>

struct ISaveGame
{
	virtual ~ISaveGame(){}
	// initialize - set output path
	virtual bool Init(const char* name) = 0;

	// set some basic meta-data
	virtual void       AddMetadata(const char* tag, const char* value) = 0;
	virtual void       AddMetadata(const char* tag, int value) = 0;
	// create a serializer for some data section
	virtual TSerialize AddSection(const char* section) = 0;
	// set a thumbnail.
	// if imageData == 0: only reserves memory and returns ptr to local data
	// if imageData != 0: copies data from imageData into local buffer
	// imageData is in BGR or BGRA
	// returns ptr to internal data storage (size=width*height*depth) if Thumbnail supported,
	// 0 otherwise
	virtual uint8* SetThumbnail(const uint8* imageData, int width, int height, int depth) = 0;

	// set a thumbnail from an already present bmp file
	// file will be read on function call
	// returns true if successful, false otherwise
	virtual bool SetThumbnailFromBMP(const char* filename) = 0;

	// finish - indicate success (negative success *must* remove file)
	// also calls delete this;
	virtual bool Complete(bool successfulSoFar) = 0;

	// returns the filename of this savegame
	virtual const char* GetFileName() const = 0;

	// save game reason
	virtual void            SetSaveGameReason(ESaveGameReason reason) = 0;
	virtual ESaveGameReason GetSaveGameReason() const = 0;

	virtual void            GetMemoryUsage(ICrySizer* pSizer) const = 0;
};

#endif
