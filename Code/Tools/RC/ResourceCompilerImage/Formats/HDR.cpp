// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"

#include "../ImageObject.h"  // ImageToProcess

/* THIS CODE CARRIES NO GUARANTEE OF USABILITY OR FITNESS FOR ANY PURPOSE.
 * WHILE THE AUTHORS HAVE TRIED TO ENSURE THE PROGRAM WORKS CORRECTLY,
 * IT IS STRICTLY USE AT YOUR OWN RISK.  */

/* utility for reading and writing Ward's rgbe image format.
   See rgbe.txt file for more details.
*/

#include <stdio.h>

typedef struct {
  int valid;            /* indicate which fields are valid */
  char programtype[16]; /* listed at beginning of file to identify it 
                         * after "#?".  defaults to "RGBE" */ 
  float gamma;          /* image has already been gamma corrected with 
                         * given gamma.  defaults to 1.0 (no correction) */
  float exposure;       /* a value of 1.0 in an image corresponds to
			 * <exposure> watts/steradian/m^2. 
			 * defaults to 1.0 */
  char instructions[512];
} rgbe_header_info;

/* flags indicating which fields in an rgbe_header_info are valid */
#define RGBE_VALID_PROGRAMTYPE	0x01
#define RGBE_VALID_GAMMA				0x02
#define RGBE_VALID_EXPOSURE			0x04
#define RGBE_VALID_INSTRUCTIONS	0x08

/* return codes for rgbe routines */
#define RGBE_RETURN_SUCCESS 0
#define RGBE_RETURN_FAILURE -1

/* read or write headers */
/* you may set rgbe_header_info to null if you want to */
int RGBE_WriteHeader(FILE *fp, uint32 width, uint32 height, rgbe_header_info *info);
int RGBE_ReadHeader(FILE *fp, uint32 *width, uint32 *height, rgbe_header_info *info);

/* read or write pixels */
/* can read or write pixels in chunks of any size including single pixels*/
int RGBE_WritePixels(FILE *fp, float *data, int numpixels);
int RGBE_ReadPixels(FILE *fp, float *data, int numpixels);

/* read or write run length encoded files */
/* must be called to read or write whole scanlines */
int RGBE_WritePixels_RLE(FILE *fp, float *data, uint32 scanline_width,
			 uint32 num_scanlines);
int RGBE_ReadPixels_RLE(FILE *fp, float *data, uint32 scanline_width,
			uint32 num_scanlines);

/* THIS CODE CARRIES NO GUARANTEE OF USABILITY OR FITNESS FOR ANY PURPOSE.
 * WHILE THE AUTHORS HAVE TRIED TO ENSURE THE PROGRAM WORKS CORRECTLY,
 * IT IS STRICTLY USE AT YOUR OWN RISK.  */

#include <math.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>

/* This file contains code to read and write four byte rgbe file format
 developed by Greg Ward.  It handles the conversions between rgbe and
 pixels consisting of floats.  The data is assumed to be an array of floats.
 By default there are three floats per pixel in the order red, green, blue.
 (RGBE_DATA_??? values control this.)  Only the mimimal header reading and 
 writing is implemented.  Each routine does error checking and will return
 a status value as defined below.  This code is intended as a skeleton so
 feel free to modify it to suit your needs.

 (Place notice here if you modified the code.)
 posted to http://www.graphics.cornell.edu/~bjw/
 written by Bruce Walter  (bjw@graphics.cornell.edu)  5/26/95
 based on code written by Greg Ward
*/

#ifndef INLINE
#ifdef _CPLUSPLUS
/* define if your compiler understands inline commands */
#define INLINE inline
#else
#define INLINE
#endif
#endif

/* offsets to red, green, and blue components in a data (float) pixel */
#define RGBE_DATA_RED    0
#define RGBE_DATA_GREEN  1
#define RGBE_DATA_BLUE   2
#define RGBE_DATA_ALPHA  3
/* number of floats per pixel */
#define RGBE_DATA_SIZE   4

enum rgbe_error_codes {
  rgbe_read_error,
  rgbe_write_error,
  rgbe_format_error,
  rgbe_memory_error,
};

/* default error routine.  change this to change error handling */
static int rgbe_error(int rgbe_error_code, char *msg)
{
  switch (rgbe_error_code) {
	case rgbe_read_error:
		RCLogError("RGBE read error");
    break;
  case rgbe_write_error:
    RCLogError("RGBE write error");
    break;
  case rgbe_format_error:
    RCLogError("RGBE bad file format: %s\n",msg);
    break;
  default:
  case rgbe_memory_error:
    RCLogError("RGBE error: %s\n",msg);
  }
  return RGBE_RETURN_FAILURE;
}

