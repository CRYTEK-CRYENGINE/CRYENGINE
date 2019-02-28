// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2012.
// -------------------------------------------------------------------------
//  File name:   GeomCacheBlockCompressor.h
//  Created:     12/8/2012 by Axel Gneiting
//
////////////////////////////////////////////////////////////////////////////

#pragma once

class IGeomCacheBlockCompressor
{
public:
	virtual bool Compress(std::vector<char> &input, std::vector<char> &output) = 0;
};

// This just passes the input to the output vector and does no compression
class GeomCacheStoreBlockCompressor : public IGeomCacheBlockCompressor
{
	virtual bool Compress(std::vector<char> &input, std::vector<char> &output) override
	{
		input.swap(output);
		return true;
	}
};

// This is the deflate compressor
class GeomCacheDeflateBlockCompressor : public IGeomCacheBlockCompressor
{
	virtual bool Compress(std::vector<char> &input, std::vector<char> &output) override;
};

// This is the LZ4 HC compressor
class GeomCacheLZ4HCBlockCompressor : public IGeomCacheBlockCompressor
{
	virtual bool Compress(std::vector<char> &input, std::vector<char> &output) override;
};