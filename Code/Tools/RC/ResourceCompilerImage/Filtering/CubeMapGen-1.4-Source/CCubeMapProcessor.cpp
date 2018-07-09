// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//CCubeMapFilter
// Classes for filtering and processing cubemaps
//
//
//--------------------------------------------------------------------------------------
// (C) 2005 ATI Research, Inc., All rights reserved.
//--------------------------------------------------------------------------------------
#include "CCubeMapProcessor.h"

// we can't build debug-builds with the concurrency-runtime,
// as _CRT_DBG_MALLOC interferes with concurrency-runtime's alloca/freea
#if !defined(_DEBUG) && !defined(_CRT_DBG_MALLOC)
#pragma warning(push)
#pragma warning(disable: 4355)  // warning C4355: 'this': used in base member initializer list
#include <ppl.h>
#pragma warning(pop)
#define PROCESS_IN_PARALLEL
#endif

#define CP_PI   3.14159265358979323846

//Filtering options for each thread
SThreadOptionsThread0 sg_FilterOptionsCPU0;

//Thread functions used to run filtering routines in the CPU0 process
// note that these functions can not be member functions since thread initiation
// must start from a global or static global function
DWORD WINAPI ThreadProcCPU0Filter(LPVOID a_NothingToPassToThisFunction)
{   
   //filter cube map mipchain
   sg_FilterOptionsCPU0.m_cmProc->FilterCubeMapMipChain(
      sg_FilterOptionsCPU0.m_BaseFilterAngle,
      sg_FilterOptionsCPU0.m_InitialMipAngle,
      sg_FilterOptionsCPU0.m_MipAnglePerLevelScale,
      sg_FilterOptionsCPU0.m_FilterType,
      sg_FilterOptionsCPU0.m_FixupType,
      sg_FilterOptionsCPU0.m_FixupWidth,
      sg_FilterOptionsCPU0.m_bUseSolidAngle,
      sg_FilterOptionsCPU0.m_GlossScale,
      sg_FilterOptionsCPU0.m_GlossBias,
      sg_FilterOptionsCPU0.m_SampleCountGGX
      );

   return(CP_THREAD_COMPLETED);
}


//Filtering options for each thread
SThreadOptionsThread1 sg_FilterOptionsCPU1;

//Thread functions used to run filtering routines in the process for CPU1
// note that these functions can not be member functions since thread initiation
// must start from a global or static global function

//This thread entry point function is called by thread 1  which is initiated
// by thread 0
DWORD WINAPI ThreadProcCPU1Filter(LPVOID a_NothingToPassToThisFunction)
{   
   //filter subset of faces 
   sg_FilterOptionsCPU1.m_cmProc->FilterCubeSurfaces(
      sg_FilterOptionsCPU1.m_SrcCubeMap, 
      sg_FilterOptionsCPU1.m_DstCubeMap, 
      sg_FilterOptionsCPU1.m_FilterConeAngle, 
      sg_FilterOptionsCPU1.m_FilterType, 
      sg_FilterOptionsCPU1.m_bUseSolidAngle, 
      sg_FilterOptionsCPU1.m_FaceIdxStart,
      sg_FilterOptionsCPU1.m_FaceIdxEnd,
      sg_FilterOptionsCPU1.m_ThreadIdx 
      ); 
      
   return(CP_THREAD_COMPLETED);
}


//------------------------------------------------------------------------------
// D3D cube map face specification
//   mapping from 3D x,y,z cube map lookup coordinates 
//   to 2D within face u,v coordinates
//
//   --------------------> U direction 
//   |                   (within-face texture space)
//   |         _____
//   |        |     |
//   |        | +Y  |
//   |   _____|_____|_____ _____
//   |  |     |     |     |     |
//   |  | -X  | +Z  | +X  | -Z  |
//   |  |_____|_____|_____|_____|
//   |        |     |
//   |        | -Y  |
//   |        |_____|
//   |
//   v   V direction
//      (within-face texture space)
//------------------------------------------------------------------------------

//Information about neighbors and how texture coorrdinates change across faces 
//  in ORDER of left, right, top, bottom (e.g. edges corresponding to u=0, 
//  u=1, v=0, v=1 in the 2D coordinate system of the particular face.
//Note this currently assumes the D3D cube face ordering and orientation
CPCubeMapNeighbor sg_CubeNgh[6][4] =
{
    //XPOS face
    {{CP_FACE_Z_POS, CP_EDGE_RIGHT },
     {CP_FACE_Z_NEG, CP_EDGE_LEFT  },
     {CP_FACE_Y_POS, CP_EDGE_RIGHT },
     {CP_FACE_Y_NEG, CP_EDGE_RIGHT }},
    //XNEG face
    {{CP_FACE_Z_NEG, CP_EDGE_RIGHT },
     {CP_FACE_Z_POS, CP_EDGE_LEFT  },
     {CP_FACE_Y_POS, CP_EDGE_LEFT  },
     {CP_FACE_Y_NEG, CP_EDGE_LEFT  }},
    //YPOS face
    {{CP_FACE_X_NEG, CP_EDGE_TOP },
     {CP_FACE_X_POS, CP_EDGE_TOP },
     {CP_FACE_Z_NEG, CP_EDGE_TOP },
     {CP_FACE_Z_POS, CP_EDGE_TOP }},
    //YNEG face
    {{CP_FACE_X_NEG, CP_EDGE_BOTTOM},
     {CP_FACE_X_POS, CP_EDGE_BOTTOM},
     {CP_FACE_Z_POS, CP_EDGE_BOTTOM},
     {CP_FACE_Z_NEG, CP_EDGE_BOTTOM}},
    //ZPOS face
    {{CP_FACE_X_NEG, CP_EDGE_RIGHT  },
     {CP_FACE_X_POS, CP_EDGE_LEFT   },
     {CP_FACE_Y_POS, CP_EDGE_BOTTOM },
     {CP_FACE_Y_NEG, CP_EDGE_TOP    }},
    //ZNEG face
    {{CP_FACE_X_POS, CP_EDGE_RIGHT  },
     {CP_FACE_X_NEG, CP_EDGE_LEFT   },
     {CP_FACE_Y_POS, CP_EDGE_TOP    },
     {CP_FACE_Y_NEG, CP_EDGE_BOTTOM }}
};


//3x2 matrices that map cube map indexing vectors in 3d 
// (after face selection and divide through by the 
//  _ABSOLUTE VALUE_ of the max coord)
// into NVC space
//Note this currently assumes the D3D cube face ordering and orientation
#define CP_UDIR     0
#define CP_VDIR     1
#define CP_FACEAXIS 2

float32 sgFace2DMapping[6][3][3] = {
    //XPOS face
    {{ 0,  0, -1},   //u towards negative Z
     { 0, -1,  0},   //v towards negative Y
     {1,  0,  0}},  //pos X axis  
    //XNEG face
     {{0,  0,  1},   //u towards positive Z
      {0, -1,  0},   //v towards negative Y
      {-1,  0,  0}},  //neg X axis       
    //YPOS face
    {{1, 0, 0},     //u towards positive X
     {0, 0, 1},     //v towards positive Z
     {0, 1 , 0}},   //pos Y axis  
    //YNEG face
    {{1, 0, 0},     //u towards positive X
     {0, 0 , -1},   //v towards negative Z
     {0, -1 , 0}},  //neg Y axis  
    //ZPOS face
    {{1, 0, 0},     //u towards positive X
     {0, -1, 0},    //v towards negative Y
     {0, 0,  1}},   //pos Z axis  
    //ZNEG face
    {{-1, 0, 0},    //u towards negative X
     {0, -1, 0},    //v towards negative Y
     {0, 0, -1}},   //neg Z axis  
};


//The 12 edges of the cubemap, (entries are used to index into the neighbor table)
// this table is used to average over the edges.
int32 sg_CubeEdgeList[12][2] = {
   {CP_FACE_X_POS, CP_EDGE_LEFT},
   {CP_FACE_X_POS, CP_EDGE_RIGHT},
   {CP_FACE_X_POS, CP_EDGE_TOP},
   {CP_FACE_X_POS, CP_EDGE_BOTTOM},

   {CP_FACE_X_NEG, CP_EDGE_LEFT},
   {CP_FACE_X_NEG, CP_EDGE_RIGHT},
   {CP_FACE_X_NEG, CP_EDGE_TOP},
   {CP_FACE_X_NEG, CP_EDGE_BOTTOM},

   {CP_FACE_Z_POS, CP_EDGE_TOP},
   {CP_FACE_Z_POS, CP_EDGE_BOTTOM},
   {CP_FACE_Z_NEG, CP_EDGE_TOP},
   {CP_FACE_Z_NEG, CP_EDGE_BOTTOM}
};


//Information about which of the 8 cube corners are correspond to the 
//  the 4 corners in each cube face
//  the order is upper left, upper right, lower left, lower right
int32 sg_CubeCornerList[6][4] = {
   { CP_CORNER_PPP, CP_CORNER_PPN, CP_CORNER_PNP, CP_CORNER_PNN }, // XPOS face
   { CP_CORNER_NPN, CP_CORNER_NPP, CP_CORNER_NNN, CP_CORNER_NNP }, // XNEG face
   { CP_CORNER_NPN, CP_CORNER_PPN, CP_CORNER_NPP, CP_CORNER_PPP }, // YPOS face
   { CP_CORNER_NNP, CP_CORNER_PNP, CP_CORNER_NNN, CP_CORNER_PNN }, // YNEG face
   { CP_CORNER_NPP, CP_CORNER_PPP, CP_CORNER_NNP, CP_CORNER_PNP }, // ZPOS face
   { CP_CORNER_PPN, CP_CORNER_NPN, CP_CORNER_PNN, CP_CORNER_NNN }  // ZNEG face
};


//--------------------------------------------------------------------------------------
//Error handling for cube map processor
//  Pop up dialog box, and terminate application
//--------------------------------------------------------------------------------------
void CPFatalError(WCHAR *a_Msg)
{
   MessageBoxW(NULL, a_Msg, L"Error: Application Terminating", MB_OK);
   exit(EM_FATAL_ERROR);
}


//--------------------------------------------------------------------------------------
// Convert cubemap face texel coordinates and face idx to 3D vector
// note the U and V coords are integer coords and range from 0 to size-1
//  this routine can be used to generate a normalizer cube map
//--------------------------------------------------------------------------------------
void TexelCoordToVect(int32 a_FaceIdx, float32 a_U, float32 a_V, int32 a_Size, float32 *a_XYZ)
{
   float32 nvcU, nvcV;
   float32 tempVec[3];

   //scale up to [-1, 1] range (inclusive)
   nvcU = (2.0f * ((float32)a_U + 0.5f) / a_Size ) - 1.0f;
   nvcV = (2.0f * ((float32)a_V + 0.5f) / a_Size ) - 1.0f;
   
   //generate x,y,z vector (xform 2d NVC coord to 3D vector)
   //U contribution
   VM_SCALE3(a_XYZ, sgFace2DMapping[a_FaceIdx][CP_UDIR], nvcU);    
   //V contribution
   VM_SCALE3(tempVec, sgFace2DMapping[a_FaceIdx][CP_VDIR], nvcV);
   VM_ADD3(a_XYZ, tempVec, a_XYZ);
   //add face axis
   VM_ADD3(a_XYZ, sgFace2DMapping[a_FaceIdx][CP_FACEAXIS], a_XYZ);

   //normalize vector
   VM_NORM3(a_XYZ, a_XYZ);
}


