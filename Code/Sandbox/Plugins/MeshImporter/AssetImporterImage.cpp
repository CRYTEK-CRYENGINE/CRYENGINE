// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetImporterImage.h"
#include "SandboxPlugin.h"
#include "TextureHelpers.h"
#include "ImporterUtil.h"
#include "RcCaller.h"

#include <AssetSystem/AssetImportContext.h>
#include <FilePathUtil.h>

#include <QFile>

namespace Private_AssetImporterImage
{

static bool MakeFileWritable(const string& path)
{
	QFile f(QtUtil::ToQString(path));
	return f.setPermissions(f.permissions() | QFileDevice::WriteOwner);
}

} // namespace Private_AssetImporterImage

REGISTER_ASSET_IMPORTER(CAssetImporterImage)

std::vector<string> CAssetImporterImage::GetFileExtensions() const
{
	// Some image formats supported by the ImageMagick version shipped with the engine. (convert -list format)
	// This is supposed to be a whitelist. If a format turns out to be problematic, we should remove it.
	// TODO: Make this a config file.
	return
	{
		"bmp", // Microsoft Windows bitmap image
		"bmp2", // Microsoft Windows bitmap image (V2)
		"bmp3", // Microsoft Windows bitmap image (V3)
		"icb", // Truevision Targa image
		"ico", // Microsoft icon
		"icon", // Microsoft icon
		"j2c", // JPEG-2000 Code Stream Syntax (2.1.0)
		"j2k", // JPEG-2000 Code Stream Syntax (2.1.0)
		"jbg", // Joint Bi-level Image experts Group interchange format
		"jbig", // Joint Bi-level Image experts Group interchange format
		"jng", // JPEG Network Graphics
		"jp2", // JPEG-2000 File Format Syntax (2.1.0)
		"jpc", // JPEG-2000 Code Stream Syntax (2.1.0)
		"jpe", // Joint Photographic Experts Group JFIF format (80)
		"jpeg", // Joint Photographic Experts Group JFIF format (80)
		"jpg", // Joint Photographic Experts Group JFIF format (80)
		"jpm", // JPEG-2000 Code Stream Syntax (2.1.0)
		"jps", // Joint Photographic Experts Group JFIF format (80)
		"jpt", // JPEG-2000 File Format Syntax (2.1.0)
		"pjpeg", // Joint Photographic Experts Group JFIF format (80)
		"png", // Portable Network Graphics (libpng 1.6.17)
		"png00", // PNG inheriting bit-depth, color-type from original if possible
		"png24", // opaque or binary transparent 24-bit RGB (zlib 1.2.8)
		"png32", // opaque or transparent 32-bit RGBA
		"png48", // opaque or binary transparent 48-bit RGB
		"png64", // opaque or transparent 64-bit RGBA
		"png8", // 8-bit indexed with optional binary transparency
		"ppm", // Portable pixmap format (color)
		"psd", // Adobe Photoshop bitmap
		"ptif", // Pyramid encoded TIFF
		"svg", // Scalable Vector Graphics (RSVG 2.40.10)
		"svgz", // Compressed Scalable Vector Graphics (RSVG 2.40.10)
		"tga", // Truevision Targa image
		"tiff", // Tagged Image File Format (LIBTIFF, Version 4.0.6)
		"tiff64", // Tagged Image File Format (64-bit) (LIBTIFF, Version 4.0.6)
		"tif", // Tagged Image File Format
	};
}

std::vector<string> CAssetImporterImage::GetAssetTypes() const
{
	return { "Texture" };
}

std::vector<string> CAssetImporterImage::GetAssetNames(const std::vector<string>&, CAssetImportContext& ctx)
{
	const string basename = PathUtil::GetFileName(ctx.GetInputFilePath());
	// TODO: Return names only.
	return { ctx.GetOutputFilePath(".dds.cryasset") };
}

std::vector<CAsset*> CAssetImporterImage::ImportAssets(const std::vector<string>& assetPaths, CAssetImportContext& ctx)
{
	using namespace Private_AssetImporterImage;

	if (assetPaths.empty())
	{
		return {};
	}

	CFileImporter fileImporter;

	// We have already got the user confirmation. see CAssetImporter::Import() 
	fileImporter.SetMayOverwriteFunc([](auto)
	{
		return true;
	});

	const string absOutputSourceFilePath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), ctx.GetOutputSourceFilePath());
	if (!fileImporter.Import(ctx.GetInputFilePath(), absOutputSourceFilePath))
	{
		return{};
	}

	// If the source file is a TIF, we make it writable, as we might call the RC and store settings
	// in it.
	if (!stricmp(PathUtil::GetExt(absOutputSourceFilePath), "tif"))
	{
		MakeFileWritable(absOutputSourceFilePath);
	}

	const string tifFilePath = TextureHelpers::CreateCryTif(absOutputSourceFilePath);
	if (!tifFilePath)
	{
		return {};
	}

	// Create DDS.
	CRcCaller rcCaller;
	rcCaller.SetAdditionalOptions(CRcCaller::OptionOverwriteFilename(ctx.GetAssetName()));
	if (!rcCaller.Call(tifFilePath))
	{
		return {};
	}

	CAsset* const pTextureAsset = ctx.LoadAsset(ctx.GetOutputFilePath("dds.cryasset"));
	if (pTextureAsset)
	{
		return { pTextureAsset };
	}
	else
	{
		return {};
	}
}

