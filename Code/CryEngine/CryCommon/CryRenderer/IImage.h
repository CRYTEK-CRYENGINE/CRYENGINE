// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef IIMAGE_H
	#define IIMAGE_H

//! Possible errors for IImageFile::mfGet_error.
enum EImFileError { eIFE_OK = 0, eIFE_IOerror, eIFE_OutOfMemory, eIFE_BadFormat, eIFE_ChunkNotFound };

	#define FIM_NORMALMAP        0x00001    //!< Image contains normal vectors.
	#define FIM_IMMEDIATE_RC     0x00002    //!< Resource Compiler should be invoked immediately if TIF needs compiling to DDS. Used for textures that the engine cannot start without, results in a blocking operation!
	#define FIM_NOTSUPPORTS_MIPS 0x00004
	#define FIM_ALPHA            0x00008    //!< Request attached alpha image instead of base image.
	#define FIM_DECAL            0x00010    //!< Use bordercolor instead of tiling for this image.
	#define FIM_GREYSCALE        0x00020    //!< Hint this texture is greyscale (could be DXT1 with colored artifacts).
//efine FIM_____________________ 0x00040
	#define FIM_STREAM_PREPARE   0x00080    //!< Image is streamable, load only persistant mips.
	#define FIM_FILESINGLE       0x00100    //!< Info from rc: no need to search for other files (e.g. DDNDIF).
//efine FIM_____________________ 0x00200
//efine FIM_____________________ 0x00400
	#define FIM_SPLITTED             0x00800 //!< Textures is stored in splitted files.
	#define FIM_SRGB_READ            0x01000 //!< Texture is stores in sRGB-space.
//efine FIM_____________________ 0x02000
	#define FIM_READ_VIA_STREAMS     0x04000 //!< REMOVE: issue file reads through the stream engine, for cases where reads may contend with the disc.
	#define FIM_RENORMALIZED_TEXTURE 0x08000 //!< Textures are stored with a dynamic range, the range is stored in the header.
	#define FIM_HAS_ATTACHED_ALPHA   0x10000 //!< Texture has an attached alpha companion texture.
	#define FIM_SUPPRESS_DOWNSCALING 0x20000 //!< Don't allow to drop mips when texture is non-streamable.
	#define FIM_DX10IO               0x40000 //!< The DDS containing the texture has an extended DX10+ header.

class IImageFile
{
public:
	virtual int           AddRef() = 0;
	virtual int           Release() = 0;

	virtual const string& mfGet_filename() const = 0;

	virtual int           mfGet_width() const = 0;
	virtual int           mfGet_height() const = 0;
	virtual int           mfGet_depth() const = 0;
	virtual int           mfGet_NumSides() const = 0;

	virtual EImFileError  mfGet_error() const = 0;

	virtual byte*         mfGet_image(const int nSide) = 0;
	virtual bool          mfIs_image(const int nSide) const = 0;

	virtual ETEX_Format   mfGetFormat() const = 0;
	virtual ETEX_TileMode mfGetTileMode() const = 0;
	virtual int           mfGet_numMips() const = 0;
	virtual int           mfGet_numPersistantMips() const = 0;
	virtual int           mfGet_Flags() const = 0;
	virtual const ColorF& mfGet_minColor() const = 0;
	virtual const ColorF& mfGet_maxColor() const = 0;
	virtual int           mfGet_ImageSize() const = 0;

protected:
	virtual ~IImageFile() {}
};

#endif
