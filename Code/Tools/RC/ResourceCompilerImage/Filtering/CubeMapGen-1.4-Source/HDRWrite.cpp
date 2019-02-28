// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Radiance format .HDR  write routines
//
//  based loosely on RGBE conversion code by Bruce Walter and Greg Ward
//
//--------------------------------------------------------------------------------------
// (C) 2005 ATI Research, Inc., All rights reserved.
//--------------------------------------------------------------------------------------

#include "HDRWrite.h"
#include <math.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>

//--------------------------------------------------------------------------------------
//conversion from 3 float RGB values to rgbe pixels 
//--------------------------------------------------------------------------------------
static void float2rgbe(uint8 a_RGBE[4], float32 a_Red, float32 a_Green, float32 a_Blue)
{
   float32 maxChannel;
   int32 exponent;

   maxChannel = a_Red;

   if (a_Green > maxChannel) 
   {
      maxChannel = a_Green;
   }

   if (a_Blue > maxChannel)
   {
      maxChannel = a_Blue;
   }

   if (maxChannel < 1e-32) 
   {
      a_RGBE[0] = 0;
      a_RGBE[1] = 0;
      a_RGBE[2] = 0;
      a_RGBE[3] = 0;
   }
   else 
   {
      maxChannel = frexpf(maxChannel, &exponent) * 256.0f / maxChannel;
      a_RGBE[0] = (uint8) (a_Red * maxChannel);
      a_RGBE[1] = (uint8) (a_Green * maxChannel);
      a_RGBE[2] = (uint8) (a_Blue * maxChannel);
      a_RGBE[3] = (uint8) (exponent + 128);
   }
}


//--------------------------------------------------------------------------------------
//conversion from rgbe to float pixels 
//
//--------------------------------------------------------------------------------------
static void rgbe2float(float32 *a_Red, float32 *a_Green, float32 *a_Blue, uint8 a_RGBE[4])
{
   if (a_RGBE[3] != 0) 
   {    
      //if the exponent is non-zero
      float32 scaleFactor = (float32)ldexp(1.0, a_RGBE[3] - (int32)(128+8));
      *a_Red = a_RGBE[0] * scaleFactor;
      *a_Green = a_RGBE[1] * scaleFactor;
      *a_Blue = a_RGBE[2] * scaleFactor;
   }
   else
   {
      *a_Red = *a_Green = *a_Blue = 0.0;
   }
}


//--------------------------------------------------------------------------------------
// write .HDR header
//
//--------------------------------------------------------------------------------------
void HDR_WriteHeader(FILE *fp, int32 a_Width, int32 a_Height)
{
   char *programtype = "RADIANCE";

   // The #? is to identify file type, the programtype is optional. 
   fprintf(fp,"#?%s%c", programtype, 10);
   fprintf(fp,"# Created with CubeMapGen%c", 10);

   /*
   fprintf(fp,"GAMMA=%g%c", gamma, 10)
   */

   fprintf(fp,"FORMAT=32-bit_rle_rgbe%c%c", 10, 10); 
   fprintf(fp, "-Y %d +X %d%c", a_Height, a_Width, 10);

}


//--------------------------------------------------------------------------------------
// write uncompressed .HDR data 
//
//--------------------------------------------------------------------------------------
void HDR_WritePixels(FILE *fp, float32 *a_RawData, int32 a_NumPixels)
{
   uint8 RGBEData[4];
   int32 i;
   float32 *rawDataPtr;

   rawDataPtr = a_RawData;

   for(i=0; i<a_NumPixels; i++)   
   {
      float2rgbe(RGBEData, rawDataPtr[0], rawDataPtr[1], rawDataPtr[2]);
      rawDataPtr += 3;
            
      fwrite(RGBEData, sizeof(RGBEData), 1, fp); 
   }

}