//--------------------------------------------------------------------------------------
// Convert 3D vector to cubemap face texel coordinates and face idx 
// note the U and V coords are integer coords and range from 0 to size-1
//  this routine can be used to generate a normalizer cube map
//
// returns face IDX and texel coords
//--------------------------------------------------------------------------------------
void VectToTexelCoord(float32 *a_XYZ, int32 a_Size, int32 *a_FaceIdx, int32 *a_U, int32 *a_V )
{
   float32 nvcU, nvcV;
   float32 absXYZ[3];
   float32 maxCoord;
   float32 onFaceXYZ[3];
   int32   faceIdx;
   int32   u, v;

   //absolute value 3
   VM_ABS3(absXYZ, a_XYZ);

   if( (absXYZ[0] >= absXYZ[1]) && (absXYZ[0] >= absXYZ[2]) )
   {
      maxCoord = absXYZ[0];

      if(a_XYZ[0] >= 0) //face = XPOS
      {
         faceIdx = CP_FACE_X_POS;            
      }    
      else
      {
         faceIdx = CP_FACE_X_NEG;                    
      }
   }
   else if ( (absXYZ[1] >= absXYZ[0]) && (absXYZ[1] >= absXYZ[2]) )
   {
      maxCoord = absXYZ[1];

      if(a_XYZ[1] >= 0) //face = XPOS
      {
         faceIdx = CP_FACE_Y_POS;            
      }    
      else
      {
         faceIdx = CP_FACE_Y_NEG;                    
      }    
   }
   else  // if( (absXYZ[2] > absXYZ[0]) && (absXYZ[2] > absXYZ[1]) )
   {
      maxCoord = absXYZ[2];

      if(a_XYZ[2] >= 0) //face = XPOS
      {
         faceIdx = CP_FACE_Z_POS;            
      }    
      else
      {
         faceIdx = CP_FACE_Z_NEG;                    
      }    
   }

   //divide through by max coord so face vector lies on cube face
   VM_SCALE3(onFaceXYZ, a_XYZ, 1.0f/maxCoord);
   nvcU = VM_DOTPROD3(sgFace2DMapping[ faceIdx ][CP_UDIR], onFaceXYZ );
   nvcV = VM_DOTPROD3(sgFace2DMapping[ faceIdx ][CP_VDIR], onFaceXYZ );

   u = (int32)floor( a_Size * 0.5f * (nvcU + 1.0f) );
   v = (int32)floor( a_Size * 0.5f * (nvcV + 1.0f) );

   *a_FaceIdx = faceIdx;
   *a_U = u;
   *a_V = v;
}


//--------------------------------------------------------------------------------------
// gets texel ptr in a cube map given a direction vector, and an array of 
//  CImageSurfaces that represent the cube faces.
//   
//--------------------------------------------------------------------------------------
CP_ITYPE *GetCubeMapTexelPtr(float32 *a_XYZ, CImageSurface *a_Surface)
{
   int32 u, v, faceIdx;    

   //get face idx and u, v texel coordinate in face
   VectToTexelCoord(a_XYZ, a_Surface[0].m_Width, &faceIdx, &u, &v );

   u = VM_MIN(u, a_Surface[0].m_Width - 1);
   v = VM_MIN(v, a_Surface[0].m_Width - 1);

   return( a_Surface[faceIdx].GetSurfaceTexelPtr(u, v) );
}


//--------------------------------------------------------------------------------------
//  Compute solid angle of given texel in cubemap face for weighting taps in the 
//   kernel by the area they project to on the unit sphere.
//
//  Note that this code uses an approximation to the solid angle, by treating the 
//   two triangles that make up the quad comprising the texel as planar.  If more
//   accuracy is required, the solid angle per triangle lying on the sphere can be 
//   computed using the sum of the interior angles - PI.
//
//--------------------------------------------------------------------------------------
float32 TexelCoordSolidAngle(int32 a_FaceIdx, float32 a_U, float32 a_V, int32 a_Size)
{
   float32 cornerVect[4][3];
   float64 cornerVect64[4][3];

   float32 halfTexelStep = 0.5f;  //note u, and v are in texel coords (where each texel is one unit)  
   float64 edgeVect0[3];
   float64 edgeVect1[3];
   float64 xProdVect[3];
   float64 texelArea;

   //compute 4 corner vectors of texel
   TexelCoordToVect(a_FaceIdx, a_U - halfTexelStep, a_V - halfTexelStep, a_Size, cornerVect[0] );
   TexelCoordToVect(a_FaceIdx, a_U - halfTexelStep, a_V + halfTexelStep, a_Size, cornerVect[1] );
   TexelCoordToVect(a_FaceIdx, a_U + halfTexelStep, a_V - halfTexelStep, a_Size, cornerVect[2] );
   TexelCoordToVect(a_FaceIdx, a_U + halfTexelStep, a_V + halfTexelStep, a_Size, cornerVect[3] );
   
   VM_NORM3_UNTYPED(cornerVect64[0], cornerVect[0] );
   VM_NORM3_UNTYPED(cornerVect64[1], cornerVect[1] );
   VM_NORM3_UNTYPED(cornerVect64[2], cornerVect[2] );
   VM_NORM3_UNTYPED(cornerVect64[3], cornerVect[3] );

   //area of triangle defined by corners 0, 1, and 2
   VM_SUB3_UNTYPED(edgeVect0, cornerVect64[1], cornerVect64[0] );
   VM_SUB3_UNTYPED(edgeVect1, cornerVect64[2], cornerVect64[0] );    
   VM_XPROD3_UNTYPED(xProdVect, edgeVect0, edgeVect1 );
   texelArea = 0.5f * sqrt( VM_DOTPROD3_UNTYPED(xProdVect, xProdVect ) );

   //area of triangle defined by corners 1, 2, and 3
   VM_SUB3_UNTYPED(edgeVect0, cornerVect64[2], cornerVect64[1] );
   VM_SUB3_UNTYPED(edgeVect1, cornerVect64[3], cornerVect64[1] );
   VM_XPROD3_UNTYPED(xProdVect, edgeVect0, edgeVect1 );
   texelArea += 0.5f * sqrt( VM_DOTPROD3_UNTYPED(xProdVect, xProdVect ) );

   return texelArea;
}



//--------------------------------------------------------------------------------------
//Builds a normalizer cubemap
//
// Takes in a cube face size, and an array of 6 surfaces to write the cube faces into
//
// Note that this normalizer cube map stores the vectors in unbiased -1 to 1 range.
//  if _bx2 style scaled and biased vectors are needed, uncomment the SCALE and BIAS
//  below
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::BuildNormalizerCubemap(int32 a_Size, CImageSurface *a_Surface )
{
   int32 iCubeFace, u, v;

   //iterate over cube faces
   for(iCubeFace=0; iCubeFace<6; iCubeFace++)
   {
      a_Surface[iCubeFace].Clear();
      a_Surface[iCubeFace].Init(a_Size, a_Size, 3);

      //fast texture walk, build normalizer cube map
      CP_ITYPE *texelPtr = a_Surface[iCubeFace].m_ImgData;

      for(v=0; v < a_Surface[iCubeFace].m_Height; v++)
      {
         for(u=0; u < a_Surface[iCubeFace].m_Width; u++)
         {
            TexelCoordToVect(iCubeFace, (float32)u, (float32)v, a_Size, texelPtr);

            //VM_SCALE3(texelPtr, texelPtr, 0.5f);
            //VM_BIAS3(texelPtr, texelPtr, 0.5f);

            texelPtr += a_Surface[iCubeFace].m_NumChannels;
         }         
      }
   }
}


