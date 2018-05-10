// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ResourceCompilerImage.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"

#include <CryCore/Assert/CryAssert_impl.h>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

#include <assert.h>
#include "ResourceCompilerImage.h"
#include "IResCompiler.h"
#include "ImageConverter.h"             // CImageConverter
#include "Streaming/TextureSplitter.h"  // CTextureSplitter
#include "PixelFormats.h"               // CPixelFormats
#include "ImageProperties.h"            // EImageCompressor

// Must be included only once in DLL module.
#include <platform_implRC.inl>


#include "IResCompiler.h"


extern "C" IMAGE_DOS_HEADER __ImageBase;
HINSTANCE g_hInst = (HINSTANCE)&__ImageBase;

void __stdcall RegisterConverters(IResourceCompiler* pRC)
{
	SetRCLog(pRC->GetIRCLog());

	// image formats
	{
		pRC->RegisterConverter("ImageConverter", new CImageConverter(pRC));

		pRC->RegisterKey("streaming","[TIF] Split final output files for streaming");
		pRC->RegisterKey("userdialog","[TIF] 0/1 to show the dialog for the ResourceCompilerImage");
		pRC->RegisterKey("analyze","[TIF] 0/1 to print statistics about the generated output file");
		pRC->RegisterKey("preview","[SRF/TIF] 0/1 to enable preview in the dialog of ResourceCompilerImage, 1 is default");
		pRC->RegisterKey("preset","[TIF] e.g. \"Normals\" to override preset used during processing");
		pRC->RegisterKey("savepreset","[TIF] e.g. \"Diffuse_HighQ\" to save preset into source TIF file");
		pRC->RegisterKey("cleansettings","[TIF] remove obsolete/unneeded items from CryTIF settings stored in the TIF file(s)");
		pRC->RegisterKey("showsettings","[TIF] print CryTIF settings stored in the TIF file(s)");
		pRC->RegisterKey("savesettings","[TIF] replace CryTIF settings stored in the TIF file(s) by specified string");
		pRC->RegisterKey("maxtexturesize",
			"[TIF] upper limit of the resolution of generated textures.\n"
			"      0 - no upper resolution limit (default);\n"
			"      n - resulting texture will be downscaled (2x, 4x, ...) if its\n"
			"          width and/or height are larger than the specified resolution.\n"
			"          n should be a power-of-2 number larger than 1, for\n"
			"          example: 256, 512, 2014, 2048, 4096.");
		pRC->RegisterKey("mintexturesize",
			"[TIF] lower limit of the resolution of generated textures.\n"
			"      0 - no lower resolution limit (default);\n"
			"      n - resulting texture will be upscaled (2x, 4x, ...) if its\n"
			"          width and/or height are smaller than the specified resolution.\n"
			"          n should be a power-of-2 number larger than 1, for\n"
			"          example: 256, 512, 2014, 2048, 4096.");
		pRC->RegisterKey("reduce","[TIF] 0=no /1=half resolution /2=quarter resolution, etc");
		pRC->RegisterKey("globalreduce","[TIF] [0..[ to remove the top mipmap levels");
		pRC->RegisterKey("reducealpha","[TIF] [0..[ to remove the top mipmap levels from attached alpha");
		pRC->RegisterKey("mipmaps","[TIF] 0/1");
		pRC->RegisterKey("mipnormalize","[TIF] 0/1=use for normalmaps");
		pRC->RegisterKey("pixelformat","[TIF] e.g. 3DC, DXT1, X8R8G8B8");
		pRC->RegisterKey("previewformat","[TIF] format for the preview panel to be displayed in. If 'previewformat' doesn't exist in the preset, then 'pixelformat' will be used");
		pRC->RegisterKey("pixelformatalpha","[TIF] pixel format to be used for attached alpha");
		pRC->RegisterKey("swizzle",
			"[TIF] swizzle source image channels before further processing.\n"
			"The value is 4-letter string, possible letters are:\n"
			"   r - input red channel,\n"
			"   g - input green channel,\n"
			"   b - input blue channel,\n"
			"   a - input alpha channel,\n"
			"   1 - 1.0,\n"
			"   0 - 0.0\n"
			"Examples:\n"
			"  'rr0g': produces image with red channel filled with the source red\n"
			"      channel, green channel filled with the source red channel, blue\n"
			"      channel filled with zeros, alpha channel filled with the source\n"
			"      green channel.\n"
			"  'rgba': keeps the image unchanged.");
		pRC->RegisterKey("bumpblur","[TIF]");
		pRC->RegisterKey("bumptype","[TIF]");
		pRC->RegisterKey("bumpstrength","[TIF]");
		pRC->RegisterKey("bumpname","[TIF] name of optional bumpmap image (.tif) used in computing normals from heights");
		pRC->RegisterKey("decal","[TIF] 0/1=use special border clamp behavior on alpha test, 0(default)");
		pRC->RegisterKey("lumintoalpha","[TIF] 1=compute luminance by using RGB values and put it into alpha channel");
		pRC->RegisterKey("imagecompressor",
			"[TIF] CTsquish     - internal compressor (default)\n"
			"      CTsquishFast - internal compressor, fast mode with less quality\n"
			"      CTsquishHiQ  - internal compressor, slow mode with superb quality");
		pRC->RegisterKey("rgbweights",
			"[TIF] specify preset for weighting of R,G,B channels (used by compressor).\n"
			"      uniform   - uniform weights (1.0, 1.0, 1.0) (default),\n"
			"      luminance - luminance-based weights (0.3086, 0.6094, 0.0820),\n"
			"      ciexyz    - ciexyz-based weights (0.2126, 0.7152, 0.0722)");
		pRC->RegisterKey("dynscale","[TIF] 1=normalize the range of the color values of the image and store the original color range in the dds header");
		pRC->RegisterKey("dynscalealpha","[TIF] 1=normalize the range of the alpha values of image and store the original alpha range in the dds header");
		pRC->RegisterKey("outputuncompressed","[TIF] output uncompressed files instead of DXTx compressed files");
		pRC->RegisterKey("globalmipblur","[TIF] default blur is 2 (->4x4 kernel)");       // obsolete parameter, is positive mipgenblur now
		pRC->RegisterKey("mc","[TIF] maintain alpha coverage in mipmaps (experimental)"); // deprecated name
		pRC->RegisterKey("ms","[TIF] 0=no sharpening .. 100=full sharpening");            // obsolete parameter, is negative mipgenblur now
		{
			string m = "[TIF] specify mipmap generation filter, one of:\n      ";
			for (int i = 0; i < CImageProperties::GetMipGenerationMethodCount(); ++i)
			{
				const char* strGen = CImageProperties::GetMipGenerationMethodName(i);
				if (strGen && strGen[0])
				{
					m += StringHelpers::Format("%s", strGen);
					if (i == CImageProperties::GetMipGenerationMethodDefault())
					{
						m += " (default)";
					}
					m += (i + 1 == CImageProperties::GetMipGenerationMethodCount()) ? "\n" : ", ";
				}
			}

			string o = "[TIF] specify mipmap generation result, one of:\n      ";
			for (int i = 0; i < CImageProperties::GetMipGenerationEvalCount(); ++i)
			{
				const char* strGen = CImageProperties::GetMipGenerationEvalName(i);
				if (strGen && strGen[0])
				{
					o += StringHelpers::Format("%s", strGen);
					if (i == CImageProperties::GetMipGenerationEvalDefault())
					{
						o += " (default)";
					}
					o += (i + 1 == CImageProperties::GetMipGenerationEvalCount()) ? "\n" : ", ";
				}
			}

			pRC->RegisterKey("mipgentype", m.c_str());
			pRC->RegisterKey("mipgeneval", o.c_str());
			pRC->RegisterKey("mipgenblur", "[TIF] specify mip generation filter amplifier:\n"
				"      0.0 (default) - do not amplify,\n"
				"      1.0 - amplify by 100%\n"
				"      1.2 - amplify by 120%\n"
				"      2.0 - amplify by 200%\n"
				"      -1.0f - decrease by 50%\n"
				"      -2.0f - decrease by 66%\n"
				"      -3.0f - decrease by 75%\n"
				"      etc.");

			pRC->RegisterKey("mipgenvblur", "[TIF] like 'mipgenblur', vertical axis only");
			pRC->RegisterKey("mipgenhblur", "[TIF] like 'mipgenblur', horizontal axis only");
		}
		pRC->RegisterKey("brdfglossscale","[TIF] Scale factor used for computing specular power");
		pRC->RegisterKey("brdfglossbias","[TIF] Bias used for computing specular power");
		pRC->RegisterKey("glosslegacydist","[TIF] assumes gloss map uses legacy distribution from Ryse and converts to squared distribution");
		pRC->RegisterKey("glossfromnormals","[TIF] bake normal variance into smoothness stored in alpha channel");
		pRC->RegisterKey("ser","[TIF] 0/1=supress reduce resolution during loading, 0(default)");
		pRC->RegisterKey("mipbordercolor",
			"[TIF] if set, all mip borders during the mipmap\n"
			"      generation become this hex value AARRGGBB");
		pRC->RegisterKey("colorspace",
			"[TIF] specifies color spaces for input and output images, sets SRGBLookup\n"
			"      flag in the header of the output DDS files.\n"
			"      Format: colorspace=<input>,<output>\n"
			"      where <input> is one of: linear, sRGB;\n"
			"      <output> is one of: linear, sRGB, auto.\n"
			"      'auto' selects best encoding (linear or sRGB) automatically,\n"
			"      based on the brightness of the output image.\n"
			"      Default is linear,linear.\n"
			"      Note that 'colorspace' replaces obsolete 'srgb' option. See the table\n"
			"      below for the correspondence between srgb and colorspace:\n" 
			"      srgb=0 - colorspace=linear,linear\n"
			"      srgb=1 - colorspace=sRGB,auto\n"
			"      srgb=2 - colorspace=sRGB,sRGB\n"
			"      srgb=3 - colorspace=linear,sRGB\n"
			"      srgb=4 - colorspace=sRGB,linear\n");
		pRC->RegisterKey("rgbk",
			"[TIF] enables compression pass of HDR images to RGBK format\n"
			"      0 - don't use RGBK encoding (default)\n"
			"      1 - use linear RGBK encoding\n"
			"      2 - use quadratic RGBK encoding\n"
			"      3 - use quadratic RGBK encoding with distinct values for K\n");
		pRC->RegisterKey("cm","[TIF] 1=enables cubemap TIF processing(TIF needs to have 6:1 aspect), 0 - default");
		pRC->RegisterKey("cm_ftype","[TIF] cubemap angular filter type: gaussian, cone, disc, cosine, cosine_power, ggx");
		pRC->RegisterKey("cm_fangle","[TIF] base filter angle for cubemap filtering(degrees), 0 - disabled");
		pRC->RegisterKey("cm_fmipangle","[TIF] initial mip filter angle for cubemap filtering(degrees), 0 - disabled");
		pRC->RegisterKey("cm_fmipslope","[TIF] mip filter angle multiplier for cubemap filtering, 1 - default");
		pRC->RegisterKey("cm_edgefixup","[TIF] cubemap edge fix-up width, 0 - disabled");
		pRC->RegisterKey("cm_diff","[TIF] 1=generate a diffuse illumination light-probe in addition");
		pRC->RegisterKey("cm_diffpreset","[TIF] the name of the preset to be used for the diffuse probe");
		pRC->RegisterKey("cm_ggxsamplecount","[TIF] number of samples for importance-sampling GGX");

		pRC->RegisterKey("autooptimize","[TIF] 0 - disable automatic size reducing, rc.ini only");
		pRC->RegisterKey("autooptimizefile","[TIF] 0/1=automatic size reducing for this file, 1 - default");
		pRC->RegisterKey("minalpha","[TIF] 0..255 to limit alpha value, (can prevent NAN in shader)");
		pRC->RegisterKey("discardalpha","[TIF] 0/1=discard alpha in input image");
		pRC->RegisterKey("powof2","[TIF] 1=texture needs to be power of 2 in width and height, 0 otherwise");
		pRC->RegisterKey("detectL8","[TIF] if R=G=B then use luminance formats if supported by the platform (L8 or A8L8)");
		pRC->RegisterKey("detectA1","[TIF] if b/w alpha then use DXT1a instead of DXT3/5");
		pRC->RegisterKey("M0","**OBSOLETE** [TIF] adjust mipalpha, 0..50=normal..100");
		pRC->RegisterKey("M1","**OBSOLETE** [TIF] adjust mipalpha, 0..50=normal..100");
		pRC->RegisterKey("M2","**OBSOLETE** [TIF] adjust mipalpha, 0..50=normal..100");
		pRC->RegisterKey("M3","**OBSOLETE** [TIF] adjust mipalpha, 0..50=normal..100");
		pRC->RegisterKey("M4","**OBSOLETE** [TIF] adjust mipalpha, 0..50=normal..100");
		pRC->RegisterKey("M5","**OBSOLETE** [TIF] adjust mipalpha, 0..50=normal..100");
		pRC->RegisterKey("M","[TIF] compact version of M0,M1,...");
		pRC->RegisterKey("nooutput","[TIF] supress output");
		pRC->RegisterKey("bumptype_a","[TIF] 0=none, 1=Gauss, 2=GaussAlpha");
		pRC->RegisterKey("bumpstrength_a","[TIF] -1000.0 .. 1000.0");
		pRC->RegisterKey("bumpblur_a","[TIF] 0 .. 10.0");

		pRC->RegisterKey("colormodel","[TIF] 0=RGB (default), 1=CIE, 2=Y'CbCr, 3=Y'FbFr");
		pRC->RegisterKey("colorchart","[TIF] extract color chart from image, 0=off (default), 1=on (3d lut)");
		pRC->RegisterKey("previewstretched","[TIF] preview destination image with size of source image, 0=off, 1=on (default)");

		pRC->RegisterKey("highpass",
			"[TIF] 0=off, >0=defines which mip level is subtracted when applying the\n"
			"      [cheap] high pass filter - this prepares assets to\n"
			"      allow this in the shader");

		pRC->RegisterKey("autopreset",
			"[TIF] depending on some rules defined in C++ (e.g. path) the preset may be overwritten\n"
			"      this is mostly used to support legacy assets"
			"      0=off, 1=on (default)");

		pRC->RegisterKey("imageconverterquality",
			"[TIF] preview  - for the 256x256 preview only\n"
			"      fast		- fast mode with less quality\n"
			"      normal	- default\n"
			"      slow		- slow mode with superb quality");
		pRC->RegisterKey("blocksize",
			"[TIF] ASTC texture compression block size. Valid sizes:\n"
			"      4x4   - 8    bit per pixel, 125% larger than next one (default)\n"
			"      5x4   - 6.4  bit per pixel, 125% larger than next one \n"
			"      5x5   - 5.12 bit per pixel, 120% larger than next one \n"
			"      6x5   - 4.27 bit per pixel, 120% larger than next one \n"
			"      6x6   - 3.56 bit per pixel, 114% larger than next one \n"
			"      8x5   - 3.2  bit per pixel, 120% larger than next one \n"
			"      8x6   - 2.67 bit per pixel, 105% larger than next one \n"
			"      10x5  - 2.56 bit per pixel, 120% larger than next one \n"
			"      10x6  - 2.13 bit per pixel, 107% larger than next one \n"
			"      8x8   - 2    bit per pixel, 125% larger than next one \n"
			"      10x8  - 1.6  bit per pixel, 125% larger than next one \n"
			"      10x10 - 1.28 bit per pixel, 120% larger than next one \n"
			"      12x10 - 1.07 bit per pixel, 120% larger than next one \n"
			"      12x12 - 0.89 bit per pixel");
	}

	// Texture splitter
	{
		pRC->RegisterConverter("TextureSplitter", new CTextureSplitter());
		pRC->RegisterKey("decompress", "[DDS] 0/1 to decompress dds to tif");
		pRC->RegisterKey("dont_split","[DDS] don't split the file for streaming layout");	
		pRC->RegisterKey("numstreamablemips", "[DDS] Number of mips that should be available for streaming - defaults to all");
	}

	// Old RC (before 2013) incorrectly saved 16-bit float TIFF.
	{
		pRC->RegisterKey("CryTIF2012", 
			"[TIF] Preventing interpretation of old CryTif 16-bit float images as 16-bit uint images\n"
			"      0 - disabled (default)\n"
			"      1 - CryTIF 2012 compatibility mode"
		);
	}
}
