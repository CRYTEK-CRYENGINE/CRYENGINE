// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// xml save game - primarily for debug purposes

#ifndef __XMLSAVEGAME_H__
#define __XMLSAVEGAME_H__

#pragma once

#include "ISaveGame.h"

class CXmlSaveGame : public ISaveGame
{
public:
	CXmlSaveGame();
	virtual ~CXmlSaveGame();

	// ISaveGame
	virtual bool            Init(const char* name);
	virtual void            AddMetadata(const char* tag, const char* value);
	virtual void            AddMetadata(const char* tag, int value);
	virtual uint8*          SetThumbnail(const uint8* imageData, int width, int height, int depth);
	virtual bool            SetThumbnailFromBMP(const char* filename);
	virtual TSerialize      AddSection(const char* section);
	virtual bool            Complete(bool successfulSoFar);
	virtual const char*     GetFileName() const;
	virtual void            SetSaveGameReason(ESaveGameReason reason) { m_eReason = reason; }
	virtual ESaveGameReason GetSaveGameReason() const                 { return m_eReason; }
	virtual void            GetMemoryUsage(ICrySizer* pSizer) const;
	// ~ISaveGame

protected:
	virtual bool Write(const char* filename, XmlNodeRef data);
	XmlNodeRef   GetMetadataXmlNode() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m_pImpl;
	ESaveGameReason       m_eReason;

};

#endif
