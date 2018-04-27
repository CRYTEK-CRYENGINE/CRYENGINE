// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//CImageSurface
// Class for storing, manipulating, and copying image data to and from D3D Surfaces
//
//--------------------------------------------------------------------------------------
// (C) 2005 ATI Research, Inc., All rights reserved.
//--------------------------------------------------------------------------------------

#include "CImageSurface.h"
#include <new.h>




//--------------------------------------------------------------------------------------
// convert D3D 16 bit float to standard 32 bit float
// Format:
// 
// 1 sign bit in MSB, (s) 
// 5 bits of biased exponent, (e) 
// 10 bits of fraction, (f), with an additional hidden bit 
// A float16 value, v, made from the format above takes the following meaning:
//
// (a) if e == 31 and f != 0, then v is NaN regardless of s 
// (b) if e == 31 and f == 0, then v = (-1)^s * infinity (signed infinity) 
// (c) if 0 < e < 31, then v = (-1)^s * 2^(e-15) * (1.f) 
// (d) if e == 0 and f != 0, then v = (-1)^s * 2^(e-14) * (0.f) (denormalized numbers) 
// (e) if e == 0 and f == 0, then v = (-1)^s *0 (signed zero) 
//
//--------------------------------------------------------------------------------------
float32 CPf16Tof32(uint16 aVal)
{
   uint32 signVal = (aVal >> 15);              //sign bit in MSB
   uint32 exponent = ((aVal >> 10) & 0x01f);   //next 5 bits after signbit
   uint32 mantissa = (aVal & 0x03ff);          //lower 10 bits
   uint32 rawFloat32Data;                      //raw binary float32 data

   //convert s10e5  5-bit exponent to IEEE754 s23e8  8-bit exponent
   if(exponent == 31)
   {  // infinity or Nan depending on mantissa
      exponent = 255;
   }
   else if(exponent == 0) 
   {  //  denormalized floats  mantissa is treated as = 0.f
      exponent = 0;
   }
   else
   {  //change 15base exponent to 127base exponent 
      //normalized floats mantissa is treated as = 1.f
      exponent += (127 - 15);
   }

   //convert 10-bit mantissa to 23-bit mantissa
   mantissa <<= (23 - 10);

   //assemble s23e8 number using logical operations
   rawFloat32Data = (signVal << 31) |  (exponent << 23) | mantissa ;
   
   //treat raw data as a 32 bit float
   return *((float32 *) &rawFloat32Data );
}


//--------------------------------------------------------------------------------------
// convert standard 32 bit float to D3D 16 bit float
//
// 16-bit float format:
// 
// 1 sign bit in MSB, (s) 
// 5 bits of biased exponent, (e) 
// 10 bits of fraction, (f), with an additional hidden bit 
// A float16 value, v, made from the format above takes the following meaning:
//
// (a) if e == 31 and f != 0, then v is NaN regardless of s 
// (b) if e == 31 and f == 0, then v = (-1)s*infinity (signed infinity) 
// (c) if 0 < e < 31, then v = (-1)s*2(e-15)*(1.f) 
// (d) if e == 0 and f != 0, then v = (-1)s*2(e-14)*(0.f) (denormalized numbers) 
// (e) if e == 0 and f == 0, then v = (-1)s*0 (signed zero) 
//--------------------------------------------------------------------------------------
uint16 CPf32Tof16(float32 aVal)
{
   uint32 rawf32Data = *((uint32 *)&aVal); //raw binary float32 data

   uint32 signVal = (rawf32Data >> 31);              //sign bit in MSB
   uint32 exponent = ((rawf32Data >> 23) & 0xff);    //next 8 bits after signbit
   uint32 mantissa = (rawf32Data & 0x7fffff);        //mantissa = lower 23 bits
   
   uint16 rawf16Data;

   //convert IEEE754 s23e8 8-bit exponent to s10e5  5-bit exponent      
   if(exponent == 255 ) 
   {//special case 32 bit float is inf or NaN, use mantissa as is
      exponent = 31;
   }
   else if(exponent < ((127-15)-10)  ) 
   {//special case, if  32-bit float exponent is out of 16-bit float range, then set 16-bit float to 0
      exponent = 0;
      mantissa = 0;
   }
   else if(exponent >= (127+(31-15)) )
   {  // max 15based exponent for s10e5 is 31
      // force s10e5 number to represent infinity by setting mantissa to 0
      //  and exponent to 31
      exponent = 31;
      mantissa = 0;
   }
   else if( exponent <= (127-15) )
   {  //convert normalized s23e8 float to denormalized s10e5 float

      //add implicit 1.0 to mantissa to convert from 1.f to use as a 0.f mantissa
      mantissa |= (1<<23);

      //shift over mantissa number of bits equal to exponent underflow
      mantissa = mantissa >> (1 + ((127-15) - exponent));

      //zero exponent to treat value as a denormalized number
      exponent = 0;
   }
   else
   {  //change 127base exponent to 15base exponent 
      // no underflow or overflow of exponent 
      //normalized floats mantissa is treated as= 1.f, so 
      // no denormalization or exponent derived shifts to the mantissa         
      exponent -= (127 - 15);
   }

   //convert 23-bit mantissa to 10-bit mantissa
   mantissa >>= (23 - 10);

   //assemble s10e5 number using logical operations
   rawf16Data = (signVal << 15) | (exponent << 10) | mantissa;

   //return re-assembled raw data as a 32 bit float
   return rawf16Data;
}


