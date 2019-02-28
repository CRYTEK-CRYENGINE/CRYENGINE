%include "CryEngine.swig"

%{
#include <Cry3DEngine/CGF/CryHeaders.h>
#include <CryExtension/ICryFactory.h>
#include <CryExtension/ICryFactoryRegistry.h>
#include <CryAnimation/IAttachment.h>
#include <CryAudio/Dialog/IDialogSystem.h>
#include <CrySystem/XML/IReadWriteXMLSink.h>
#include <CrySystem/Scaleform/IFlashUI.h>
#include <CryAISystem/IPathfinder.h>
#include <CryAction/IMaterialEffects.h>
#include <CryCore/CryTypeInfo.h>
#include <CryLobby/ICryStats.h>
#include <CryExtension/ICryUnknown.h>
%}

%feature("nspace", 1);

%rename(op_Equal) operator =;
%rename(op_PlusEqual) operator +=;
%rename(op_MinusEqual) operator -=;
%rename(op_MultiplyEqual) operator *=;
%rename(op_DivideEqual) operator /=;
%rename(op_PercentEqual) operator %=;
%rename(op_Plus) operator +;
%rename(op_Minus) operator -;
%rename(op_Multiply) operator *;
%rename(op_Divide) operator /;
%rename(op_Percent) operator %;
%rename(op_Not) operator !;
%rename(op_IndexIntoConst) operator[](unsigned idx) const;
%rename(op_IndexInto) operator[](unsigned idx);
%rename(op_Functor) operator ();
%rename(op_EqualEqual) operator ==;
%rename(op_NotEqual) operator !=;
%rename(op_LessThan) operator <;
%rename(op_LessThanEqual) operator <=;
%rename(op_GreaterThan) operator >;
%rename(op_GreaterThanEqual) operator >=;
%rename(op_And) operator &&;
%rename(op_Or) operator ||;
%rename(op_PlusPlusPrefix) operator++();
%rename(op_PlusPlusPostfix) operator++(int);
%rename(op_MinusMinusPrefix) operator--();
%rename(op_MinusMinusPostfix) operator--(int);

//%include "csharp.swg"

%ignore CryStringT<char>::compare;
%ignore _npos_type;
%include "../../../../CryEngine/CryCommon/CryString/CryString.h"
%template(CryString) CryStringT<char>;
%include "../../../../CryEngine/CryCommon/CryString/CryFixedString.h"
%template (CryFixedString1024) CryFixedStringT<1024>;
%template (CryFixedString1025) CryFixedStringT<1025>;
%template (CryFixedString128) CryFixedStringT<128>;
%template (CryFixedString16) CryFixedStringT<16>;
%template (CryFixedString32) CryFixedStringT<32>;
%template (CryFixedString33) CryFixedStringT<33>;
%template (CryFixedString40) CryFixedStringT<40>;
%template (CryFixedString48) CryFixedStringT<48>;
%template (CryFixedString64) CryFixedStringT<64>;
%template (CryFixedString65) CryFixedStringT<65>;
%template (CryFixedString512) CryFixedStringT<512>;
%template(CharTraitsChar) CharTraits<char>;
%include "../../../../CryEngine/CryCommon/CryString/CryPath.h"
%include "../../../../CryEngine/CryCommon/CryMemory/IGeneralMemoryHeap.h"
%include "../../../../CryEngine/CryCommon/CryCore/CryEndian.h"
//TODO: %import "../../../CryEngine/CryCommon/CryArray.h"
%include <std_shared_ptr.i>
//%include "../../../CryEngine/CryCommon/IComponent.h"
%include <typemaps.i>
%apply int *OUTPUT { int& value };
%apply unsigned int *OUTPUT { unsigned int& value };
%apply float *OUTPUT { float& value };
%apply double *OUTPUT { double& value };
%apply bool *OUTPUT { bool& value };
%include "../../../../CryEngine/CryCommon/CrySystem/XML/IXml.h"
%ignore CCryNameCRC::CCryNameCRC;
%include "../../../../CryEngine/CryCommon/CryString/CryName.h"
%include "../../../../CryEngine/CryCommon/CryMath/Cry_Camera.h"
%include "../../../../CryEngine/CryCommon/CrySystem/File/CryFile.h"
%ignore NODE_CHUNK_DESC_0824; // until it's tm is properly swigged
%include "../../../../CryEngine/CryCommon/Cry3DEngine/CGF/CryHeaders.h"

%include "../../../../CryEngine/CryCommon/CryThreading/CryThreadSafeRendererContainer.h"

%include "../../../../CryEngine/CryCommon/CryMath/CryHalf.inl"
%include "../../../../CryEngine/CryCommon/CryMath/Cry_Math.h"
%include "../../../../CryEngine/CryCommon/CryMath/Cry_Geo.h"
%import "../../../../CryEngine/CryCommon/CryMath/Cry_Vector2.h"
%template(Vec2) Vec2_tpl<f32>;
%import "../../../../CryEngine/CryCommon/CryMath/Cry_Vector3.h"
%template(Vec3) Vec3_tpl<f32>;
// for c# unit test
%extend Vec3_tpl<f32> {
	f32 Dot(const Vec3_tpl<f32> vec1)
	{
		return $self->dot(vec1);
	} 
}
%template(Ang3) Ang3_tpl<f32>;
%import "../../../../CryEngine/CryCommon/CryMath/Cry_Vector4.h"
%template(Vec4) Vec4_tpl<f32>;
// for c# unit test
%extend Vec4_tpl<f32> {
	Vec4_tpl<f32> Normalize()
	{
		return $self->Normalize();
	}
}

