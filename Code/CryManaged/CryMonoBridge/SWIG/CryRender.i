%include "CryEngine.swig"

%import "CryCommon.i"
%import "CryPhysics.i"

%{
#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryRenderer/IColorGradingController.h>
#include <CrySystem/Scaleform/IFlashPlayer.h>
#include <CryRenderer/IStereoRenderer.h>
#include <CryRenderer/IImage.h>
%}
%ignore IRendererEngineModule;
%ignore operator==(const CInputLightMaterial &m1, const CInputLightMaterial &m2);
%ignore CRenderCamera::GetXform_Screen2Obj;
%ignore CRenderCamera::GetXform_Obj2Screen;
%ignore CRenderCamera::GetViewportMatrix;
%ignore CRenderCamera::SetModelviewMatrix;
%ignore CRenderCamera::GetInvModelviewMatrix;
%ignore CRenderCamera::GetInvProjectionMatrix;
%ignore CRenderCamera::GetInvViewportMatrix;
%ignore SMinMaxBox::PointInBBox;
%ignore SMinMaxBox::ViewFrustumCull;
%ignore IRenderer::SDrawCallCountInfo::Update;
%csconstvalue("(uint)0x80000000") FT_USAGE_UAV_RWTEXTURE;
%typemap(csbase) ETextureFlags "uint"