/* standard conversion from float pixels to rgbe pixels */
/* note: you can remove the "inline"s if your compiler complains about it */
static INLINE void 
float2rgbe(unsigned char rgbe[4], float red, float green, float blue)
{
  float v;
  int e;

  v = red;
  if (green > v) v = green;
  if (blue > v) v = blue;
  if (v < 1e-32f) {
    rgbe[0] = rgbe[1] = rgbe[2] = rgbe[3] = 0;
  }
  else {
    v = frexp(v,&e) * 256.0f/v;
    rgbe[0] = (unsigned char) (red * v);
    rgbe[1] = (unsigned char) (green * v);
    rgbe[2] = (unsigned char) (blue * v);
    rgbe[3] = (unsigned char) (e + 128);
  }
}

/* standard conversion from rgbe to float pixels */
/* note: Ward uses ldexp(col+0.5,exp-(128+8)).  However we wanted pixels */
/*       in the range [0,1] to map back into the range [0,1].            */
static INLINE void 
rgbe2float(float *red, float *green, float *blue, unsigned char rgbe[4])
{

  if (rgbe[3]) {   /*nonzero pixel*/
    float f = ldexp(1.0f,rgbe[3]-(int)(128+8));
    *red = rgbe[0] * f;
    *green = rgbe[1] * f;
    *blue = rgbe[2] * f;
  }
  else
    *red = *green = *blue = 0.0;
}

/* default minimal header. modify if you want more information in header */
int RGBE_WriteHeader(FILE *fp, uint32 width, uint32 height, rgbe_header_info *info)
{
  char *programtype = "RGBE";

  if (info && (info->valid & RGBE_VALID_PROGRAMTYPE))
    programtype = info->programtype;
  if (fprintf(fp,"#?%s\n",programtype) < 0)
    return rgbe_error(rgbe_write_error,NULL);
  /* The #? is to identify file type, the programtype is optional. */
  if (info && (info->valid & RGBE_VALID_GAMMA)) {
    if (fprintf(fp,"GAMMA=%g\n",info->gamma) < 0)
      return rgbe_error(rgbe_write_error,NULL);
  }
  if (info && (info->valid & RGBE_VALID_EXPOSURE)) {
    if (fprintf(fp,"EXPOSURE=%g\n",info->exposure) < 0)
      return rgbe_error(rgbe_write_error,NULL);
	}
	if (info && (info->valid & RGBE_VALID_INSTRUCTIONS)) {
		if (fprintf(fp,"INSTRUCTIONS=%s\n",info->instructions) < 0)
			return rgbe_error(rgbe_write_error,NULL);
	}
  if (fprintf(fp,"FORMAT=32-bit_rle_rgbe\n\n") < 0)
    return rgbe_error(rgbe_write_error,NULL);
  if (fprintf(fp, "-Y %d +X %d\n", height, width) < 0)
    return rgbe_error(rgbe_write_error,NULL);
  return RGBE_RETURN_SUCCESS;
}