//--------------------------------------------------------------------------------------
//size of data types in bytes
//--------------------------------------------------------------------------------------
int32 CPTypeSizeOf(int32 a_Type)
{
   switch(a_Type)
   {
      case CP_VAL_UNORM8:    
      case CP_VAL_UNORM8_BGRA:    
          return 1;
      break;
      case CP_VAL_UNORM16:
          return 2;
      break;
      case CP_VAL_FLOAT16:
          return 2;
      break;
      case CP_VAL_FLOAT32:
          return 4;
      break;
      default:
          return 1;
      break;
   }
}


//--------------------------------------------------------------------------------------
//get value of data pointed to by a_Ptr given type information
//--------------------------------------------------------------------------------------
CP_ITYPE CPTypeGetVal(int32 a_Type, void *a_Ptr)
{
   switch(a_Type)
   {
      case CP_VAL_UNORM8:
      case CP_VAL_UNORM8_BGRA:
         return (1.0f/255.0f) * *((uint8 *)a_Ptr);
      break;
      case CP_VAL_UNORM16:
         return (1.0f/65535.0f) * *((uint16 *)a_Ptr);
      break;
      case CP_VAL_FLOAT16:
         return CPf16Tof32( *((uint16 *)a_Ptr) );
      break;
      case CP_VAL_FLOAT32:
         return *((float32 *) a_Ptr);
      break;
      default:
         return 0;
      break;
   }
}


//--------------------------------------------------------------------------------------
//Given a CP_ITYPE value as input, convert it to the given type specified by a_Type
//  and write the value to a_Ptr 
//--------------------------------------------------------------------------------------
void CPTypeSetVal(CP_ITYPE a_Val, int32 a_Type, void *a_Ptr)
{
   CP_ITYPE clampVal;  //clamp value to 0-1 range to output UNORM types

   switch(a_Type)
   {
      case CP_VAL_UNORM8:
      case CP_VAL_UNORM8_BGRA:
         clampVal = VM_MIN(VM_MAX(a_Val, 0.0f), 1.0f);
         *((uint8 *)a_Ptr) = (uint8)(clampVal * 255.0f);
      break;
      case CP_VAL_UNORM16:
         clampVal = VM_MIN(VM_MAX(a_Val, 0.0f), 1.0f);
         *((uint16 *)a_Ptr) = (uint16)(clampVal * 65535.0f);
      break;
      case CP_VAL_FLOAT16:
         *((uint16 *)a_Ptr) =  CPf32Tof16( a_Val );
      break;
      case CP_VAL_FLOAT32:
         *((float32 *) a_Ptr) = a_Val;
      break;
      default:
      break;
   }
}


//--------------------------------------------------------------------------------------
//Error handling for imagesurface class
//  Pop up dialog box, and terminate application
//--------------------------------------------------------------------------------------
void CImageSurface::FatalError(WCHAR *a_Msg)
{
   //MessageBoxW(NULL, a_Msg, L"Error: Application Terminating", MB_OK);

   OutputMessageString(L"CImageSurface Error: Application Terminating", a_Msg);
   exit(EM_FATAL_ERROR);

}