//--------------------------------------------------------------------------------------
//Builds a normalizer cubemap, with the texels solid angle stored in the fourth component
//
//Takes in a cube face size, and an array of 6 surfaces to write the cube faces into
//
//Note that this normalizer cube map stores the vectors in unbiased -1 to 1 range.
// if _bx2 style scaled and biased vectors are needed, uncomment the SCALE and BIAS
// below
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::BuildNormalizerSolidAngleCubemap(int32 a_Size, CImageSurface *a_Surface )
{
   //iterate over cube faces
   for(int32 iCubeFace=0; iCubeFace<6; iCubeFace++)
   {
      a_Surface[iCubeFace].Clear();
      a_Surface[iCubeFace].Init(a_Size, a_Size, 4);  //First three channels for norm cube, and last channel for solid angle
   }

    //iterate over cube faces
#ifdef PROCESS_IN_PARALLEL
   // thread-workload is one face's line, gives plenty of concurrent occupation
   Concurrency::parallel_for(0 * a_Size, 6 * a_Size, 1, [&](int32 iCubeFaceHeight)
   {
      const int32 height = a_Size;
      const int32 width  = a_Size;

      //iterate over dst cube map face texel
      int32 iCubeFace = iCubeFaceHeight / a_Size;
      int32 v = iCubeFaceHeight % a_Size;

      {
#else
   for(int32 iCubeFace=0; iCubeFace<6; iCubeFace++)
   {
      const int32 height = a_Surface[iCubeFace].m_Height;
      const int32 width  = a_Surface[iCubeFace].m_Width;

      for(int32 v=0; v<height; v++)
      {
#endif
         //fast texture walk, build normalizer cube map
         CP_ITYPE *texelPtr = a_Surface[iCubeFace].m_ImgData + v * width * a_Surface[iCubeFace].m_NumChannels;

         for(int32 u=0; u<width; u++)
         {
            TexelCoordToVect(iCubeFace, (float32)u, (float32)v, a_Size, texelPtr);
            //VM_SCALE3(texelPtr, texelPtr, 0.5f);
            //VM_BIAS3(texelPtr, texelPtr, 0.5f);

            *(texelPtr + 3) = TexelCoordSolidAngle(iCubeFace, (float32)u, (float32)v, a_Size);

            texelPtr += a_Surface[iCubeFace].m_NumChannels;
         }         
      }
   }
#ifdef PROCESS_IN_PARALLEL
   );
#endif
}


//--------------------------------------------------------------------------------------
//Clear filter extents for the 6 cube map faces
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::ClearFilterExtents(CBBoxInt32 *aFilterExtents)
{
   int32 iCubeFaces;

   for(iCubeFaces=0; iCubeFaces<6; iCubeFaces++)
   {
      aFilterExtents[iCubeFaces].Clear();    
   }
}


//--------------------------------------------------------------------------------------
//Define per-face bounding box filter extents
//
// These define conservative texel regions in each of the faces the filter can possibly 
// process.  When the pixels in the regions are actually processed, the dot product  
// between the tap vector and the center tap vector is used to determine the weight of 
// the tap and whether or not the tap is within the cone.
//
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::DetermineFilterExtents(float32 *a_CenterTapDir, int32 a_SrcSize, int32 a_BBoxSize, 
                                               CBBoxInt32 *a_FilterExtents )
{
   int32 u, v;
   int32 faceIdx;
   int32 minU, minV, maxU, maxV;
   int32 i;

   //neighboring face and bleed over amount, and width of BBOX for
   // left, right, top, and bottom edges of this face
   int32 bleedOverAmount[4];
   int32 bleedOverBBoxMin[4];
   int32 bleedOverBBoxMax[4];

   int32 neighborFace;
   int32 neighborEdge;

   //get face idx, and u, v info from center tap dir
   VectToTexelCoord(a_CenterTapDir, a_SrcSize, &faceIdx, &u, &v );

   //define bbox size within face
   a_FilterExtents[faceIdx].Augment(u - a_BBoxSize, v - a_BBoxSize, 0);
   a_FilterExtents[faceIdx].Augment(u + a_BBoxSize, v + a_BBoxSize, 0);

   a_FilterExtents[faceIdx].ClampMin(0, 0, 0);
   a_FilterExtents[faceIdx].ClampMax(a_SrcSize-1, a_SrcSize-1, 0);

   //u and v extent in face corresponding to center tap
   minU = a_FilterExtents[faceIdx].m_minCoord[0];
   minV = a_FilterExtents[faceIdx].m_minCoord[1];
   maxU = a_FilterExtents[faceIdx].m_maxCoord[0];
   maxV = a_FilterExtents[faceIdx].m_maxCoord[1];

   //bleed over amounts for face across u=0 edge (left)    
   bleedOverAmount[0] = (a_BBoxSize - u);
   bleedOverBBoxMin[0] = minV;
   bleedOverBBoxMax[0] = maxV;

   //bleed over amounts for face across u=1 edge (right)    
   bleedOverAmount[1] = (u + a_BBoxSize) - (a_SrcSize-1);
   bleedOverBBoxMin[1] = minV;
   bleedOverBBoxMax[1] = maxV;

   //bleed over to face across v=0 edge (up)
   bleedOverAmount[2] = (a_BBoxSize - v);
   bleedOverBBoxMin[2] = minU;
   bleedOverBBoxMax[2] = maxU;

   //bleed over to face across v=1 edge (down)
   bleedOverAmount[3] = (v + a_BBoxSize) - (a_SrcSize-1);
   bleedOverBBoxMin[3] = minU;
   bleedOverBBoxMax[3] = maxU;

   //compute bleed over regions in neighboring faces
   for(i=0; i<4; i++)
   {
      if(bleedOverAmount[i] > 0)
      {
         neighborFace = sg_CubeNgh[faceIdx][i].m_Face;
         neighborEdge = sg_CubeNgh[faceIdx][i].m_Edge;

         //For certain types of edge abutments, the bleedOverBBoxMin, and bleedOverBBoxMax need to 
         //  be flipped: the cases are 
         // if a left   edge mates with a left or bottom  edge on the neighbor
         // if a top    edge mates with a top or right edge on the neighbor
         // if a right  edge mates with a right or top edge on the neighbor
         // if a bottom edge mates with a bottom or left  edge on the neighbor
         //Seeing as the edges are enumerated as follows 
         // left   =0 
         // right  =1 
         // top    =2 
         // bottom =3            
         // 
         // so if the edge enums are the same, or the sum of the enums == 3, 
         //  the bbox needs to be flipped
         if( (i == neighborEdge) || ((i+neighborEdge) == 3) )
         {
            bleedOverBBoxMin[i] = (a_SrcSize-1) - bleedOverBBoxMin[i];
            bleedOverBBoxMax[i] = (a_SrcSize-1) - bleedOverBBoxMax[i];
         }


         //The way the bounding box is extended onto the neighboring face
         // depends on which edge of neighboring face abuts with this one
         switch(sg_CubeNgh[faceIdx][i].m_Edge)
         {
            case CP_EDGE_LEFT:
               a_FilterExtents[neighborFace].Augment(0, bleedOverBBoxMin[i], 0);
               a_FilterExtents[neighborFace].Augment(bleedOverAmount[i], bleedOverBBoxMax[i], 0);
            break;
            case CP_EDGE_RIGHT:                
               a_FilterExtents[neighborFace].Augment( (a_SrcSize-1), bleedOverBBoxMin[i], 0);
               a_FilterExtents[neighborFace].Augment( (a_SrcSize-1) - bleedOverAmount[i], bleedOverBBoxMax[i], 0);
            break;
            case CP_EDGE_TOP:   
               a_FilterExtents[neighborFace].Augment(bleedOverBBoxMin[i], 0, 0);
               a_FilterExtents[neighborFace].Augment(bleedOverBBoxMax[i], bleedOverAmount[i], 0);
            break;
            case CP_EDGE_BOTTOM:   
               a_FilterExtents[neighborFace].Augment(bleedOverBBoxMin[i], (a_SrcSize-1), 0);
               a_FilterExtents[neighborFace].Augment(bleedOverBBoxMax[i], (a_SrcSize-1) - bleedOverAmount[i], 0);            
            break;
         }

         //clamp filter extents in non-center tap faces to remain within surface
         a_FilterExtents[neighborFace].ClampMin(0, 0, 0);
         a_FilterExtents[neighborFace].ClampMax(a_SrcSize-1, a_SrcSize-1, 0);
      }

      //If the bleed over amount bleeds past the adjacent face onto the opposite face 
      // from the center tap face, then process the opposite face entirely for now. 
      //Note that the cases in which this happens, what usually happens is that 
      // more than one edge bleeds onto the opposite face, and the bounding box 
      // encompasses the entire cube map face.
      if(bleedOverAmount[i] > a_SrcSize)
      {
         uint32 oppositeFaceIdx; 

         //determine opposite face 
         switch(faceIdx)
         {
            case CP_FACE_X_POS:
               oppositeFaceIdx = CP_FACE_X_NEG;
            break;
            case CP_FACE_X_NEG:
               oppositeFaceIdx = CP_FACE_X_POS;
            break;
            case CP_FACE_Y_POS:
               oppositeFaceIdx = CP_FACE_Y_NEG;
            break;
            case CP_FACE_Y_NEG:
               oppositeFaceIdx = CP_FACE_Y_POS;
            break;
            case CP_FACE_Z_POS:
               oppositeFaceIdx = CP_FACE_Z_NEG;
							 break;
						default: // CP_FACE_Z_NEG:
               oppositeFaceIdx = CP_FACE_Z_POS;
            break;
         }
   
         //just encompass entire face for now
         a_FilterExtents[oppositeFaceIdx].Augment(0, 0, 0);
         a_FilterExtents[oppositeFaceIdx].Augment((a_SrcSize-1), (a_SrcSize-1), 0);            
      }
   }

   minV=minV;
}


//--------------------------------------------------------------------------------------
//ProcessFilterExtents 
//  Process bounding box in each cube face 
//
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::ProcessFilterExtents(float32 *a_CenterTapDir, float32 a_DotProdThresh, 
    CBBoxInt32 *a_FilterExtents, CImageSurface *a_NormCubeMap, CImageSurface *a_SrcCubeMap, 
    CP_ITYPE *a_DstVal, uint32 a_FilterType, bool8 a_bUseSolidAngleWeighting, float32 a_SpecularPower)
{
   //accumulators are 64-bit floats in order to have the precision needed 
   // over a summation of a large number of pixels 
   float64 dstAccumFace[6][4];
   float64 weightAccumFace[6];

   const int32 nSrcChannels = a_SrcCubeMap[0].m_NumChannels;

   //norm cube map and srcCubeMap have same face width
   const int32 faceWidth = a_NormCubeMap[0].m_Width;

   //amount to add to pointer to move to next scanline in images
   const int32 normCubePitch = faceWidth * a_NormCubeMap[0].m_NumChannels;
   const int32 srcCubePitch = faceWidth * a_SrcCubeMap[0].m_NumChannels;

   //iterate over cubefaces
   for(int32 iFaceIdx=0; iFaceIdx<6; iFaceIdx++ )
   {
      //dest accum
      for(int32 k=0; k<m_NumChannels; k++)
      {
        dstAccumFace[iFaceIdx][k] = 0.0f;
      }

      weightAccumFace[iFaceIdx] = 0.0f;

      //if bbox is non empty
      if(a_FilterExtents[iFaceIdx].Empty() == FALSE) 
      {
         //pointers used to walk across the image surface to accumulate taps
         CP_ITYPE *normCubeRowStartPtr;
         CP_ITYPE *srcCubeRowStartPtr;

         int32 uStart, uEnd;
         int32 vStart, vEnd;

         uStart = a_FilterExtents[iFaceIdx].m_minCoord[0];
         vStart = a_FilterExtents[iFaceIdx].m_minCoord[1];
         uEnd = a_FilterExtents[iFaceIdx].m_maxCoord[0];
         vEnd = a_FilterExtents[iFaceIdx].m_maxCoord[1];

         normCubeRowStartPtr = a_NormCubeMap[iFaceIdx].m_ImgData + (a_NormCubeMap[iFaceIdx].m_NumChannels * 
            ((vStart * faceWidth) + uStart) );

         srcCubeRowStartPtr = a_SrcCubeMap[iFaceIdx].m_ImgData + (a_SrcCubeMap[iFaceIdx].m_NumChannels * 
            ((vStart * faceWidth) + uStart) );

         //note that <= is used to ensure filter extents always encompass at least one pixel if bbox is non empty
         for(int32 v = vStart; v <= vEnd; v++)
         {
            int32 normCubeRowWalk;
            int32 srcCubeRowWalk;

            normCubeRowWalk = 0;
            srcCubeRowWalk = 0;

            for(int32 u = uStart; u <= uEnd; u++)
            {
               //pointers used to walk across the image surface to accumulate taps
               CP_ITYPE *texelVect;

               CP_ITYPE tapDotProd;   //dot product between center tap and current tap

               //pointer to direction in cube map associated with texel
               texelVect = (normCubeRowStartPtr + normCubeRowWalk);

               //check dot product to see if texel is within cone
               tapDotProd = VM_DOTPROD3(texelVect, a_CenterTapDir);

               if( tapDotProd >= a_DotProdThresh )
               {
                  CP_ITYPE weight;

                  //for now just weight all taps equally, but ideally
                  // weight should be proportional to the solid angle of the tap
                  if(a_bUseSolidAngleWeighting == TRUE)
                  {   //solid angle stored in 4th channel of normalizer/solid angle cube map
                     weight = *(texelVect+3); 
                  }
                  else
                  {   //all taps equally weighted
                     weight = 1.0f;          
                  }

                  switch(a_FilterType)
                  {
                  case CP_FILTER_TYPE_COSINE_POWER:
                     {
                        if(tapDotProd > 0.0f)
                        {
                           weight *= pow(tapDotProd, a_SpecularPower) * tapDotProd;
                        }
                        else
                        {
                           weight = 0;
                        }
                     }
                     break;
                  case CP_FILTER_TYPE_CONE:                                
                  case CP_FILTER_TYPE_ANGULAR_GAUSSIAN:
                     {
                        //weights are in same lookup table for both of these filter types
                        weight *= m_FilterLUT[(int32)(tapDotProd * (m_NumFilterLUTEntries - 1))];
                     }
                     break;
                  case CP_FILTER_TYPE_COSINE:
                     {
                        if(tapDotProd > 0.0f)
                        {
                           weight *= tapDotProd;
                        }
                        else
                        {
                           weight = 0.0f;
                        }
                     }
                     break;
                  case CP_FILTER_TYPE_DISC:
                  default:
                     break;
                  }

                  //iterate over channels
                  for(int32 k=0; k<nSrcChannels; k++)   //(aSrcCubeMap[iFaceIdx].m_NumChannels) //up to 4 channels 
                  {
                     dstAccumFace[iFaceIdx][k] += weight * *(srcCubeRowStartPtr + srcCubeRowWalk);
                     srcCubeRowWalk++;                            
                  } 

                  weightAccumFace[iFaceIdx] += weight; //accumulate weight
               }
               else
               {   
                  //step across source pixel
                  srcCubeRowWalk += nSrcChannels;                    
               }

               normCubeRowWalk += a_NormCubeMap[iFaceIdx].m_NumChannels;
            }

            normCubeRowStartPtr += normCubePitch;
            srcCubeRowStartPtr += srcCubePitch;
         }       
      }
   }

   // reduction to 1 value from 6 faces
   float64 dstAccum[4];
   float64 weightAccum;

   //dest accum
   for(int32 k=0; k<m_NumChannels; k++)
   {
	   dstAccum[k] = 0.0f;
   }

   weightAccum = 0.0f;

   for(int32 iFaceIdx=0; iFaceIdx<6; iFaceIdx++ )
   {
     //dest accum
     for(int32 k=0; k<m_NumChannels; k++)
     {
       dstAccum[k] += dstAccumFace[iFaceIdx][k];
     }
  
     weightAccum += weightAccumFace[iFaceIdx];
   }

   //divide through by weights if weight is non zero
   if(weightAccum != 0.0f)
   {
      for(int32 k=0; k<m_NumChannels; k++)
      {
         a_DstVal[k] = (float32)(dstAccum[k] / weightAccum);
      }
   }
   else
   {   //otherwise sample nearest
      CP_ITYPE *texelPtr;

      texelPtr = GetCubeMapTexelPtr(a_CenterTapDir, a_SrcCubeMap);

      for(int32 k=0; k<m_NumChannels; k++)
      {
         a_DstVal[k] = texelPtr[k];
      }
   }
}


//--------------------------------------------------------------------------------------
// Fixup cube edges
//
// average texels on cube map faces across the edges
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::FixupCubeEdges(CImageSurface *a_CubeMap, int32 a_FixupType, int32 a_FixupWidth)
{
   int32 i, j, k;
   int32 face;
   int32 edge;
   int32 neighborFace;
   int32 neighborEdge;

   int32 nChannels = a_CubeMap[0].m_NumChannels;
   int32 size = a_CubeMap[0].m_Width;

   CPCubeMapNeighbor neighborInfo;

   CP_ITYPE* edgeStartPtr;
   CP_ITYPE* neighborEdgeStartPtr;

   int32 edgeWalk;
   int32 neighborEdgeWalk;

   //pointer walk to walk one texel away from edge in perpendicular direction
   int32 edgePerpWalk;
   int32 neighborEdgePerpWalk;

   //number of texels inward towards cubeface center to apply fixup to
   int32 fixupDist;
   int32 iFixup;   

   // note that if functionality to filter across the three texels for each corner, then 
   CP_ITYPE *cornerPtr[8][3];      //indexed by corner and face idx
   CP_ITYPE *faceCornerPtrs[4];    //corner pointers for face
   int32 cornerNumPtrs[8];         //indexed by corner and face idx
   int32 iCorner;                  //corner iterator
   int32 iFace;                    //iterator for faces
   int32 corner;

   //if there is no fixup, or fixup width = 0, do nothing
   if((a_FixupType == CP_FIXUP_NONE) ||
      (a_FixupWidth == 0)  )
   {
      return;
   }

   //special case 1x1 cubemap, average face colors
   if( a_CubeMap[0].m_Width == 1 )
   {
      //iterate over channels
      for(k=0; k<nChannels; k++)
      {   
         CP_ITYPE accum = 0.0f;

         //iterate over faces to accumulate face colors
         for(iFace=0; iFace<6; iFace++)
         {
            accum += *(a_CubeMap[iFace].m_ImgData + k);
         }

         //compute average over 6 face colors
         accum /= 6.0f;

         //iterate over faces to distribute face colors
         for(iFace=0; iFace<6; iFace++)
         {
            *(a_CubeMap[iFace].m_ImgData + k) = accum;
         }
      }

      return;
   }


   //iterate over corners
   for(iCorner = 0; iCorner < 8; iCorner++ )
   {
      cornerNumPtrs[iCorner] = 0;
   }

   //iterate over faces to collect list of corner texel pointers
   for(iFace=0; iFace<6; iFace++ )
   {
      //the 4 corner pointers for this face
      faceCornerPtrs[0] = a_CubeMap[iFace].m_ImgData;
      faceCornerPtrs[1] = a_CubeMap[iFace].m_ImgData + ( (size - 1) * nChannels );
      faceCornerPtrs[2] = a_CubeMap[iFace].m_ImgData + ( (size) * (size - 1) * nChannels );
      faceCornerPtrs[3] = a_CubeMap[iFace].m_ImgData + ( (((size) * (size - 1)) + (size - 1)) * nChannels );

      //iterate over face corners to collect cube corner pointers
      for(i=0; i<4; i++ )
      {
         corner = sg_CubeCornerList[iFace][i];   
         cornerPtr[corner][ cornerNumPtrs[corner] ] = faceCornerPtrs[i];
         cornerNumPtrs[corner]++;
      }
   }


   //iterate over corners to average across corner tap values
   for(iCorner = 0; iCorner < 8; iCorner++ )
   {
      for(k=0; k<nChannels; k++)
      {             
         CP_ITYPE cornerTapAccum;

         cornerTapAccum = 0.0f;

         //iterate over corner texels and average results
         for(i=0; i<3; i++ )
         {
            cornerTapAccum += *(cornerPtr[iCorner][i] + k);
         }

         //divide by 3 to compute average of corner tap values
         cornerTapAccum *= (1.0f / 3.0f);

         //iterate over corner texels and average results
         for(i=0; i<3; i++ )
         {
            *(cornerPtr[iCorner][i] + k) = cornerTapAccum;
         }
      }
   }   


   //maximum width of fixup region is one half of the cube face size
   fixupDist = VM_MIN( a_FixupWidth, size / 2);

   //iterate over the twelve edges of the cube to average across edges
   for(i=0; i<12; i++)
   {
      face = sg_CubeEdgeList[i][0];
      edge = sg_CubeEdgeList[i][1];

      neighborInfo = sg_CubeNgh[face][edge];
      neighborFace = neighborInfo.m_Face;
      neighborEdge = neighborInfo.m_Edge;

      edgeStartPtr = a_CubeMap[face].m_ImgData;
      neighborEdgeStartPtr = a_CubeMap[neighborFace].m_ImgData;
      edgeWalk = 0;
      neighborEdgeWalk = 0;

      //amount to pointer to sample taps away from cube face
      edgePerpWalk = 0;
      neighborEdgePerpWalk = 0;

      //Determine walking pointers based on edge type
      // e.g. CP_EDGE_LEFT, CP_EDGE_RIGHT, CP_EDGE_TOP, CP_EDGE_BOTTOM
      switch(edge)
      {
         case CP_EDGE_LEFT:
            // no change to faceEdgeStartPtr  
            edgeWalk = nChannels * size;
            edgePerpWalk = nChannels;
         break;
         case CP_EDGE_RIGHT:
            edgeStartPtr += (size - 1) * nChannels;
            edgeWalk = nChannels * size;
            edgePerpWalk = -nChannels;
         break;
         case CP_EDGE_TOP:
            // no change to faceEdgeStartPtr  
            edgeWalk = nChannels;
            edgePerpWalk = nChannels * size;
         break;
         case CP_EDGE_BOTTOM:
            edgeStartPtr += (size) * (size - 1) * nChannels;
            edgeWalk = nChannels;
            edgePerpWalk = -(nChannels * size);
         break;
      }

      //For certain types of edge abutments, the neighbor edge walk needs to 
      //  be flipped: the cases are 
      // if a left   edge mates with a left or bottom  edge on the neighbor
      // if a top    edge mates with a top or right edge on the neighbor
      // if a right  edge mates with a right or top edge on the neighbor
      // if a bottom edge mates with a bottom or left  edge on the neighbor
      //Seeing as the edges are enumerated as follows 
      // left   =0 
      // right  =1 
      // top    =2 
      // bottom =3            
      // 
      //If the edge enums are the same, or the sum of the enums == 3, 
      //  the neighbor edge walk needs to be flipped
      if( (edge == neighborEdge) || ((edge + neighborEdge) == 3) )
      {   //swapped direction neighbor edge walk
         switch(neighborEdge)
         {
            case CP_EDGE_LEFT:  //start at lower left and walk up
               neighborEdgeStartPtr += (size - 1) * (size) *  nChannels;
               neighborEdgeWalk = -(nChannels * size);
               neighborEdgePerpWalk = nChannels;
            break;
            case CP_EDGE_RIGHT: //start at lower right and walk up
               neighborEdgeStartPtr += ((size - 1)*(size) + (size - 1)) * nChannels;
               neighborEdgeWalk = -(nChannels * size);
               neighborEdgePerpWalk = -nChannels;
            break;
            case CP_EDGE_TOP:   //start at upper right and walk left
               neighborEdgeStartPtr += (size - 1) * nChannels;
               neighborEdgeWalk = -nChannels;
               neighborEdgePerpWalk = (nChannels * size);
            break;
            case CP_EDGE_BOTTOM: //start at lower right and walk left
               neighborEdgeStartPtr += ((size - 1)*(size) + (size - 1)) * nChannels;
               neighborEdgeWalk = -nChannels;
               neighborEdgePerpWalk = -(nChannels * size);
            break;
         }            
      }
      else
      { //swapped direction neighbor edge walk
         switch(neighborEdge)
         {
            case CP_EDGE_LEFT: //start at upper left and walk down
               //no change to neighborEdgeStartPtr for this case since it points 
               // to the upper left corner already
               neighborEdgeWalk = nChannels * size;
               neighborEdgePerpWalk = nChannels;
            break;
            case CP_EDGE_RIGHT: //start at upper right and walk down
               neighborEdgeStartPtr += (size - 1) * nChannels;
               neighborEdgeWalk = nChannels * size;
               neighborEdgePerpWalk = -nChannels;
            break;
            case CP_EDGE_TOP:   //start at upper left and walk left
               //no change to neighborEdgeStartPtr for this case since it points 
               // to the upper left corner already
               neighborEdgeWalk = nChannels;
               neighborEdgePerpWalk = (nChannels * size);
            break;
            case CP_EDGE_BOTTOM: //start at lower left and walk left
               neighborEdgeStartPtr += (size) * (size - 1) * nChannels;
               neighborEdgeWalk = nChannels;
               neighborEdgePerpWalk = -(nChannels * size);
            break;
         }
      }


      //Perform edge walk, to average across the 12 edges and smoothly propagate change to 
      //nearby neighborhood

      //step ahead one texel on edge
      edgeStartPtr += edgeWalk;
      neighborEdgeStartPtr += neighborEdgeWalk;

      // note that this loop does not process the corner texels, since they have already been
      //  averaged across faces across earlier
      for(j=1; j<(size - 1); j++)       
      {             
         //for each set of taps along edge, average them
         // and rewrite the results into the edges
         for(k = 0; k<nChannels; k++)
         {             
            CP_ITYPE edgeTap, neighborEdgeTap, avgTap;  //edge tap, neighborEdgeTap and the average of the two
            CP_ITYPE edgeTapDev, neighborEdgeTapDev;

            edgeTap = *(edgeStartPtr + k);
            neighborEdgeTap = *(neighborEdgeStartPtr + k);

            //compute average of tap intensity values
            avgTap = 0.5f * (edgeTap + neighborEdgeTap);

            //propagate average of taps to edge taps
            (*(edgeStartPtr + k)) = avgTap;
            (*(neighborEdgeStartPtr + k)) = avgTap;

            edgeTapDev = edgeTap - avgTap;
            neighborEdgeTapDev = neighborEdgeTap - avgTap;

            //iterate over taps in direction perpendicular to edge, and 
            //  adjust intensity values gradualy to obscure change in intensity values of 
            //  edge averaging.
            for(iFixup = 1; iFixup < fixupDist; iFixup++)
            {
               //fractional amount to apply change in tap intensity along edge to taps 
               //  in a perpendicular direction to edge 
               CP_ITYPE fixupFrac = (CP_ITYPE)(fixupDist - iFixup) / (CP_ITYPE)(fixupDist); 
               CP_ITYPE fixupWeight;

               switch(a_FixupType )
               {
                  case CP_FIXUP_PULL_LINEAR:
                  {
                     fixupWeight = fixupFrac;
                  }
                  break;
                  case CP_FIXUP_PULL_HERMITE:
                  {
                     //hermite spline interpolation between 1 and 0 with both pts derivatives = 0 
                     // e.g. smooth step
                     // the full formula for hermite interpolation is:
                     //              
                     //                  [  2  -2   1   1 ][ p0 ] 
                     // [t^3  t^2  t  1 ][ -3   3  -2  -1 ][ p1 ]
                     //                  [  0   0   1   0 ][ d0 ]
                     //                  [  1   0   0   0 ][ d1 ]
                     // 
                     // Where p0 and p1 are the point locations and d0, and d1 are their respective derivatives
                     // t is the parameteric coordinate used to specify an interpoltion point on the spline
                     // and ranges from 0 to 1.
                     //  if p0 = 0 and p1 = 1, and d0 and d1 = 0, the interpolation reduces to
                     //
                     //  p(t) =  - 2t^3 + 3t^2
                     fixupWeight = ((-2.0 * fixupFrac + 3.0) * fixupFrac * fixupFrac);
                  }
                  break;
                  case CP_FIXUP_AVERAGE_LINEAR:
                  {
                     fixupWeight = fixupFrac;

                     //perform weighted average of edge tap value and current tap
                     // fade off weight linearly as a function of distance from edge
                     edgeTapDev = 
                        (*(edgeStartPtr + (iFixup * edgePerpWalk) + k)) - avgTap;
                     neighborEdgeTapDev = 
                        (*(neighborEdgeStartPtr + (iFixup * neighborEdgePerpWalk) + k)) - avgTap;
                  }
                  break;
                  case CP_FIXUP_AVERAGE_HERMITE:
                  {
                     fixupWeight = ((-2.0 * fixupFrac + 3.0) * fixupFrac * fixupFrac);

                     //perform weighted average of edge tap value and current tap
                     // fade off weight using hermite spline with distance from edge
                     //  as parametric coordinate
                     edgeTapDev = 
                        (*(edgeStartPtr + (iFixup * edgePerpWalk) + k)) - avgTap;
                     neighborEdgeTapDev = 
                        (*(neighborEdgeStartPtr + (iFixup * neighborEdgePerpWalk) + k)) - avgTap;
                  }
                  break;
               }

               // vary intensity of taps within fixup region toward edge values to hide changes made to edge taps
               *(edgeStartPtr + (iFixup * edgePerpWalk) + k) -= (fixupWeight * edgeTapDev);
               *(neighborEdgeStartPtr + (iFixup * neighborEdgePerpWalk) + k) -= (fixupWeight * neighborEdgeTapDev);
            }

         }

         edgeStartPtr += edgeWalk;
         neighborEdgeStartPtr += neighborEdgeWalk;
      }        
   }
}


//--------------------------------------------------------------------------------------
//Constructor
//--------------------------------------------------------------------------------------
CCubeMapProcessor::CCubeMapProcessor(void)
{
   int32 i;

   //If zero filtering threads are specified then all filtering is performed in the 
   // process that called the cubemap filtering routines.
   //Otherwise, the filtering is performed in separate filtering threads that cubemap generates
   m_NumFilterThreads = CP_INITIAL_NUM_FILTER_THREADS;

   //clear all threads
   for(i=0; i<CP_MAX_FILTER_THREADS; i++ )
   {
      m_bThreadInitialized[i] = FALSE;
      m_ThreadHandle[i] = NULL;
      m_ThreadID[0] = 0;
   }

   m_ProgressString[0] = 0;
   
   m_BaseFilterAngle = 0.0f;
   m_InitialMipAngle = 0.0f;
   m_MipAnglePerLevelScale = 0.0f;

   m_InputSize = 0;             
   m_OutputSize = 0;             
   m_NumMipLevels = 0;     
   m_NumChannels = 0; 

   m_NumFilterLUTEntries = 0;
   m_FilterLUT = NULL;

   //Constructors are automatically called for m_InputSurface and m_OutputSurface arrays
}


//--------------------------------------------------------------------------------------
//destructor
//--------------------------------------------------------------------------------------
CCubeMapProcessor::~CCubeMapProcessor()
{
    Clear();
}


//--------------------------------------------------------------------------------------
// Stop any currently running threads, and clear all allocated data from cube map 
//   processor.
//
// To use the cube map processor after calling Clear(....), you need to call Init(....) 
//   again
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::Clear(void)
{
   int32 i, j;

   TerminateActiveThreads();

   for(i=0; i<CP_MAX_FILTER_THREADS; i++ )
   {
      m_bThreadInitialized[i] = FALSE;
   }

   m_InputSize = 0;             
   m_OutputSize = 0;             
   m_NumMipLevels = 0;     
   m_NumChannels = 0; 

   //Iterate over faces for input images
   for(i=0; i<6; i++)
   {
      m_InputSurface[i].Clear();
   }

   //Iterate over mip chain, and allocate memory for mip-chain
   for(j=0; j<CP_MAX_MIPLEVELS; j++)
   {
      //Iterate over faces for output images
      for(i=0; i<6; i++)
      {
         m_OutputSurface[j][i].Clear();            
      }
   }

   m_NumFilterLUTEntries = 0;
   CP_SAFE_DELETE_ARRAY( m_FilterLUT );
}


//--------------------------------------------------------------------------------------
// Terminates execution of active threads
//
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::TerminateActiveThreads(void)
{
   int32 i;

   for(i=0; i<CP_MAX_FILTER_THREADS; i++)
   {
      if( m_bThreadInitialized[i] == TRUE)
      {
         if(m_ThreadHandle[i] != NULL)
         {
            TerminateThread(m_ThreadHandle[i], CP_THREAD_TERMINATED);
            CloseHandle(m_ThreadHandle[i]);
            m_ThreadHandle[i] = NULL;
            m_Status = CP_STATUS_FILTER_TERMINATED;
         }
      }
   }

}


//--------------------------------------------------------------------------------------
//Init cube map processor
//
// note that if a_MaxNumMipLevels is set to 0, the entire mip chain will be created
//  similar to the way the D3D tools allow you to specify 0 to denote an entire mip chain
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::Init(int32 a_InputSize, int32 a_OutputSize, int32 a_MaxNumMipLevels, int32 a_NumChannels)
{
    int32 i, j;
    int32 mipLevelSize;
    int32 maxNumMipLevels;

    m_Status = CP_STATUS_READY;

    //since input is being modified, terminate any active filtering threads
    TerminateActiveThreads();

    m_InputSize = a_InputSize;
    m_OutputSize = a_OutputSize;

    m_NumChannels = a_NumChannels;
        
    maxNumMipLevels = a_MaxNumMipLevels;

    //if nax num mip levels is set to 0, set it to generate the entire mip chain
    if(maxNumMipLevels == 0 )
    {
        maxNumMipLevels = CP_MAX_MIPLEVELS;
    }

    //Iterate over faces for input images
    for(i=0; i<6; i++)
    {
        m_InputSurface[i].Init(m_InputSize, m_InputSize, m_NumChannels );
    }

    //zero mip levels constructed so far
    m_NumMipLevels = 0;

    //first miplevel size 
    mipLevelSize = m_OutputSize;

    //Iterate over mip chain, and init CImageSurfaces for mip-chain
    for(j=0; j<a_MaxNumMipLevels; j++)
    {
        //Iterate over faces for output images
        for(i=0; i<6; i++)
        {
            m_OutputSurface[j][i].Init(mipLevelSize, mipLevelSize, a_NumChannels );            
        }

        //next mip level is half size
        mipLevelSize >>= 1;
        
        m_NumMipLevels++;

        //terminate if mip chain becomes too small
        if(mipLevelSize == 0)
        {            
            return;
        }
    }
}


//--------------------------------------------------------------------------------------
//Copy and convert cube map face data from an external image/surface into this object
//
// a_FaceIdx        = a value 0 to 5 speciying which face to copy into (one of the CP_FACE_? )
// a_Level          = mip level to copy into
// a_SrcType        = data type of image being copyed from (one of the CP_TYPE_? types)
// a_SrcNumChannels = number of channels of the image being copied from (usually 1 to 4)
// a_SrcPitch       = number of bytes per row of the source image being copied from
// a_SrcDataPtr     = pointer to the image data to copy from
// a_Degamma        = original gamma level of input image to undo by degamma
// a_Scale          = scale to apply to pixel values after degamma (in linear space)
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::SetInputFaceData(int32 a_FaceIdx, int32 a_SrcType, int32 a_SrcNumChannels, 
    int32 a_SrcPitch, void *a_SrcDataPtr, float32 a_MaxClamp, float32 a_Degamma, float32 a_Scale)
{
    //since input is being modified, terminate any active filtering threads
    TerminateActiveThreads();
  
    m_InputSurface[a_FaceIdx].SetImageDataClampDegammaScale( a_SrcType, a_SrcNumChannels, a_SrcPitch, 
       a_SrcDataPtr, a_MaxClamp, a_Degamma, a_Scale );
}


//--------------------------------------------------------------------------------------
//Copy and convert cube map face data from this object into an external image/surface
//
// a_FaceIdx        = a value 0 to 5 speciying which face to copy into (one of the CP_FACE_? )
// a_Level          = mip level to copy into
// a_DstType        = data type of image to copy to (one of the CP_TYPE_? types)
// a_DstNumChannels = number of channels of the image to copy to (usually 1 to 4)
// a_DstPitch       = number of bytes per row of the dest image to copy to
// a_DstDataPtr     = pointer to the image data to copy to
// a_Scale          = scale to apply to pixel values (in linear space) before gamma for output
// a_Gamma          = gamma level to apply to pixels after scaling
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::GetInputFaceData(int32 a_FaceIdx, int32 a_DstType, int32 a_DstNumChannels, 
    int32 a_DstPitch, void *a_DstDataPtr, float32 a_Scale, float32 a_Gamma)
{
    m_InputSurface[a_FaceIdx].GetImageDataScaleGamma( a_DstType, a_DstNumChannels, a_DstPitch, 
       a_DstDataPtr, a_Scale, a_Gamma );
}


//--------------------------------------------------------------------------------------
//ChannelSwapInputFaceData
//  swizzle data in first 4 channels for input faces
//
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::ChannelSwapInputFaceData(int32 a_Channel0Src, int32 a_Channel1Src, 
                                                 int32 a_Channel2Src, int32 a_Channel3Src )
{
   int32 iFace, u, v, k;
   int32 size;
   CP_ITYPE texelData[4];
   int32 channelSrcArray[4];

   //since input is being modified, terminate any active filtering threads
   TerminateActiveThreads();

   size = m_InputSize;

   channelSrcArray[0] = a_Channel0Src;
   channelSrcArray[1] = a_Channel1Src;
   channelSrcArray[2] = a_Channel2Src;
   channelSrcArray[3] = a_Channel3Src;

   //Iterate over faces for input images
   for(iFace=0; iFace<6; iFace++)
   {
      for(v=0; v<m_InputSize; v++ )
      {
         for(u=0; u<m_InputSize; u++ )
         {
            //get channel data
            for(k=0; k<m_NumChannels; k++)
            {
               texelData[k] = *(m_InputSurface[iFace].GetSurfaceTexelPtr(u, v) + k);
            }

            //repack channel data accoring to swizzle information
            for(k=0; k<m_NumChannels; k++)
            {
               *( m_InputSurface[iFace].GetSurfaceTexelPtr(u, v) + k) = 
                  texelData[ channelSrcArray[k] ];
            }
         }
      }
   }
}


//--------------------------------------------------------------------------------------
//ChannelSwapOutputFaceData
//  swizzle data in first 4 channels for input faces
//
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::ChannelSwapOutputFaceData(int32 a_Channel0Src, int32 a_Channel1Src, 
    int32 a_Channel2Src, int32 a_Channel3Src )
{
    int32 iFace, iMipLevel, u, v, k;
    int32 size;
    CP_ITYPE texelData[4];
    int32 channelSrcArray[4];

    //since output is being modified, terminate any active filtering threads
    TerminateActiveThreads();

    size = m_OutputSize;
    
    channelSrcArray[0] = a_Channel0Src;
    channelSrcArray[1] = a_Channel1Src;
    channelSrcArray[2] = a_Channel2Src;
    channelSrcArray[3] = a_Channel3Src;

    //Iterate over faces for input images
    for(iMipLevel=0; iMipLevel<m_NumMipLevels; iMipLevel++ )
    {
        for(iFace=0; iFace<6; iFace++)
        {
            for(v=0; v<m_OutputSurface[iMipLevel][iFace].m_Height; v++ )
            {
                for(u=0; u<m_OutputSurface[iMipLevel][iFace].m_Width; u++ )
                {
                    //get channel data
                    for(k=0; k<m_NumChannels; k++)
                    {
                        texelData[k] = *(m_OutputSurface[iMipLevel][iFace].GetSurfaceTexelPtr(u, v) + k);
                    }

                    //repack channel data accoring to swizzle information
                    for(k=0; k<m_NumChannels; k++)
                    {
                        *(m_OutputSurface[iMipLevel][iFace].GetSurfaceTexelPtr(u, v) + k) = texelData[ channelSrcArray[k] ];
                    }
                }
            }
        }
    }
}


//--------------------------------------------------------------------------------------
//Copy and convert cube map face data out of this class into an external image/surface
//
// a_FaceIdx        = a value 0 to 5 specifying which face to copy from (one of the CP_FACE_? )
// a_Level          = mip level to copy from
// a_DstType        = data type of image to copyed into (one of the CP_TYPE_? types)
// a_DstNumChannels = number of channels of the image to copyed into  (usually 1 to 4)
// a_DstPitch       = number of bytes per row of the source image to copyed into 
// a_DstDataPtr     = pointer to the image data to copyed into 
// a_Scale          = scale to apply to pixel values (in linear space) before gamma for output
// a_Gamma          = gamma level to apply to pixels after scaling
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::GetOutputFaceData(int32 a_FaceIdx, int32 a_Level, int32 a_DstType, 
   int32 a_DstNumChannels, int32 a_DstPitch, void *a_DstDataPtr, float32 a_Scale, float32 a_Gamma )   
{
   switch(a_DstType)
   {
      case CP_VAL_UNORM8:
      case CP_VAL_UNORM8_BGRA:
      case CP_VAL_UNORM16:
      case CP_VAL_FLOAT16:
      case CP_VAL_FLOAT32:
      {
         m_OutputSurface[a_Level][a_FaceIdx].GetImageDataScaleGamma( a_DstType, a_DstNumChannels, 
            a_DstPitch, a_DstDataPtr, a_Scale, a_Gamma ); 
      }
      break;
      default:
      break;
   }
}


//--------------------------------------------------------------------------------------
//Cube map filtering and mip chain generation.
// the cube map filtereing is specified using a number of parameters:
// Filtering per miplevel is specified using 2D cone angle (in degrees) that 
//  indicates the region of the hemisphere to filter over for each tap. 
//                
// Note that the top mip level is also a filtered version of the original input images 
//  as well in order to create mip chains for diffuse environment illumination.
// The cone angle for the top level is specified by a_BaseAngle.  This can be used to
//  generate mipchains used to store the resutls of preintegration across the hemisphere.
//
// Then the mip angle used to genreate the next level of the mip chain from the first level 
//  is a_InitialMipAngle
//
// The angle for the subsequent levels of the mip chain are specified by their parents 
//  filtering angle and a per-level scale and bias
//   newAngle = oldAngle * a_MipAnglePerLevelScale;
//
//--------------------------------------------------------------------------------------
static float32 ComputeBaseFilterAngle(float32 cosinePower)
{
   // Find angle for which: cos(a) ^ cosinePower = epsilon
   const float32 epsilon = 0.000001f;
   float32 angle = acosf(powf(epsilon, 1.0f / cosinePower));
   angle *= 180.0f / (float32)CP_PI;
   angle *= 2.0f;

   return angle;
}

inline float RadicalInverse2(uint32 bits)
{
	// Van der Corput radical inverse in base 2
	
	// Reverse bits
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	
	return float(bits) * 2.3283064365386963e-10;  // float(bits) * 2^-32
}

inline void HammersleySequence(uint32 sampleIndex, uint32 sampleCount, float32* vXi)
{
	vXi[0] = float(sampleIndex) / float(sampleCount);
	vXi[1] = RadicalInverse2(sampleIndex);
}

void ImportanceSampleGGX(float* vXi, float roughness, float* vNormal, float* vOut)
{
	float32 phi = 2 * CP_PI * vXi[0];
	float32 cosTheta = sqrtf((1 - vXi[1]) / ( 1 + (roughness * roughness - 1) * vXi[1]));
	float32 sinTheta = sqrtf(1 - cosTheta * cosTheta);

	float32 vH[3];
	vH[0] = sinTheta * cosf(phi);
	vH[1] = sinTheta * sinf(phi);
	vH[2] = cosTheta;

	float32 vUpVectorX[3] = {1, 0, 0};
	float32 vUpVectorZ[3] = {0, 0, 1};
	float32 vTangentX[3];
	float32 vTangentY[3];
	float32 vTempVec[3];
	
	// Build local frame
	VM_XPROD3(vTempVec, fabs(vNormal[2]) < 0.999f ? vUpVectorZ : vUpVectorX, vNormal);
	VM_NORM3(vTangentX, vTempVec);
	VM_XPROD3(vTangentY, vNormal, vTangentX);
	
	// Convert from tangent to world space
	vOut[0] = vTangentX[0] * vH[0] + vTangentY[0] * vH[1] + vNormal[0] * vH[2];
	vOut[1] = vTangentX[1] * vH[0] + vTangentY[1] * vH[1] + vNormal[1] * vH[2];
	vOut[2] = vTangentX[2] * vH[0] + vTangentY[2] * vH[1] + vNormal[2] * vH[2];
}


void CCubeMapProcessor::FilterCubeSurfacesGGX(CImageSurface *a_SrcCubeMap, CImageSurface *a_DstCubeMap, int32 a_SampleCount, float32 a_Roughness,
      int32 a_FaceIdxStart, int32 a_FaceIdxEnd, int32 a_ThreadIdx)
{
	const uint32 numChannels = VM_MIN(m_NumChannels, 4);
	const int32 dstSize = a_DstCubeMap[0].m_Width;

	//thread progress
	m_ThreadProgress[a_ThreadIdx].m_StartFace = a_FaceIdxStart;
	m_ThreadProgress[a_ThreadIdx].m_EndFace = a_FaceIdxEnd;

	//process required faces
#ifdef PROCESS_IN_PARALLEL
	// thread-workload is one face's line, gives plenty of concurrent occupation
	Concurrency::parallel_for((a_FaceIdxStart) * dstSize, (a_FaceIdxEnd + 1) * dstSize, 1, [&](int32 iCubeFaceDstSize)
	{
		//iterate over dst cube map face texel
		int32 iCubeFace = iCubeFaceDstSize / dstSize;
		int32 v = iCubeFaceDstSize % dstSize;

		{
#else
	for(int32 iCubeFace = a_FaceIdxStart; iCubeFace <= a_FaceIdxEnd; iCubeFace++)
	{
		//iterate over dst cube map face texel
		for(int32 v = 0; v < dstSize; v++)
		{
#endif
			CP_ITYPE *texelPtr = a_DstCubeMap[iCubeFace].m_ImgData + v * a_DstCubeMap[iCubeFace].m_NumChannels * dstSize;

			m_ThreadProgress[a_ThreadIdx].m_CurrentFace = iCubeFace;
			m_ThreadProgress[a_ThreadIdx].m_CurrentRow = v;

			for (int32 u = 0; u < dstSize; u++)
			{
				float32 color[4] = { 0 };
				float32 totalWeight = 0;
				float32 vH[3];
				float32 vL[3];

				// Assume normal and view vector to be vCenterTapDir
				float32 vCenterTapDir[3];
				TexelCoordToVect(iCubeFace, (float32)u, (float32)v, dstSize, vCenterTapDir);
				
				for (uint32 i = 0; i < (uint32)a_SampleCount; i++)
				{
					float32 vXi[2];
					HammersleySequence(i, a_SampleCount, vXi);
					ImportanceSampleGGX(vXi, a_Roughness, vCenterTapDir, vH);
					
					float fVdotH = VM_DOTPROD3(vCenterTapDir, vH);
					vL[0] = 2 * fVdotH * vH[0] - vCenterTapDir[0];
					vL[1] = 2 * fVdotH * vH[1] - vCenterTapDir[1];
					vL[2] = 2 * fVdotH * vH[2] - vCenterTapDir[2];
					
					float fNdotL = VM_DOTPROD3(vCenterTapDir, vL);
					if (fNdotL > 0)
					{
						CP_ITYPE *sourceTexel = GetCubeMapTexelPtr(vL, a_SrcCubeMap);
						for (uint32 k = 0; k < numChannels; k++)
						{
							color[k] += sourceTexel[k] * fNdotL;
						}
						
						totalWeight += fNdotL;
					}
				}

				for (uint32 k = 0; k < numChannels; k++)
				{
					texelPtr[k] = color[k] / totalWeight;
				}
				
				texelPtr += a_DstCubeMap[iCubeFace].m_NumChannels;
			}
		}
	}
#ifdef PROCESS_IN_PARALLEL
	);
#endif
}


void CCubeMapProcessor::FilterCubeMapMipChain(float32 a_BaseFilterAngle, float32 a_InitialMipAngle, float32 a_MipAnglePerLevelScale, 
    int32 a_FilterType, int32 a_FixupType, int32 a_FixupWidth, bool8 a_bUseSolidAngle, float32 a_GlossScale, float32 a_GlossBias,
    int32 a_SampleCountGGX)
{
   int32 i;
   float32 coneAngle;
   SECURITY_ATTRIBUTES secAttr;

   //thread security attributes for thread1
   secAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
   secAttr.lpSecurityDescriptor = NULL;            //use same security descriptor as parent
   secAttr.bInheritHandle = TRUE;                  //handle is inherited by new processes spawning from this one


   if(a_FilterType == CP_FILTER_TYPE_COSINE_POWER || a_FilterType == CP_FILTER_TYPE_GGX)
   {
      // Don't filter top mipmap
      a_BaseFilterAngle = 0;
   }

   //Build filter lookup tables based on the source miplevel size
   PrecomputeFilterLookupTables(a_FilterType, m_InputSurface[0].m_Width, a_BaseFilterAngle);

   //initialize thread progress
   m_ThreadProgress[0].m_CurrentMipLevel = 0;
   m_ThreadProgress[0].m_CurrentRow = 0;
   m_ThreadProgress[0].m_CurrentFace = 0;

   //if less than 2 filter threads, process all filtering from within callers thread
   if(m_NumFilterThreads < 2)
   {
      //Filter the top mip level (initial filtering used for diffuse or blurred specular lighting )
      FilterCubeSurfaces(m_InputSurface, m_OutputSurface[0], a_BaseFilterAngle, a_FilterType, a_bUseSolidAngle, 
         0,  //start at face 0 
         5,  //end at face 5
         0); //thread 0 is processing
   }   
   else //otherwise, split work between callers thread, and newly created thread
   {

      //consider thread 1 to be initialized
      m_bThreadInitialized[1] = TRUE;

      //setup filtering options for thread1
      sg_FilterOptionsCPU1.m_cmProc = this;
      sg_FilterOptionsCPU1.m_SrcCubeMap = m_InputSurface; 
      sg_FilterOptionsCPU1.m_DstCubeMap = m_OutputSurface[0];
      sg_FilterOptionsCPU1.m_FilterConeAngle = a_BaseFilterAngle;
      sg_FilterOptionsCPU1.m_FilterType = a_FilterType;
      sg_FilterOptionsCPU1.m_bUseSolidAngle = a_bUseSolidAngle; 
      sg_FilterOptionsCPU1.m_FaceIdxStart = 3; 
      sg_FilterOptionsCPU1.m_FaceIdxEnd = 5; 
      sg_FilterOptionsCPU1.m_ThreadIdx = 1; 

      m_ThreadProgress[1].m_CurrentMipLevel = 0;
      m_ThreadProgress[1].m_CurrentFace = 3;
      m_ThreadProgress[1].m_CurrentRow = 0;

      //start thread 1
      m_ThreadHandle[1] = CreateThread(
         &secAttr,                     //security attributes
         0,                            //0 is sentinel for use default stack size
         ThreadProcCPU1Filter,         //LPTHREAD_START_ROUTINE
         (LPVOID)NULL,                 // pass nothing into function
         NULL,                         // no creation flags (run thread immediately)
         &m_ThreadID[1] );



      //Filter the top mip level (initial filtering used for diffuse or blurred specular lighting )
      FilterCubeSurfaces(m_InputSurface, m_OutputSurface[0], a_BaseFilterAngle, a_FilterType, a_bUseSolidAngle, 
         0,  //start at face 0 
         2,  //end at face 2
         0); //thread 0 is processing

      //spin and sleep until thread1 is completed before continuing
      while( IsFilterThreadActive(1) )
      {
         Sleep(100);  //free up this thread for other thread to complete
      }
   }

   m_ThreadProgress[0].m_CurrentMipLevel = 1;
   m_ThreadProgress[0].m_CurrentRow = 0;
   m_ThreadProgress[0].m_CurrentFace = 0;


   FixupCubeEdges(m_OutputSurface[0], a_FixupType, a_FixupWidth);

   //Cone angle start (for generating subsequent mip levels)
   coneAngle = a_InitialMipAngle;

   //generate subsequent mip levels
   for(i=0; i<(m_NumMipLevels-1); i++)
   {
      m_ThreadProgress[0].m_CurrentMipLevel = i+1;
      m_ThreadProgress[0].m_CurrentRow = 0;
      m_ThreadProgress[0].m_CurrentFace = 0;

      CImageSurface* srcCubeImage = m_OutputSurface[i];
      
      if (a_FilterType == CP_FILTER_TYPE_GGX)
      {
        uint32 numUsableMips = m_NumMipLevels - 2;  // Lowest used mip is 4x4
        float32 smoothness = VM_MAX(1.0f - (float)(i + 1) / (float)(numUsableMips - 1), 0.0f);

        // Convert smoothness to roughness (needs to match shader code)
        float roughness = (1.0f - smoothness) * (1.0f - smoothness);

        FilterCubeSurfacesGGX(srcCubeImage, m_OutputSurface[i+1], a_SampleCountGGX, roughness,
          0,  //start at face 0 
          5,  //end at face 5
          0   //thread 0 is processing
          );
      }
      else
      {
        float specPow = 1.0f;

        if(a_FilterType == CP_FILTER_TYPE_COSINE_POWER)
        {
          uint32 numMipsForGloss = m_NumMipLevels - 2;  // Lowest used mip is 4x4
          float32 gloss = VM_MAX(1.0f - (float)(i + 1) / (float)(numMipsForGloss - 1), 0.0f);

          // Compute specular power (this must match shader code)
          specPow = pow(2.0f, a_GlossScale * gloss + a_GlossBias);

          // Blinn to Phong approximation: (R.E)^p == (N.H)^(4*p)
          specPow /= 4.0f;

          coneAngle = ComputeBaseFilterAngle(specPow);
          srcCubeImage = m_InputSurface;
        }

        //Build filter lookup tables based on the source miplevel size
        PrecomputeFilterLookupTables(a_FilterType, srcCubeImage->m_Width, coneAngle);

        //filter cube surfaces
        FilterCubeSurfaces(srcCubeImage, m_OutputSurface[i+1], coneAngle, a_FilterType, a_bUseSolidAngle,
          0,  //start at face 0 
          5,  //end at face 5
          0,  //thread 0 is processing
          specPow);
      }

      m_ThreadProgress[0].m_CurrentMipLevel = i+2;
      m_ThreadProgress[0].m_CurrentRow = 0;
      m_ThreadProgress[0].m_CurrentFace = 0;

      FixupCubeEdges(m_OutputSurface[i+1], a_FixupType, a_FixupWidth);        

      coneAngle = coneAngle * a_MipAnglePerLevelScale;
   }

   m_Status = CP_STATUS_FILTER_COMPLETED;
}


//--------------------------------------------------------------------------------------
//Builds the following lookup tables prior to filtering:
//  -normalizer cube map
//  -tap weight lookup table
// 
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::PrecomputeFilterLookupTables(uint32 a_FilterType, int32 a_SrcCubeMapWidth, float32 a_FilterConeAngle)
{
    float32 srcTexelAngle;
    int32   iCubeFace;
        
    //angle about center tap that defines filter cone
    float32 filterAngle;

    //min angle a src texel can cover (in degrees)
    srcTexelAngle = (180.0f / (float32)CP_PI) * atan2f(1.0f, (float32)a_SrcCubeMapWidth);  

    //filter angle is 1/2 the cone angle
    filterAngle = a_FilterConeAngle / 2.0f;

    //ensure filter angle is larger than a texel
    if(filterAngle < srcTexelAngle)
    {
        filterAngle = srcTexelAngle;    
    }

    //ensure filter cone is always smaller than the hemisphere
    if(filterAngle > 90.0f)
    {
        filterAngle = 90.0f;
    }

    //build lookup table for tap weights based on angle between current tap and center tap
    BuildAngleWeightLUT(a_SrcCubeMapWidth * 2, a_FilterType, filterAngle);

    //clear pre-existing normalizer cube map
    for(iCubeFace=0; iCubeFace<6; iCubeFace++)
    {
        m_NormCubeMap[iCubeFace].Clear();            
    }

    //Normalized vectors per cubeface and per-texel solid angle 
    BuildNormalizerSolidAngleCubemap(a_SrcCubeMapWidth, m_NormCubeMap);

}

//--------------------------------------------------------------------------------------
//The key to the speed of these filtering routines is to quickly define a per-face 
//  bounding box of pixels which enclose all the taps in the filter kernel efficiently.  
//  Later these pixels are selectively processed based on their dot products to see if 
//  they reside within the filtering cone.
//
//This is done by computing the smallest per-texel angle to get a conservative estimate 
// of the number of texels needed to be covered in width and height order to filter the
// region.  the bounding box for the center taps face is defined first, and if the 
// filtereing region bleeds onto the other faces, bounding boxes for the other faces are 
// defined next
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::FilterCubeSurfaces(CImageSurface *a_SrcCubeMap, CImageSurface *a_DstCubeMap, 
    float32 a_FilterConeAngle, int32 a_FilterType, bool8 a_bUseSolidAngle, int32 a_FaceIdxStart, 
    int32 a_FaceIdxEnd, int32 a_ThreadIdx, float32 a_SpecularPower)
{
    const int32 srcSize = a_SrcCubeMap[0].m_Width;
    const int32 dstSize = a_DstCubeMap[0].m_Width;

    //min angle a src texel can cover (in degrees)
    const float32 srcTexelAngle = (180.0f / (float32)CP_PI) * atan2f(1.0f, (float32)srcSize);  

    //angle about center tap to define filter cone
    float32 filterAngle;

    //filter angle is 1/2 the cone angle
    filterAngle = a_FilterConeAngle / 2.0f;

    //ensure filter angle is larger than a texel
    if(filterAngle < srcTexelAngle)
    {
        filterAngle = srcTexelAngle;    
    }

    //ensure filter cone is always smaller than the hemisphere
    if(filterAngle > 90.0f)
    {
        filterAngle = 90.0f;
    }

    //the maximum number of texels in 1D the filter cone angle will cover
    //  used to determine bounding box size for filter extents
    //ensure conservative region always covers at least one texel
    const int32 filterSize = max((int32)ceil(filterAngle / srcTexelAngle), 1);   

    //dotProdThresh threshold based on cone angle to determine whether or not taps 
    // reside within the cone angle
    const float32 dotProdThresh = cosf( ((float32)CP_PI / 180.0f) * filterAngle );

    //thread progress
    m_ThreadProgress[a_ThreadIdx].m_StartFace = a_FaceIdxStart;
    m_ThreadProgress[a_ThreadIdx].m_EndFace = a_FaceIdxEnd;

    //process required faces
#ifdef PROCESS_IN_PARALLEL
    // thread-workload is one face's line, gives plenty of concurrent occupation
    Concurrency::parallel_for((a_FaceIdxStart) * dstSize, (a_FaceIdxEnd + 1) * dstSize, 1, [&](int32 iCubeFaceDstSize)
    {
        //iterate over dst cube map face texel
        int32 iCubeFace = iCubeFaceDstSize / dstSize;
        int32 v = iCubeFaceDstSize % dstSize;

        {
#else
    for(int32 iCubeFace = a_FaceIdxStart; iCubeFace <= a_FaceIdxEnd; iCubeFace++)
    {
        //iterate over dst cube map face texel
        for(int32 v = 0; v < dstSize; v++)
        {
#endif
           CP_ITYPE *texelPtr = a_DstCubeMap[iCubeFace].m_ImgData + v * a_DstCubeMap[iCubeFace].m_NumChannels * dstSize;

           m_ThreadProgress[a_ThreadIdx].m_CurrentFace = iCubeFace;
           m_ThreadProgress[a_ThreadIdx].m_CurrentRow = v;

            for(int32 u=0; u<dstSize; u++)
            {
                //CImageSurface normCubeMap[6];     //
                CBBoxInt32    filterExtents[6];   //bounding box per face to specify region to process
                                                  // note that pixels within these regions may be rejected 
                                                  // based on the     
                float32 centerTapDir[3];  //direction of center tap

                //get center tap direction
                TexelCoordToVect(iCubeFace, (float32)u, (float32)v, dstSize, centerTapDir );

                //clear old per-face filter extents
                ClearFilterExtents(filterExtents);

                //define per-face filter extents
                DetermineFilterExtents(centerTapDir, srcSize, filterSize, filterExtents );
                    
                //perform filtering of src faces using filter extents 
                ProcessFilterExtents(centerTapDir, dotProdThresh, filterExtents, m_NormCubeMap, a_SrcCubeMap, texelPtr, a_FilterType, a_bUseSolidAngle, a_SpecularPower);

                texelPtr += a_DstCubeMap[iCubeFace].m_NumChannels;
            }            
        }
    }
#ifdef PROCESS_IN_PARALLEL
    );
#endif
}



//--------------------------------------------------------------------------------------
//starts a new thread to execute the filtering options
//
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::InitiateFiltering(float32 a_BaseFilterAngle, float32 a_InitialMipAngle, 
      float32 a_MipAnglePerLevelScale, int32 a_FilterType, int32 a_FixupType, int32 a_FixupWidth, bool8 a_bUseSolidAngle,
      float32 a_GlossScale, float32 a_GlossBias, int32 a_SampleCountGGX)
{
   SECURITY_ATTRIBUTES secAttr;

   secAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
   secAttr.lpSecurityDescriptor = NULL;            //use same security descriptor as parent
   secAttr.bInheritHandle = TRUE;                  //handle is inherited by new processes spawning from this one

   //set filtering options in main class to determine 
   m_BaseFilterAngle = a_BaseFilterAngle;
   m_InitialMipAngle = a_InitialMipAngle;
   m_MipAnglePerLevelScale = a_MipAnglePerLevelScale;


   //terminate preexisting threads if needed
   TerminateActiveThreads();

   //start thread if at least one filter thread is available
   if(m_NumFilterThreads > 0)
   {
      //consider thread to be initialized
      m_bThreadInitialized[0] = TRUE;

      sg_FilterOptionsCPU0.m_cmProc = this;

      sg_FilterOptionsCPU0.m_BaseFilterAngle = a_BaseFilterAngle;
      sg_FilterOptionsCPU0.m_InitialMipAngle = a_InitialMipAngle;
      sg_FilterOptionsCPU0.m_MipAnglePerLevelScale = a_MipAnglePerLevelScale; 
      sg_FilterOptionsCPU0.m_GlossScale = a_GlossScale;
      sg_FilterOptionsCPU0.m_GlossBias = a_GlossBias;
      sg_FilterOptionsCPU0.m_FilterType = a_FilterType;
      sg_FilterOptionsCPU0.m_FixupType = a_FixupType;
      sg_FilterOptionsCPU0.m_FixupWidth = a_FixupWidth;
      sg_FilterOptionsCPU0.m_SampleCountGGX = a_SampleCountGGX;
      sg_FilterOptionsCPU0.m_bUseSolidAngle = a_bUseSolidAngle;

      m_Status = CP_STATUS_PROCESSING;

      m_ThreadHandle[0] = CreateThread(
         &secAttr,                     //security attributes
         0,                            //0 is sentinel for use default stack size
         ThreadProcCPU0Filter,         //LPTHREAD_START_ROUTINE
         (LPVOID)NULL,                 // pass nothing into function
         NULL,                         // no creation flags (run thread immediately)
         &m_ThreadID[0] );
   }
   else //otherwise call filtering function from the current process
   {
      FilterCubeMapMipChain(a_BaseFilterAngle, a_InitialMipAngle, a_MipAnglePerLevelScale, a_FilterType, 
         a_FixupType, a_FixupWidth, a_bUseSolidAngle, a_GlossScale, a_GlossBias, a_SampleCountGGX);
   
   }


}


//--------------------------------------------------------------------------------------
//build filter lookup table
//
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::BuildAngleWeightLUT(int32 a_NumFilterLUTEntries, int32 a_FilterType, float32 a_FilterAngle)
{
    int32 iLUTEntry;

    CP_SAFE_DELETE_ARRAY( m_FilterLUT );

    m_NumFilterLUTEntries = 4096; //a_NumFilterLUTEntries;
    m_FilterLUT = new CP_ITYPE [m_NumFilterLUTEntries];

    // note that CP_FILTER_TYPE_DISC weights all taps equally and does not need a lookup table    
    if( a_FilterType == CP_FILTER_TYPE_CONE )
    {
        //CP_FILTER_TYPE_CONE is a cone centered around the center tap and falls off to zero 
        //  over the filtering radius
        CP_ITYPE filtAngleRad = a_FilterAngle * CP_PI / 180.0f;

        for(iLUTEntry=0; iLUTEntry<m_NumFilterLUTEntries; iLUTEntry++ )
        {
            CP_ITYPE angle = acos( (float32)iLUTEntry / (float32)(m_NumFilterLUTEntries - 1) );
            CP_ITYPE filterVal;
        
            filterVal = (filtAngleRad - angle) / filtAngleRad;

            if(filterVal < 0)
            {
                filterVal = 0;
            }

            //note that gaussian is not weighted by 1.0 / (sigma* sqrt(2 * PI)) seen as weights
            // weighted tap accumulation in filters is divided by sum of weights
            m_FilterLUT[iLUTEntry] = filterVal;
        }
    }
    else if( a_FilterType == CP_FILTER_TYPE_ANGULAR_GAUSSIAN )
    {
        //fit 3 standard deviations within angular extent of filter
        CP_ITYPE stdDev = (a_FilterAngle * CP_PI / 180.0) / 3.0;
        CP_ITYPE inv2Variance = 1.0 / (2.0 * stdDev * stdDev);
        
        for(iLUTEntry=0; iLUTEntry<m_NumFilterLUTEntries; iLUTEntry++ )
        {
            CP_ITYPE angle = acos( (float32)iLUTEntry / (float32)(m_NumFilterLUTEntries - 1) );
            CP_ITYPE filterVal;
        
            filterVal = exp( -(angle * angle) * inv2Variance );

            //note that gaussian is not weighted by 1.0 / (sigma* sqrt(2 * PI)) seen as weights
            // weighted tap accumulation in filters is divided by sum of weights
            m_FilterLUT[iLUTEntry] = filterVal;
        }
    }
}


//--------------------------------------------------------------------------------------
// WriteMipLevelIntoAlpha
//
//  Writes the current mip level into alpha in order for 2.0 shaders that need to 
//  know the current mip-level
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::WriteMipLevelIntoAlpha(void)
{
    int32 iFace, iMipLevel;

    //since output is being modified, terminate any active filtering threads
    TerminateActiveThreads();

    //generate subsequent mip levels
    for(iMipLevel = 0; iMipLevel < m_NumMipLevels; iMipLevel++)
    {
        //Iterate over faces for input images
        for(iFace = 0; iFace < 6; iFace++)
        {
            m_OutputSurface[iMipLevel][iFace].ClearChannelConst(3, (float32) (16.0f * (iMipLevel / 255.0f)) );
        }
    }
}


//--------------------------------------------------------------------------------------
// Horizonally flip input cube map faces
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::FlipInputCubemapFaces(void)
{
    int32 iFace;

    //since input is being modified, terminate any active filtering threads
    TerminateActiveThreads();

    //Iterate over faces for input images
    for(iFace = 0; iFace < 6; iFace++)
    {
        m_InputSurface[iFace].InPlaceHorizonalFlip();
    }
}


//--------------------------------------------------------------------------------------
//Horizonally flip output cube map faces
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::FlipOutputCubemapFaces(void)
{
   int32 iFace, iMipLevel;

   //since output is being modified, terminate any active filtering threads
   TerminateActiveThreads();

   //Iterate over faces for input images
   for(iMipLevel = 0; iMipLevel < m_NumMipLevels; iMipLevel++)
   {
      for(iFace = 0; iFace < 6; iFace++)
      {
         m_OutputSurface[iMipLevel][iFace].InPlaceHorizonalFlip();
      }
   }
}


//--------------------------------------------------------------------------------------
// test to see if filter thread is still active
//
//--------------------------------------------------------------------------------------
bool8 CCubeMapProcessor::IsFilterThreadActive(uint32 a_ThreadIdx)
{
   if( (m_bThreadInitialized[a_ThreadIdx] == FALSE) ||
       (m_ThreadHandle[a_ThreadIdx] == NULL) 
       )
   {
      return FALSE;
   }
   else 
   {  //thread is initialized
      DWORD exitCode;

      GetExitCodeThread(m_ThreadHandle[a_ThreadIdx], &exitCode );

      if( exitCode == CP_THREAD_STILL_ACTIVE )
      {
         return TRUE;
      }
   }
   
   return FALSE;
}


//--------------------------------------------------------------------------------------
//estimate fraction completed of filter thread based on current conditions
//
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::EstimateFilterThreadProgress(SFilterProgress *a_FilterProgress)
{
   float32 totalMipComputation = 0.0f;     //time to compute all mip levels as a function of the time it takes 
											//to compute the top mip level

   float32 progressMipComputation = 0.0f;	//progress based on entirely computed mip levels
   float32 currentMipComputation = 0.0f;	//amount of computation it takes to process this entire mip level
   float32 progressFaceComputation = 0.0f;	//progress based on entirely computed faces for this mip level
   float32 currentFaceComputation = 0.0f;	//amount of computation it takes to process this entire face
   float32 progressRowComputation = 0.0f;	//progress based on entirely computed rows for this face
											//estimated fraction of total computation time the current face will take
   
   int32 i;

   float32 filterAngle = 1.0f;					//filter angle for given miplevel
   int32 dstSize = 1;						//destination cube map size of given mip level
   int32 currentMipSize = 1;				//size of mip level currently being processed

   //compuate total compuation time as a function of the time  
   // cubemap processing for each miplevel is roughly O(n^2 * m^2) 
   //  where n is the cube map size, and m is the filter size   
   // Each miplevel is half the size of the previous level,  
   //  and the filter size in texels is roughly proportional to the 
   // (filter angle size * size of source cubemap texels are fetched from) ^2 

   // computation to generate base mip level (generated from input cube map)
   if(m_BaseFilterAngle > 0.0f)
   {
      totalMipComputation = pow(m_InputSize * m_BaseFilterAngle , 2.0f) * (m_OutputSize * m_OutputSize);
   }
   else
   {
      totalMipComputation = pow(m_InputSize * 0.01f , 2.0f) * (m_OutputSize * m_OutputSize);
   }

   progressMipComputation = 0.0f;
   if(a_FilterProgress->m_CurrentMipLevel > 0)
   {
      progressMipComputation = totalMipComputation;
   }

   //filtering angle for this miplevel
   filterAngle = m_InitialMipAngle;
   dstSize = m_OutputSize;

   //computation for entire base mip level (if current level is base level)
   if(a_FilterProgress->m_CurrentMipLevel == 0)
   {
      currentMipComputation = totalMipComputation;
      currentMipSize = dstSize;
   }

   //compuatation to generate subsequent mip levels
   for(i=1; i<m_NumMipLevels; i++)
   {
      float32 computation;

      dstSize /= 2;
      filterAngle *= m_MipAnglePerLevelScale;

      if(filterAngle > 180)
      {
         filterAngle = 180;
      }

      //note src size is dstSize*2 since miplevels are generated from the subsequent level
      computation = pow(dstSize * 2 * filterAngle, 2.0f) * (dstSize * dstSize);

      totalMipComputation += computation;

      //accumulate computation for completed mip levels
      if(a_FilterProgress->m_CurrentMipLevel > i)
      {
         progressMipComputation = totalMipComputation;
      }

      //computation for entire current mip level
      if(a_FilterProgress->m_CurrentMipLevel == i)
      {
         currentMipComputation = computation;
         currentMipSize = dstSize;
      }
   }

   //fraction of compuation time processing the entire current mip level will take
   currentMipComputation  /= totalMipComputation; 
   progressMipComputation /= totalMipComputation;

   progressFaceComputation = currentMipComputation * 
      (float32)(a_FilterProgress->m_CurrentFace - a_FilterProgress->m_StartFace) /
      (float32)(1 + a_FilterProgress->m_EndFace - a_FilterProgress->m_StartFace);

   currentFaceComputation = currentMipComputation * 
      1.0f /
      (1 + a_FilterProgress->m_EndFace - a_FilterProgress->m_StartFace);

   progressRowComputation = currentFaceComputation * 
      ((float32)a_FilterProgress->m_CurrentRow / (float32)currentMipSize);  

   //progress completed
   a_FilterProgress->m_FractionCompleted = 
      progressMipComputation +
      progressFaceComputation +
      progressRowComputation;


   if( a_FilterProgress->m_CurrentFace < 0)
   {
      a_FilterProgress->m_CurrentFace = 0;
   }

   if( a_FilterProgress->m_CurrentMipLevel < 0)
   {
      a_FilterProgress->m_CurrentMipLevel = 0;
   }

   if( a_FilterProgress->m_CurrentRow < 0)
   {
      a_FilterProgress->m_CurrentRow = 0;
   }

}


//--------------------------------------------------------------------------------------
// Return string describing the current status of the cubemap processing threads
//
//--------------------------------------------------------------------------------------
WCHAR *CCubeMapProcessor::GetFilterProgressString(void)
{
   WCHAR threadProgressString[CP_MAX_FILTER_THREADS][CP_MAX_PROGRESS_STRING];
   int32 i;

   for(i=0; i<m_NumFilterThreads; i++)
   {
      if(IsFilterThreadActive(i))
      {
         //set wait
         SetCursor(LoadCursor(NULL, IDC_WAIT ));

         EstimateFilterThreadProgress(&(m_ThreadProgress[i]) );

         _snwprintf_s(threadProgressString[i],
            CP_MAX_PROGRESS_STRING,
            CP_MAX_PROGRESS_STRING,
            L"%5.2f%% Complete (Level %3d, Face %3d, Row %3d)", 
            100.0f * m_ThreadProgress[i].m_FractionCompleted,
            m_ThreadProgress[i].m_CurrentMipLevel, 
            m_ThreadProgress[i].m_CurrentFace,
            m_ThreadProgress[i].m_CurrentRow        
            );
      }
      else
      {
         //set arrow
         SetCursor(LoadCursor(NULL, IDC_ARROW ));

         _snwprintf_s(threadProgressString[i], 
            CP_MAX_PROGRESS_STRING,
            CP_MAX_PROGRESS_STRING,
            L"Ready");   
      }
   }

   if(m_NumFilterThreads == 2)
   {  //display information about both threads
      _snwprintf_s(m_ProgressString, 
         CP_MAX_PROGRESS_STRING,
         CP_MAX_PROGRESS_STRING,
         L"Thread0: %s \nThread1: %s", 
         threadProgressString[0],
         threadProgressString[1]);
   }
   else
   {  //only display information about one thread
      _snwprintf_s(m_ProgressString, 
         CP_MAX_PROGRESS_STRING,
         CP_MAX_PROGRESS_STRING,
         L"Thread 0: %s ", 
         threadProgressString[0]);
   }

   return m_ProgressString;
}


//--------------------------------------------------------------------------------------
//get status of cubemap processor
//
//--------------------------------------------------------------------------------------
int32 CCubeMapProcessor::GetStatus(void)
{
   return m_Status;
}


//--------------------------------------------------------------------------------------
//refresh status
// sets cubemap processor to ready state if not processing
//--------------------------------------------------------------------------------------
void CCubeMapProcessor::RefreshStatus(void)
{
   if(m_Status != CP_STATUS_PROCESSING )
   {
      m_Status = CP_STATUS_READY;
   
   }
}




