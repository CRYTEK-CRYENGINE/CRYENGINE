// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2012.
// -------------------------------------------------------------------------
//  File name:   GeomCacheBlockCompressor.cpp
//  Created:     12/8/2012 by Axel Gneiting
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <zlib.h>
#include "GeomCacheBlockCompressor.h"

#include <lz4.h>
#include <lz4hc.h>

bool GeomCacheDeflateBlockCompressor::Compress(std::vector<char> &input, std::vector<char> &output)
{
	const size_t uncompressedSize = input.size();
	
	// Reserve buffer. zlib maximum overhead is 5 bytes per 32KB block + 6 bytes fixed. 	
	const size_t maxCompressedSize = uncompressedSize + ((uncompressedSize / 32768) + 1) * 5 + 6;
	output.resize(maxCompressedSize);

	unsigned long compressedSize = 0;

	// Deflate
	z_stream stream;
	int error;

	stream.next_in = (Bytef*)&input[0];
	stream.avail_in = (uInt)uncompressedSize;

	stream.next_out = (Bytef*)&output[0];
	stream.avail_out = (uInt)maxCompressedSize;

	stream.zalloc = Z_NULL;
	stream.zfree  = Z_NULL;
	stream.opaque = Z_NULL;

	error = deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 9, Z_DEFAULT_STRATEGY);
	if (error != Z_OK) 
	{
		return false;
	}

	error = deflate(&stream, Z_FINISH);
	if (error != Z_STREAM_END) 
	{
		deflateEnd(&stream);		
		return false;
	}

	compressedSize = stream.total_out;

	error = deflateEnd(&stream);
	if (error != Z_OK)
	{
		return false;
	}

	if (compressedSize == 0)
	{
		return false;
	}

	// Resize output to final, compressed size
	output.resize(compressedSize);

	return true;
}

bool GeomCacheLZ4HCBlockCompressor::Compress(std::vector<char> &input, std::vector<char> &output)
{
	const size_t uncompressedSize = input.size();	

	// Reserve compress buffer
	const size_t maxCompressedSize = LZ4_compressBound(uncompressedSize);
	output.resize(maxCompressedSize);

	// Compress
	int compressedSize = LZ4_compressHC(input.data(), output.data(), uncompressedSize);

	if (compressedSize == 0)
	{
		return false;
	}

	// Resize output to final, compressed size
	output.resize(compressedSize);

	return true;
}