//--------------------------------------------------------------------------------------
// Image surface
//--------------------------------------------------------------------------------------
CImageSurface::CImageSurface(void)
{
   m_Width = 0;          //cubemap face width
   m_Height = 0;         //cubemap face height
   m_NumChannels = 0;    //number of channels
   m_ImgData = NULL;

}


//--------------------------------------------------------------------------------------
// Clear
//--------------------------------------------------------------------------------------
void CImageSurface::Clear(void)
{
   m_Width = 0;                        //cubemap face width
   m_Height = 0;                       //cubemap face height
   m_NumChannels = 0;                  //number of channels
   SAFE_DELETE_ARRAY(m_ImgData);       //safe delete old image data
}


//--------------------------------------------------------------------------------------
// Initialize surface and associated memory
//--------------------------------------------------------------------------------------
void CImageSurface::Init(int32 a_Width, int32 a_Height, int32 a_NumChannels )
{
   m_Width = a_Width;                 //cubemap face width
   m_Height = a_Height;               //cubemap face height
   m_NumChannels = a_NumChannels;     //number of channels
   
   SAFE_DELETE_ARRAY(m_ImgData);   //safe delete old image data

   m_ImgData = new(std::nothrow) CP_ITYPE [ m_Width * m_Height * m_NumChannels];   //assume tight data packing
   if(!m_ImgData)
   {
      FatalError(L"Unable to allocate data for image in CImageSurface::Init.");
   }
}


//--------------------------------------------------------------------------------------
//copy and convert data from external buffer into this surface
//
// note that srcPitch == the source pitch in bytes
//--------------------------------------------------------------------------------------
void CImageSurface::SetImageData(int32 a_SrcType, int32 a_SrcNumChannels, int32 a_SrcPitch, void *a_SrcDataPtr )
{
   int32 i, j, k;

   CP_ITYPE *dstDataWalk = m_ImgData;
   uint8 *srcDataWalk = (uint8 *)a_SrcDataPtr;

   int32 srcValueSize = CPTypeSizeOf(a_SrcType);
   int32 srcTexelStep = srcValueSize * a_SrcNumChannels;
   int32 numChannelsSet = VM_MIN(a_SrcNumChannels, m_NumChannels);
   int32 srcChannelSelect;

   //loop over rows
   for(j=0; j<m_Height; j++)
   {
      //pointer arithmetic to offset pointer by pitch in bytes
      srcDataWalk = ( (uint8 *)a_SrcDataPtr + (j * a_SrcPitch) );

      //loop over texels within row
      for(i=0; i<m_Width; i++)
      {
         srcChannelSelect = 0;

         //loop over channels within texel
         for(k=0; k<numChannelsSet; k++)
         {
            if(a_SrcType == CP_VAL_UNORM8_BGRA) //swap channels 0, and 2 if in BGRA format
            {
               switch(k)
               {
                  case 0:
                     *(dstDataWalk + 2) = CPTypeGetVal(a_SrcType, srcDataWalk + srcChannelSelect);
                  break;
                  case 2:
                     *(dstDataWalk + 0) = CPTypeGetVal(a_SrcType, srcDataWalk + srcChannelSelect);
                  break;
                  default:
                     *(dstDataWalk + k) = CPTypeGetVal(a_SrcType, srcDataWalk + srcChannelSelect);
                  break;
               }
            }
            else
            {
               *(dstDataWalk + k) = CPTypeGetVal(a_SrcType, srcDataWalk + srcChannelSelect);
            }

            srcChannelSelect += srcValueSize;
         }   

         dstDataWalk += m_NumChannels;
         srcDataWalk += srcTexelStep;
      }    
   }        
}