/* minimal header reading.  modify if you want to parse more information */
int RGBE_ReadHeader(FILE *fp, uint32 *width, uint32 *height, rgbe_header_info *info)
{
  char buf[512];
  float tempf;
  int i;

  if (info) {
    info->valid = 0;
    info->programtype[0] = 0;
    info->gamma = info->exposure = 1.0;
  }
  if (fgets(buf,sizeof(buf)/sizeof(buf[0]),fp) == NULL)
    return rgbe_error(rgbe_read_error,NULL);
  if ((buf[0] != '#')||(buf[1] != '?')) {
    /* if you want to require the magic token then uncomment the next line */
    /*return rgbe_error(rgbe_format_error,"bad initial token"); */
  }
  else if (info) {
    info->valid |= RGBE_VALID_PROGRAMTYPE;
    for(i=0;i<sizeof(info->programtype)-1;i++) {
      if ((buf[i+2] == 0) || isspace(buf[i+2]))
	break;
      info->programtype[i] = buf[i+2];
    }
    info->programtype[i] = 0;
    if (fgets(buf,sizeof(buf)/sizeof(buf[0]),fp) == 0)
      return rgbe_error(rgbe_read_error,NULL);
  }
  for(;;) {
    if ((buf[0] == 0)||(buf[0] == '\n'))
      return rgbe_error(rgbe_format_error,"no FORMAT specifier found");
    else if (strcmp(buf,"FORMAT=32-bit_rle_rgbe\n") == 0)
      break;       /* format found so break out of loop */
    else if (info && (sscanf(buf,"GAMMA=%g",&tempf) == 1)) {
      info->gamma = tempf;
      info->valid |= RGBE_VALID_GAMMA;
    }
    else if (info && (sscanf(buf,"EXPOSURE=%g",&tempf) == 1)) {
      info->exposure = tempf;
      info->valid |= RGBE_VALID_EXPOSURE;
		}
		else if (info && (!strncmp(buf,"INSTRUCTIONS=",13))) {
			info->valid |= RGBE_VALID_INSTRUCTIONS;
			for(i=0;i<sizeof(info->instructions)-1;i++) {
				if ((buf[i+13] == 0) || isspace(buf[i+13]))
					break;
				info->instructions[i] = buf[i+13];
			}
		}
    if (fgets(buf,sizeof(buf)/sizeof(buf[0]),fp) == 0)
      return rgbe_error(rgbe_read_error,NULL);
  }
  if (fgets(buf,sizeof(buf)/sizeof(buf[0]),fp) == 0)
    return rgbe_error(rgbe_read_error,NULL);
  if (strcmp(buf,"\n") != 0)
    return rgbe_error(rgbe_format_error,
		      "missing blank line after FORMAT specifier");
  if (fgets(buf,sizeof(buf)/sizeof(buf[0]),fp) == 0)
    return rgbe_error(rgbe_read_error,NULL);
  if (sscanf(buf,"-Y %d +X %d",height,width) < 2)
    return rgbe_error(rgbe_format_error,"missing image size specifier");
  return RGBE_RETURN_SUCCESS;
}

/* simple write routine that does not use run length encoding */
/* These routines can be made faster by allocating a larger buffer and
   fread-ing and fwrite-ing the data in larger chunks */
int RGBE_WritePixels(FILE *fp, float *data, int numpixels)
{
  unsigned char rgbe[4];

  while (numpixels-- > 0) {
    float2rgbe(rgbe,data[RGBE_DATA_RED],
	       data[RGBE_DATA_GREEN],data[RGBE_DATA_BLUE]);
    data += RGBE_DATA_SIZE;
    if (fwrite(rgbe, sizeof(rgbe), 1, fp) < 1)
      return rgbe_error(rgbe_write_error,NULL);
  }
  return RGBE_RETURN_SUCCESS;
}

/* simple read routine.  will not correctly handle run length encoding */
int RGBE_ReadPixels(FILE *fp, float *data, int numpixels)
{
  unsigned char rgbe[4];

  while(numpixels-- > 0) {
    if (fread(rgbe, sizeof(rgbe), 1, fp) < 1)
      return rgbe_error(rgbe_read_error,NULL);
    rgbe2float(&data[RGBE_DATA_RED],&data[RGBE_DATA_GREEN],
	       &data[RGBE_DATA_BLUE],rgbe);
		data[RGBE_DATA_ALPHA] = 0.0f;
    data += RGBE_DATA_SIZE;
  }
  return RGBE_RETURN_SUCCESS;
}

/* The code below is only needed for the run-length encoded files. */
/* Run length encoding adds considerable complexity but does */
/* save some space.  For each scanline, each channel (r,g,b,e) is */
/* encoded separately for better compression. */

static int RGBE_WriteBytes_RLE(FILE *fp, unsigned char *data, int numbytes)
{
#define MINRUNLENGTH 4
  int cur = 0;
  while(cur < numbytes) {
    unsigned char buf[2];
    int beg_run = cur;
    /* find next run of length at least 4 if one exists */
    int run_count = 0;
    int old_run_count = 0;
    while((run_count < MINRUNLENGTH) && (beg_run < numbytes)) {
      beg_run += run_count;
      old_run_count = run_count;
      run_count = 1;
      while((data[beg_run] == data[beg_run + run_count]) 
        && (beg_run + run_count < numbytes) && (run_count < 127))
        run_count++;
    }
    /* if data before next big run is a short run then write it as such */
    if ((old_run_count > 1)&&(old_run_count == beg_run - cur)) {
      buf[0] = 128 + old_run_count;   /*write short run*/
      buf[1] = data[cur];
      if (fwrite(buf,sizeof(buf[0])*2,1,fp) < 1)
        return rgbe_error(rgbe_write_error,NULL);
      cur = beg_run;
    }
    /* write out bytes until we reach the start of the next run */
    while(cur < beg_run) {
      int nonrun_count = beg_run - cur;
      if (nonrun_count > 128) 
        nonrun_count = 128;
      buf[0] = nonrun_count;
      if (fwrite(buf,sizeof(buf[0]),1,fp) < 1)
        return rgbe_error(rgbe_write_error,NULL);
      if (fwrite(&data[cur],sizeof(data[0])*nonrun_count,1,fp) < 1)
        return rgbe_error(rgbe_write_error,NULL);
      cur += nonrun_count;
    }
    /* write out next run if one was found */
    if (run_count >= MINRUNLENGTH) {
      buf[0] = 128 + run_count;
      buf[1] = data[beg_run];
      if (fwrite(buf,sizeof(buf[0])*2,1,fp) < 1)
        return rgbe_error(rgbe_write_error,NULL);
      cur += run_count;
    }
  }
  return RGBE_RETURN_SUCCESS;
#undef MINRUNLENGTH
}