%import "../../../../CryEngine/CryCommon/CryMath/Cry_MatrixDiag.h"
%template(Diag33) Diag33_tpl<f32>;
%import "../../../../CryEngine/CryCommon/CryMath/Cry_Matrix33.h"
%template(Matrix33) Matrix33_tpl<f32>;
// for c# unit test
%extend Matrix33_tpl<f32> {
	static Matrix33_tpl<f32> CreateMatrix33(Vec3_tpl<f32> right, Vec3_tpl<f32> forward, Vec3_tpl<f32> up)
	{
		return Matrix33_tpl<f32>(right, forward,up);
	}
}
%import "../../../../CryEngine/CryCommon/CryMath/Cry_Matrix34.h"
%template(Matrix34) Matrix34_tpl<f32>;
%import "../../../../CryEngine/CryCommon/CryMath/Cry_Matrix44.h"
%template(Matrix44) Matrix44_tpl<f32>;
%import "../../../../CryEngine/CryCommon/CryMath/Cry_Quat.h"
%template(Quat) Quat_tpl<f32>;
%extend Quat_tpl<f32> {
	Quat_tpl<f32> Normalize()
	{
		return $self->Normalize();
	}

	float GetLength()
	{
		return $self->GetLength();
	}

	static Quat_tpl<f32> CreateQuatFromMatrix(Matrix33_tpl<f32> matrix33)
	{
		return Quat_tpl<f32>(matrix33);
	}

	static Quat_tpl<f32> CreateQuatFromMatrix(Matrix34_tpl<f32> matrix34)
	{
		return Quat_tpl<f32>(matrix34);
	}
}
%template(QuatTS) QuatTS_tpl<f32>;
%import "../../../../CryEngine/CryCommon/CryMath/Cry_XOptimise.h" //<-- throwing errors, because of undefined _MSC_VER!
%ignore Color_tpl<uint8>::set; // <-- until RnD fixes the method defintion
%ignore Color_tpl<float>::set; // <-- until RnD fixes the method definition
%ignore Color_tpl<uint8>::Clamp; // <-- (float-)type-specific method in template o.O
%ignore Color_tpl<uint8>::NormalizeCol; // <-- (float-)type-specific method in template o.O
%ignore Color_tpl<uint8>::RGB2mCIE;
%ignore Color_tpl<uint8>::mCIE2RGB;
%ignore Color_tpl<uint8>::rgb2srgb;
%ignore Color_tpl<uint8>::srgb2rgb;
%ignore Color_tpl<uint8>::adjust_contrast;
%ignore Color_tpl<uint8>::adjust_saturation;
%ignore Color_tpl<uint8>::operator /=;
%ignore Color_tpl<uint8>::operator /;
%import "../../../../CryEngine/CryCommon/CryMath/Cry_Color.h"
%template(ColorB) Color_tpl<uint8>; // [ 0,  255]
%template(ColorF) Color_tpl<float>; // [0.0, 1.0]
%include "../../../../CryEngine/CryCommon/CryMath/Range.h"
%include "../../../../CryEngine/CryCommon/CrySystem/CryVersion.h"

%include "../../../CryEngine/CryCommon/CryExtension/ICryFactory.h"
%include "../../../CryEngine/CryCommon/CryExtension/ICryUnknown.h"

SMART_PTR_TEMPLATE(CPriorityPulseState)
SMART_PTR_TEMPLATE(IAttachmentSkin)
SMART_PTR_TEMPLATE(IBreakDescriptionInfo)
SMART_PTR_TEMPLATE(ICharacterInstance)
SMART_PTR_TEMPLATE(IDialogScriptIterator)
SMART_PTR_TEMPLATE(INetBreakagePlayback)
SMART_PTR_TEMPLATE(INetBreakageSimplePlayback)
SMART_PTR_TEMPLATE(INetSendable)
SMART_PTR_TEMPLATE(INetSendableHook)
SMART_PTR_TEMPLATE(INetworkService)
SMART_PTR_TEMPLATE(IReadXMLSink)
SMART_PTR_TEMPLATE(IRMIMessageBody)
SMART_PTR_TEMPLATE(ISerializableInfo)
SMART_PTR_TEMPLATE(IUIElementIterator)
SMART_PTR_TEMPLATE(IUIEventSystemIterator)
SMART_PTR_TEMPLATE(IWriteXMLSource)
SMART_PTR_TEMPLATE(SCrySessionID)
SMART_PTR_TEMPLATE(SCryUserID)
SMART_PTR_TEMPLATE(SMFXResourceList)

//%include <std_vector.i>
//%template(ProfilerList) std::vector<CFrameProfiler*>;
//%template(EntityList) std::vector<IEntity*>;
//%template(MaterialList) std::vector<IMaterial*>;
//%template(StatObjList) std::vector<IStatObj*>;
//%template(RenderNodeList) std::vector<IRenderNode*>;
//%template(size_tList) std::vector<size_t>;
//%template(uintList) std::vector<unsigned int>;
//%template(unsignedCharList) std::vector<unsigned char>;
//%template(unsignedShortList) std::vector<unsigned short>;