//--------------------------------------------------------------------------------------
// Copy and convert data from external buffer into this surface set image data degamma 
// and scale
//
//--------------------------------------------------------------------------------------
void CImageSurface::SetImageDataClampDegammaScale(int32 a_SrcType, int32 a_SrcNumChannels, int32 a_SrcPitch, 
   void *a_SrcDataPtr, float32 a_MaxClamp, float32 a_Gamma, float32 a_Scale)
{
   int32 i, j, k;

   CP_ITYPE *dstDataWalk = m_ImgData;
   uint8 *srcDataWalk = (uint8 *)a_SrcDataPtr;

   int32 srcValueSize = CPTypeSizeOf(a_SrcType);
   int32 srcTexelStep = srcValueSize * a_SrcNumChannels;
   int32 numChannelsSet = VM_MIN(a_SrcNumChannels, m_NumChannels);
   int32 srcChannelSelect;

   //loop over rows
   for(j=0; j<m_Height; j++)
   {
      //pointer arithmetic to offset pointer by pitch in bytes
      srcDataWalk = ( (uint8 *)a_SrcDataPtr + (j * a_SrcPitch) );

      //loop over texels within row
      for(i=0; i<m_Width; i++)
      {
         srcChannelSelect = 0;

         //loop over channels within texel
         for(k=0; k<numChannelsSet; k++)
         {
            CP_ITYPE texelVal; 
            
            //get texel value from external buffer
            texelVal= CPTypeGetVal(a_SrcType, srcDataWalk + srcChannelSelect);

            //clamp texelVal using max value only 
            // (using texelVal as the min clamping arguement means no minimum clamping)
            VM_CLAMP(texelVal, texelVal, texelVal, a_MaxClamp);

            if(k < 3)  //only apply gamma and scale to RGB channels
            {
               //degamma texel val, by raising to the power gamma 
               texelVal = pow(texelVal, a_Gamma);

               //scale texel val in linear space (after degamma)
               texelVal *= a_Scale;
            }

            //write data
            if( (a_SrcType == CP_VAL_UNORM8_BGRA) && (k==0))
            {
               *(dstDataWalk + 2) = texelVal;                
            }
            else if( (a_SrcType == CP_VAL_UNORM8_BGRA) && (k==2))
            {
               *(dstDataWalk + 0) = texelVal;                
            }
            else
            {
               *(dstDataWalk + k) = texelVal;
            }

            srcChannelSelect += srcValueSize;
         }   

         dstDataWalk += m_NumChannels;
         srcDataWalk += srcTexelStep;
      }    
   }        
}


//--------------------------------------------------------------------------------------
//copy data from this image surface into an external buffer
//
//--------------------------------------------------------------------------------------
void CImageSurface::GetImageData(int32 a_DstType, int32 a_DstNumChannels, int32 a_DstPitch, void *a_DstDataPtr )
{
   int32 i, j, k;

   CP_ITYPE *srcDataWalk = m_ImgData;
   uint8 *dstDataWalk = (uint8 *)a_DstDataPtr;

   int32 dstValueSize = CPTypeSizeOf(a_DstType);
   int32 dstTexelStep = dstValueSize * a_DstNumChannels;

   int32 numChannelsSet = VM_MIN(a_DstNumChannels, m_NumChannels);
   int32 dstChannelSelect;

   //loop over rows
   for(j=0; j<m_Height; j++)
   {
      //pointer arithmetic to offset pointer by pitch in bytes
      dstDataWalk = ( (uint8 *)a_DstDataPtr + (j * a_DstPitch) );

      //loop over texels within row
      for(i=0; i<m_Width; i++)
      {
         dstChannelSelect = 0;

         //loop over channels within texel
         for(k=0; k<numChannelsSet; k++)
         {               
            //write data
            if( (a_DstType == CP_VAL_UNORM8_BGRA) && (k == 0))
            {
               CPTypeSetVal(*(srcDataWalk + 2), a_DstType, dstDataWalk + dstChannelSelect);
            }
            else if( (a_DstType == CP_VAL_UNORM8_BGRA) && (k == 2))
            {
               CPTypeSetVal(*(srcDataWalk + 0), a_DstType, dstDataWalk + dstChannelSelect);
            }
            else
            {
               CPTypeSetVal(*(srcDataWalk + k), a_DstType, dstDataWalk + dstChannelSelect);
            }

            dstChannelSelect += dstValueSize;
         }   

         srcDataWalk += m_NumChannels;
         dstDataWalk += dstTexelStep;
      }    
   }       
}