int RGBE_WritePixels_RLE(FILE *fp, float *data, uint32 scanline_width, uint32 num_scanlines)
{
  unsigned char rgbe[4];
  unsigned char *buffer;
  int i, err;

  if ((scanline_width < 8)||(scanline_width > 0x7fff))
    /* run length encoding is not allowed so write flat*/
    return RGBE_WritePixels(fp,data,scanline_width*num_scanlines);
  buffer = (unsigned char *)malloc(sizeof(unsigned char)*4*scanline_width);
  if (buffer == NULL) 
    /* no buffer space so write flat */
    return RGBE_WritePixels(fp,data,scanline_width*num_scanlines);
  while(num_scanlines-- > 0) {
    rgbe[0] = 2;
    rgbe[1] = 2;
    rgbe[2] = scanline_width >> 8;
    rgbe[3] = scanline_width & 0xFF;
    if (fwrite(rgbe, sizeof(rgbe), 1, fp) < 1) {
      free(buffer);
      return rgbe_error(rgbe_write_error,NULL);
    }
    for(i=0;i<scanline_width;i++) {
      float2rgbe(rgbe,data[RGBE_DATA_RED],
        data[RGBE_DATA_GREEN],data[RGBE_DATA_BLUE]);
      buffer[i] = rgbe[0];
      buffer[i+scanline_width] = rgbe[1];
      buffer[i+2*scanline_width] = rgbe[2];
      buffer[i+3*scanline_width] = rgbe[3];
      data += RGBE_DATA_SIZE;
    }
    /* write out each of the four channels separately run length encoded */
    /* first red, then green, then blue, then exponent */
    for(i=0;i<4;i++) {
      if ((err = RGBE_WriteBytes_RLE(fp,&buffer[i*scanline_width],
				     scanline_width)) != RGBE_RETURN_SUCCESS) {
				free(buffer);
				return err;
      }
    }
  }
  free(buffer);
  return RGBE_RETURN_SUCCESS;
}
      