%include "../../../../CryEngine/CryCommon/CryRenderer/IImage.h"
%include "../../../../CryEngine/CryCommon/CryRenderer/ITexture.h"
%ignore SShaderParam::GetValue;
%ignore CRenderObject;
%ignore STexState;
%ignore STexSamplerFX;
%ignore STexSamplerRT;
%ignore SEfResTexture::UpdateForCreate;
%ignore SEfResTexture::Update;
%ignore SEfResTexture::UpdateWithModifier;
%ignore SShaderItem::PostLoad;
%ignore SShaderItem::Update;
%ignore SShaderItem::RefreshResourceConstants;
%ignore SShaderItem::GetTechnique;
%ignore SShaderItem::IsMergable;
%ignore CRenderChunk::Size;
%ignore SShaderGraphNode;
%ignore SShaderGraphBlock;
%ignore SBending::GetShaderConstants;
%include "../../../../CryEngine/CryCommon/CryRenderer/IShader.h"
%feature("director") ICaptureFrameListener;
%feature("director") IRenderEventListener;
%feature("director") ITextureStreamListener;
%feature("director") ISyncMainWithRenderListener;
%feature("director") ILoadtimeCallback;
%template(IImageFile_SmartPtr) _smart_ptr<IImageFile>;
%template(ITexture_SmartPtr) _smart_ptr<ITexture>;
%typemap(cscode) IRenderer %{
  public ITexture CreateTexture(string name, int width, int height, int numMips, byte[] data, byte eTF, int flags) {
	System.Runtime.InteropServices.GCHandle pinnedArray = System.Runtime.InteropServices.GCHandle.Alloc(data, System.Runtime.InteropServices.GCHandleType.Pinned);
	System.Runtime.InteropServices.HandleRef pData = new System.Runtime.InteropServices.HandleRef(pinnedArray, pinnedArray.AddrOfPinnedObject());
	global::System.IntPtr cPtr = GlobalPINVOKE.IRenderer_CreateTexture(swigCPtr, name, width, height, numMips, pData, eTF, flags);
	pinnedArray.Free();
	return (cPtr == global::System.IntPtr.Zero) ? null : new ITexture(cPtr, false);
  }
  public void UpdateTextureInVideoMemory(uint tnum, byte[] newdata,int posx,int posy,int w,int h,ETEX_Format eTFSrc=ETEX_Format.eTF_B8G8R8,int posz=0, int sizez=1)
  {
	System.Runtime.InteropServices.GCHandle pinnedArray = System.Runtime.InteropServices.GCHandle.Alloc(newdata, System.Runtime.InteropServices.GCHandleType.Pinned);
	System.Runtime.InteropServices.HandleRef pNewdata = new System.Runtime.InteropServices.HandleRef(pinnedArray, pinnedArray.AddrOfPinnedObject());
	GlobalPINVOKE.IRenderer_UpdateTextureInVideoMemory__SWIG_0(swigCPtr, tnum, pNewdata, posx, posy, w, h, (byte)eTFSrc, posz, sizez);
	pinnedArray.Free();
  }
%}
%include "../../../../CryEngine/CryCommon/CryRenderer/IRenderer.h"
%extend IRenderer {
public:
	int GetDetailedRayHitInfo(IPhysicalEntity& pCollider, const Vec3& vOrigin, const Vec3& vDirection, const float& maxRayDist, float& fSwigRef1, float& fSwigRef2) 
	{ 
		return $self->GetDetailedRayHitInfo(&pCollider, vOrigin, vDirection, maxRayDist, &fSwigRef1, &fSwigRef2);
	}
}
// hacky enum definitions
%csconstvalue("0x0 << EAuxGeomPublicRenderflagBitMasks.e_Mode2D3DShift") e_Mode3D;
%csconstvalue("0x1 << EAuxGeomPublicRenderflagBitMasks.e_Mode2D3DShift") e_Mode2D;
%csconstvalue("0x2 << EAuxGeomPublicRenderflagBitMasks.e_Mode2D3DShift") e_ModeText;
%csconstvalue("0x3 << EAuxGeomPublicRenderflagBitMasks.e_Mode2D3DShift") e_ModeUnit;
%csconstvalue("0x0 << EAuxGeomPublicRenderflagBitMasks.e_AlphaBlendingShift") e_AlphaNone;
%csconstvalue("0x1 << EAuxGeomPublicRenderflagBitMasks.e_AlphaBlendingShift") e_AlphaAdditive;
%csconstvalue("0x2 << EAuxGeomPublicRenderflagBitMasks.e_AlphaBlendingShift") e_AlphaBlended;
%csconstvalue("0x0 << EAuxGeomPublicRenderflagBitMasks.e_DrawInFrontShift") e_DrawInFrontOff;
%csconstvalue("0x1 << EAuxGeomPublicRenderflagBitMasks.e_DrawInFrontShift") e_DrawInFrontOn;
%csconstvalue("0x0 << EAuxGeomPublicRenderflagBitMasks.e_FillModeShift") e_FillModeSolid;
%csconstvalue("0x1 << EAuxGeomPublicRenderflagBitMasks.e_FillModeShift") e_FillModeWireframe;
%csconstvalue("0x2 << EAuxGeomPublicRenderflagBitMasks.e_FillModeShift") e_FillModePoint;
%csconstvalue("0x0 << EAuxGeomPublicRenderflagBitMasks.e_CullModeShift") e_CullModeNone;
%csconstvalue("0x1 << EAuxGeomPublicRenderflagBitMasks.e_CullModeShift") e_CullModeFront;
%csconstvalue("0x2 << EAuxGeomPublicRenderflagBitMasks.e_CullModeShift") e_CullModeBack;
%csconstvalue("0x0 << EAuxGeomPublicRenderflagBitMasks.e_DepthWriteShift") e_DepthWriteOn;
%csconstvalue("0x1 << EAuxGeomPublicRenderflagBitMasks.e_DepthWriteShift") e_DepthWriteOff;
%csconstvalue("0x0 << EAuxGeomPublicRenderflagBitMasks.e_DepthTestShift") e_DepthTestOn;
%csconstvalue("0x1 << EAuxGeomPublicRenderflagBitMasks.e_DepthTestShift") e_DepthTestOff;
%csconstvalue("EAuxGeomPublicRenderflags_Mode2D3D.e_Mode3D|EAuxGeomPublicRenderflags_AlphaBlendMode.e_AlphaNone|EAuxGeomPublicRenderflags_DrawInFrontMode.e_DrawInFrontOff|EAuxGeomPublicRenderflags_FillMode.e_FillModeSolid|EAuxGeomPublicRenderflags_CullMode.e_CullModeBack|EAuxGeomPublicRenderflags_DepthWrite.e_DepthWriteOn|EAuxGeomPublicRenderflags_DepthTest.e_DepthTestOn") e_Def3DPublicRenderflags;
%csconstvalue("EAuxGeomPublicRenderflags_Mode2D3D.e_Mode2D|EAuxGeomPublicRenderflags_AlphaBlendMode.e_AlphaNone|EAuxGeomPublicRenderflags_DrawInFrontMode.e_DrawInFrontOff|EAuxGeomPublicRenderflags_FillMode.e_FillModeSolid|EAuxGeomPublicRenderflags_CullMode.e_CullModeBack|EAuxGeomPublicRenderflags_DepthWrite.e_DepthWriteOn|EAuxGeomPublicRenderflags_DepthTest.e_DepthTestOn") e_Def2DPublicRenderflags;
%csconstvalue("EAuxGeomPublicRenderflags_Mode2D3D.e_Mode2D|EAuxGeomPublicRenderflags_AlphaBlendMode.e_AlphaBlended|EAuxGeomPublicRenderflags_DepthTest.e_DepthTestOff|EAuxGeomPublicRenderflags_CullMode.e_CullModeNone") e_Def2DImageRenderflags;
// ~hacky enum definitions
%include "../../../../CryEngine/CryCommon/CryRenderer/IRenderAuxGeom.h"
%include "../../../../CryEngine/CryCommon/CryRenderer/RenderElements/CREMesh.h"
%include "../../../../CryEngine/CryCommon/CryRenderer/IColorGradingController.h"
%typemap(csbase) IFlashPlayer::ECategory "uint"
%include "../../../../CryEngine/CryCommon/CrySystem/Scaleform/IFlashPlayer.h"
%include "../../../../CryEngine/CryCommon/CryRenderer/IStereoRenderer.h"
%include "../../../../CryEngine/CryCommon/CryRenderer/IMeshBaking.h"