//--------------------------------------------------------------------------------------
// Scale and then apply gamma to image data, then copy image data into an external buffer
//  note: only apply scale and gamma to RGB channels (e.g. first 3 channels)
// 
//--------------------------------------------------------------------------------------
void CImageSurface::GetImageDataScaleGamma(int32 a_DstType, int32 a_DstNumChannels, int32 a_DstPitch, 
    void *a_DstDataPtr, float32 a_Scale, float32 a_Gamma)
{
   int32 i, j, k;

   CP_ITYPE *srcDataWalk = m_ImgData;

   int32 dstValueSize = CPTypeSizeOf(a_DstType);
   int32 dstTexelStep = dstValueSize * a_DstNumChannels;

   int32 numChannelsSet = VM_MIN(a_DstNumChannels, m_NumChannels);
   int32 dstChannelSelect;
   
   const int numChannelsScaleGamma = 
	   (a_Scale == 1.0f && a_Gamma == 1.0f)
	   ? 0
	   : 3;  // only apply gamma and scale to RGB channels
   const float rcpGamma = 1.0f / a_Gamma;

   //loop over rows
   for(j=0; j<m_Height; j++)
   {
      //pointer arithmetic to offset pointer by pitch in bytes
      uint8 *dstDataWalk = ( (uint8 *)a_DstDataPtr + (j * a_DstPitch) );

      //loop over texels within row
      for(i=0; i<m_Width; i++)
      {
         dstChannelSelect = 0;

         //loop over channels within texel
         for(k=0; k<numChannelsSet ; k++)
         {               
            CP_ITYPE texelVal; 

            texelVal = *(srcDataWalk + k);

            if(k < numChannelsScaleGamma)
            {
               //scale texel val
               texelVal *= a_Scale;

               //apply gamma to texel val by raising the texelVal to the power of (1/gamma)
               texelVal = pow(texelVal, rcpGamma);
            }

            //write out texture value
            if( (a_DstType == CP_VAL_UNORM8_BGRA) && (k == 0))
            {
               CPTypeSetVal(texelVal, a_DstType, dstDataWalk + (dstValueSize * 2) );
            }
            else if( (a_DstType == CP_VAL_UNORM8_BGRA) && (k == 2))
            {
               CPTypeSetVal(texelVal, a_DstType, dstDataWalk + (dstValueSize * 0) );
            }
            else
            {
               CPTypeSetVal(texelVal, a_DstType, dstDataWalk + dstChannelSelect);
            }


            dstChannelSelect += dstValueSize;
         }   

         srcDataWalk += m_NumChannels;
         dstDataWalk += dstTexelStep;
      }
   }   
}


//--------------------------------------------------------------------------------------
//Set image channel a_ChannelIdx to a_ClearColor for all pixels.
//
//--------------------------------------------------------------------------------------
void CImageSurface::ClearChannelConst(int32 a_ChannelIdx, CP_ITYPE a_ClearColor)
{
   int32 u, v;
   CP_ITYPE *texelPtr;

   //if channel does not exist, do not attempt to clear the channel
   if(a_ChannelIdx > (m_NumChannels-1) )
   {
      return;
   }

   for(v=0; v<m_Height; v++)
   {
      for(u=0; u<m_Width; u++)
      {
         texelPtr = GetSurfaceTexelPtr(u, v );

         *(texelPtr + a_ChannelIdx) = a_ClearColor;
      }    
   }
}


//--------------------------------------------------------------------------------------
//Gets texel ptr in a surface given u and v coordinates, 
//   
//--------------------------------------------------------------------------------------
CP_ITYPE *CImageSurface::GetSurfaceTexelPtr(int32 u, int32 v)
{
   return( m_ImgData + (((m_Width * v) + u) * m_NumChannels) );
}