int RGBE_ReadPixels_RLE(FILE *fp, float *data, uint32 scanline_width,
			uint32 num_scanlines)
{
  unsigned char rgbe[4], *scanline_buffer, *ptr, *ptr_end;
  int i, count;
  unsigned char buf[2];

  if ((scanline_width < 8)||(scanline_width > 0x7fff))
    /* run length encoding is not allowed so read flat*/
    return RGBE_ReadPixels(fp,data,scanline_width*num_scanlines);
  scanline_buffer = NULL;
  /* read in each successive scanline */
  while(num_scanlines > 0) {
    if (fread(rgbe,sizeof(rgbe),1,fp) < 1) {
      free(scanline_buffer);
      return rgbe_error(rgbe_read_error,NULL);
    }
    if ((rgbe[0] != 2)||(rgbe[1] != 2)||(rgbe[2] & 0x80)) {
      /* this file is not run length encoded */
      rgbe2float(&data[0],&data[1],&data[2],rgbe);
      data += RGBE_DATA_SIZE;
      free(scanline_buffer);
      return RGBE_ReadPixels(fp,data,scanline_width*num_scanlines-1);
    }
    if ((((int)rgbe[2])<<8 | rgbe[3]) != scanline_width) {
      free(scanline_buffer);
      return rgbe_error(rgbe_format_error,"wrong scanline width");
    }
    if (scanline_buffer == NULL)
      scanline_buffer = (unsigned char *)
				malloc(sizeof(unsigned char)*4*scanline_width);
    if (scanline_buffer == NULL) 
      return rgbe_error(rgbe_memory_error,"unable to allocate buffer space");
    
    ptr = &scanline_buffer[0];
    /* read each of the four channels for the scanline into the buffer */
    for(i=0;i<4;i++) {
      ptr_end = &scanline_buffer[(i+1)*scanline_width];
      while(ptr < ptr_end) {
				if (fread(buf,sizeof(buf[0])*2,1,fp) < 1) {
					free(scanline_buffer);
					return rgbe_error(rgbe_read_error,NULL);
				}
				if (buf[0] > 128) {
					/* a run of the same value */
					count = buf[0]-128;
					if ((count == 0)||(count > ptr_end - ptr)) {
						free(scanline_buffer);
						return rgbe_error(rgbe_format_error,"bad scanline data");
					}
					while(count-- > 0)
						*ptr++ = buf[1];
				}
				else {
					/* a non-run */
					count = buf[0];
					if ((count == 0)||(count > ptr_end - ptr)) {
						free(scanline_buffer);
						return rgbe_error(rgbe_format_error,"bad scanline data");
					}
					*ptr++ = buf[1];
					if (--count > 0) {
						if (fread(ptr,sizeof(*ptr)*count,1,fp) < 1) {
							free(scanline_buffer);
							return rgbe_error(rgbe_read_error,NULL);
						}
						ptr += count;
					}
				}
      }
    }
    /* now convert data from buffer into floats */
    for(i=0;i<scanline_width;i++) {
      rgbe[0] = scanline_buffer[i];
      rgbe[1] = scanline_buffer[i+scanline_width];
      rgbe[2] = scanline_buffer[i+2*scanline_width];
      rgbe[3] = scanline_buffer[i+3*scanline_width];
      rgbe2float(&data[RGBE_DATA_RED],&data[RGBE_DATA_GREEN],
				&data[RGBE_DATA_BLUE],rgbe);
			data[RGBE_DATA_ALPHA] = 0.0f;
      data += RGBE_DATA_SIZE;
    }
    num_scanlines--;
  }
  free(scanline_buffer);
  return RGBE_RETURN_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////////

namespace
{
class CryHdrConfigSink
	: public IConfigSink
{
public:
	CryHdrConfigSink()
	{
	}

	~CryHdrConfigSink()
	{
	}

	// interface IConfigSink -------------------------------------

	virtual void SetKeyValue(EConfigPriority ePri, const char* key, const char* value)
	{
		assert(ePri == eCP_PriorityFile);

		// We skip deprecated/obsolete keys to avoid having too long
		// and/or polluted config strings. 
		// Note that Photoshop silently truncates config strings
		// to 256 characters in length so this skipping helps us
		// to avoid hitting this limit.
		static const char* const keysBlackList[] =
		{
			"dither",		// obsolete
			"m0",			// obsolete
			"m1",			// obsolete
			"m2",			// obsolete
			"m3",			// obsolete
			"m4",			// obsolete
			"m5",			// obsolete
			"mipgamma",		// obsolete
			"pixelformat",  // no need to store it in file, it's read from preset only
		};
		for (size_t i = 0; i < sizeof(keysBlackList) / sizeof(keysBlackList[0]); ++i)
		{
			if (stricmp(key, keysBlackList[i]) == 0)
			{
				return;
			}
		}

		// determine if we need to use quotes around the value
		bool bQuotes = false;        
		{
			const char* p = value;
			while (*p)
			{
				if (!IConfig::IsValidNameChar(*p))
				{
					bQuotes = true;
					break;
				}
				++p;
			}
		}

		const string keyValue =
			bQuotes
			? string("/") + key + "=\"" + value + "\""
			: string("/") + key + "=" + value;

		// We skip keys with default values to avoid having too long
		// config strings. 
		// Note that Photoshop silently truncates config strings
		// to 255 characters in length so this skipping helps us
		// to avoid hitting this limit.
		for (int i = 0; ; ++i)
		{
			const char* const defaultProp = CImageProperties::GetDefaultProperty(i);
			if (!defaultProp)
			{
				break;
			}
			if (stricmp(keyValue.c_str() + 1, defaultProp) == 0)  // '+ 1' because defaultProp doesn't have "/" in the beginning
			{
				return;
			}
		}

		if (!m_result.empty())
		{
			m_result += " ";
		}
		m_result += keyValue;
	}
	//------------------------------------------------------------

	const string& GetAsString() const
	{
		return m_result;
	}

private:
	string m_result;
};
}

///////////////////////////////////////////////////////////////////////////////////

namespace ImageHDR
{

ImageObject* LoadByUsingHDRLoader(const char* filenameRead, CImageProperties* pProps, string& res_specialInstructions)
{
	res_specialInstructions.clear();

	rgbe_header_info info;
	uint32 dwWidth, dwHeight;

	std::auto_ptr<ImageObject> pRet;

	FILE* const pHdrFile = fopen(filenameRead, "rb");
	if (!pHdrFile) 
	{
		char messageBuffer[2048] = { 0 };
		FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, messageBuffer, sizeof(messageBuffer) - 1, NULL);
		string messagetrim(messageBuffer);
		messagetrim.replace('\n',' ');
		RCLogError("%s: fopen failed (%s) - (%s)", __FUNCTION__, messagetrim.c_str(), filenameRead); 
	}
	else
	{
		if (RGBE_RETURN_SUCCESS != RGBE_ReadHeader(pHdrFile, &dwWidth, &dwHeight, &info))
		{
			fclose(pHdrFile);
			RCLogError("Unsupported HDR format");
			assert(0);
			return NULL;
		}

		if (info.valid & RGBE_VALID_INSTRUCTIONS)
		{
			res_specialInstructions = info.instructions;
		}

		std::auto_ptr<ImageObject> pLoad(new ImageObject(dwWidth, dwHeight, 1, ePixelFormat_A32B32G32R32F, ImageObject::eCubemap_UnknownYet));

		char* pDst;
		uint32 dwPitch;
		pLoad->GetImagePointer(0, pDst, dwPitch);
		assert(dwPitch == dwWidth * sizeof(float) * 4);

		if (RGBE_RETURN_SUCCESS != RGBE_ReadPixels_RLE(pHdrFile, (float*)(void*)pDst, dwWidth, dwHeight))
		{
			fclose(pHdrFile);
			RCLogError("Failure reading HDR format");
			assert(0);
			return NULL;
		}

		fclose(pHdrFile);

		pRet.reset(pLoad.release());
	}

	if (pProps != 0)
	{
		pProps->ClearInputImageFlags();

		if (pRet.get())
		{
			pProps->m_bPreserveAlpha = false;
		}
	}

	return pRet.release();
}

bool SaveByUsingHDRSaver(const char* filenameWrite, const CImageProperties* pProps, const ImageObject* pImageObject)
{
	assert(filenameWrite);
	assert(pImageObject);

	ImageToProcess image(pImageObject->CopyImage());

	if (pImageObject->GetPixelFormat() != ePixelFormat_A32B32G32R32F)
	{
		image.ConvertFormat(pProps, ePixelFormat_A32B32G32R32F);
	}

	rgbe_header_info info;
	uint32 dwWidth, dwHeight, dwMips;
	image.get()->GetExtent(dwWidth, dwHeight, dwMips);

	FILE* const pHdrFile = fopen(filenameWrite, "wb");
	if (!pHdrFile) 
	{
		return false;
	}

	info.valid = 0;

	if (pProps)
	{
		CryHdrConfigSink sink;
		pProps->GetMultiConfig().CopyToConfig(eCP_PriorityFile, &sink);

		size_t len = sink.GetAsString().length();
		const size_t maxLen = sizeof(info.instructions) - 1;
		if (len > maxLen)
		{
			RCLogError("CryHDR settings are too long: %d characters (max is %d characters)", (int)len, (int)maxLen);
			len = maxLen;
		}

		memcpy(info.instructions, sink.GetAsString().c_str(), len);
		info.instructions[len] = 0;

		info.valid |= RGBE_VALID_INSTRUCTIONS;
	}

	if (RGBE_RETURN_SUCCESS != RGBE_WriteHeader(pHdrFile, dwWidth, dwHeight, &info))
	{
		fclose(pHdrFile);
		RCLogError("Unsupported HDR format");
		assert(0);
		return false;
	}

	char* pSrc;
	uint32 dwPitch;		
	pImageObject->GetImagePointer(0, pSrc, dwPitch);
	assert(dwPitch == dwWidth * sizeof(float) * 4);

	if (RGBE_RETURN_SUCCESS != RGBE_WritePixels_RLE(pHdrFile, (float*)(void*)pSrc, dwWidth, dwHeight))
	{
		fclose(pHdrFile);
		RCLogError("Failure writing HDR format");
		assert(0);
		return false;
	}

	fclose(pHdrFile);

	return true;
}

}