//--------------------------------------------------------------------------------------
//flips surface image in place horizontally
//
//--------------------------------------------------------------------------------------
void CImageSurface::InPlaceHorizonalFlip(void)
{
   int32 u, v, k;
   CP_ITYPE *texelPtrTop, *texelPtrBottom;

   //iterate over V
   for(v=0; v<(m_Height/2); v++)
   {
      for(u=0; u<m_Height; u++)
      {
         texelPtrTop = GetSurfaceTexelPtr(u, v );
         texelPtrBottom = GetSurfaceTexelPtr(u, (m_Height-1) - v);

         //iterate over channels
         for(k=0; k<m_NumChannels; k++)
         {
            CP_ITYPE tmpTexelVal;

            tmpTexelVal = *(texelPtrTop + k);
            *(texelPtrTop + k) = *(texelPtrBottom + k);
            *(texelPtrBottom + k) = tmpTexelVal;

         }
      }    
   }
}


//--------------------------------------------------------------------------------------
//flips surface image in place vertically
//
//--------------------------------------------------------------------------------------
void CImageSurface::InPlaceVerticalFlip(void)
{
   int32 u, v, k;
   CP_ITYPE *texelPtrLeft, *texelPtrRight;

   for(u=0; u<(m_Width/2); u++)
   {
      for(v=0; v<m_Height; v++)
      {
         texelPtrLeft = GetSurfaceTexelPtr(u, v );
         texelPtrRight = GetSurfaceTexelPtr((m_Width-1)-u, v );

         //iterate over channels
         for(k=0; k<m_NumChannels; k++)
         {
            CP_ITYPE tmpTexelVal;

            tmpTexelVal = *(texelPtrLeft + k);
            *(texelPtrLeft + k) = *(texelPtrRight + k);
            *(texelPtrRight + k) = tmpTexelVal;
         }
      }    
   }
}


//--------------------------------------------------------------------------------------
//flip image around line defined by u = v  (effectively swaps the u and v axises)
//--------------------------------------------------------------------------------------
void CImageSurface::InPlaceDiagonalUVFlip(void)
{
   int32 u, v, k;
   CP_ITYPE *texelPtrLeft, *texelPtrRight;

   if(m_Width != m_Height)
   { //only flip image if square
      return;
   }

   for(v=0; v<m_Height; v++)
   {
      for(u=0; u<v; u++) //only iterate over lower left triangle
      {
         texelPtrLeft = GetSurfaceTexelPtr(u, v );
         texelPtrRight = GetSurfaceTexelPtr(v, u );

         //iterate over channels
         for(k=0; k<m_NumChannels; k++)
         {
            CP_ITYPE tmpTexelVal;

            tmpTexelVal = *(texelPtrLeft + k);
            *(texelPtrLeft + k) = *(texelPtrRight + k);
            *(texelPtrRight + k) = tmpTexelVal;
         }
      }    
   }
}


#ifdef CG_HDR_FILE_SUPPORT
//--------------------------------------------------------------------------------------
// Write radiance .HDR File
//--------------------------------------------------------------------------------------
void CImageSurface::WriteHDRFile(WCHAR *a_FileName )
{
   FILE *ofp;

   if(m_NumChannels != 3)
   {    
      FatalError(L"CImageSurface::WriteHDRFile: only works for 3 channel CImageSurfaces.");
      return;
   }

   if(sizeof(CP_ITYPE) != 4)
   {    
      FatalError(L"CImageSurface::WriteHDRFile: requires internal data format CP_ITYPE to be float32");
      return;
   }

   errno_t res = _wfopen_s( &ofp, a_FileName, L"wb");

   if( res != 0 )
   {    
	   enum { kWcharCount = 4096 };
      WCHAR outMsg[kWcharCount];
      _snwprintf_s(outMsg, kWcharCount, kWcharCount, L"CImageSurface::WriteHDRFile: Can't open file %s for writing out RGBE image!", a_FileName);
      FatalError(outMsg);
      return;
   }
/*
   HDR_WriteHeader(ofp, m_Width, m_Height); 
   HDR_WritePixels(ofp, m_ImgData, m_Width * m_Height); 
   //HDR_WritePixels_RLE(ofp, m_ImgData, m_Width, m_Height);
*/

   fclose(ofp);
}
#endif //CG_HDR_FILE_SUPPORT


//--------------------------------------------------------------------------------------
// destructor, free all memory used
//--------------------------------------------------------------------------------------
CImageSurface::~CImageSurface()
{
   SAFE_DELETE_ARRAY(m_ImgData);    
}



