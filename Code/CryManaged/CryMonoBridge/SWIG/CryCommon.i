%include "CryEngine.swig"

%{
#include <Cry3DEngine/CGF/CryHeaders.h>
#include <CryExtension/ICryFactory.h>
#include <CryExtension/ICryFactoryRegistry.h>
#include <CryAnimation/IAttachment.h>
#include <CryAudio/Dialog/IDialogSystem.h>
#include <CryFlowGraph/IFlowGraphModuleManager.h>
#include <CrySystem/XML/IReadWriteXMLSink.h>
#include <CrySystem/Scaleform/IFlashUI.h>
#include <CryAISystem/IPathfinder.h>
#include <CryAction/IMaterialEffects.h>
#include <CryCore/CryTypeInfo.h>
#include <CryLobby/ICryStats.h>
%}

%feature("nspace", 1);

//%import "../../../CryEngine/CryCommon/IEntity.h"

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
%typemap(cscode) Vec2_tpl<f32>
%{
	public static Vec2 operator +(Vec2 a, Vec2 b) { return op_Plus(a, b); }
	public static Vec2 operator -(Vec2 a, Vec2 b) { return op_Minus(a, b); }
	public static Vec2 operator *(Vec2 v, float f) { return v.op_Multiply(f); }
	public static Vec2 operator /(Vec2 v, float f) { return v.op_Divide(f); }
	public static Vec2 operator -(Vec2 v) { return v.op_Minus(); }
%}
%extend Vec2_tpl<f32>{
	static Vec2_tpl<f32> op_Minus(Vec2_tpl<f32> a, Vec2_tpl<f32> b) { return a - b; }
	static Vec2_tpl<f32> op_Plus(Vec2_tpl<f32> a, Vec2_tpl<f32> b) { return a + b; }
}
%import "../../../../CryEngine/CryCommon/CryMath/Cry_Vector2.h"
%template(Vec2) Vec2_tpl<f32>;
%typemap(cscode) Vec3_tpl<f32>
%{
	public static Vec3 operator +(Vec3 a, Vec3 b) { return op_Plus(a, b); }
	public static Vec3 operator -(Vec3 a, Vec3 b) { return op_Minus(a, b); }
	public static Vec3 operator *(Vec3 v, float f) { return v.op_Multiply(f); }
	public static Vec3 operator /(Vec3 v, float f) { return v.op_Divide(f); }
	public static Vec3 operator -(Vec3 v) { return v.op_Minus(); }
	public static Vec3 operator *(Vec3 v, Quat q) { return op_MultiplyVQ(v, q); }
	public static Vec3 operator *(Quat q, Vec3 v) { return op_MultiplyQV(q, v); }
	public static Vec3 operator %(Vec3 a, Vec3 b) { return op_AndVV(a, b); }
	
	public static Vec3 Zero { get { return new Vec3(0, 0, 0); } }
	public static Vec3 One { get{ return new Vec3 (1, 1, 1);} }
	public static Vec3 Up { get{ return new Vec3 (0, 0, 1);} }
	public static Vec3 Forward { get{ return new Vec3 (0, 1, 0);} }

	public Vec3 Normalized { get { return normalized (); } }
	public float Magnitude { get { return len (); } }
%}
%extend Vec3_tpl<f32>{
	static Vec3_tpl<f32> op_Minus(Vec3_tpl<f32> a, Vec3_tpl<f32> b) { return a - b; }
	static Vec3_tpl<f32> op_Plus(Vec3_tpl<f32> a, Vec3_tpl<f32> b) { return a + b; }
	static Vec3_tpl<f32> op_MultiplyVQ(Vec3_tpl<f32> v, Quat_tpl<f32> q) { return v * q; }
	static Vec3_tpl<f32> op_MultiplyQV(Quat_tpl<f32> q, Vec3_tpl<f32> v) { return q * v; }
	static Vec3_tpl<f32> op_AndVV(Vec3_tpl<f32> a, Vec3_tpl<f32> b) { return a % b; }
}
%import "../../../../CryEngine/CryCommon/CryMath/Cry_Vector3.h"
%template(Vec3) Vec3_tpl<f32>;
%typemap(cscode) Ang3_tpl<f32>
%{
	public static Ang3 operator +(Ang3 a, Ang3 b) { return op_Plus(a, b); }
	public static Ang3 operator -(Ang3 a, Ang3 b) { return op_Minus(a, b); }
	public static Ang3 operator *(Ang3 v, float f) { return v.op_Multiply(f); }
%}
%extend Ang3_tpl<f32>{
	static Ang3_tpl<f32> op_Minus(Ang3_tpl<f32> a, Ang3_tpl<f32> b) { return a - b; }
	static Ang3_tpl<f32> op_Plus(Ang3_tpl<f32> a, Ang3_tpl<f32> b) { return a + b; }
	static Ang3_tpl<f32> op_Multiply(Ang3_tpl<f32> v, float f) { return v * f; }
}
%template(Ang3) Ang3_tpl<f32>;
%typemap(cscode) Vec4_tpl<f32>
%{
	public static Vec4 operator +(Vec4 a, Vec4 b) { return op_Plus(a, b); }
	public static Vec4 operator -(Vec4 a, Vec4 b) { return op_Minus(a, b); }
	public static Vec4 operator *(Vec4 v, float f) { return v.op_Multiply(f); }
	public static Vec4 operator /(Vec4 v, float f) { return v.op_Divide(f); }
%}
%extend Vec4_tpl<f32>{
	static Vec4_tpl<f32> op_Minus(Vec4_tpl<f32> a, Vec4_tpl<f32> b) { return a - b; }
	static Vec4_tpl<f32> op_Plus(Vec4_tpl<f32> a, Vec4_tpl<f32> b) { return a + b; }
}
%import "../../../../CryEngine/CryCommon/CryMath/Cry_Vector4.h"
%template(Vec4) Vec4_tpl<f32>;
%import "../../../../CryEngine/CryCommon/CryMath/Cry_MatrixDiag.h"
%template(Diag33) Diag33_tpl<f32>;
%typemap(cscode) Matrix33_tpl<f32>
%{
	public static Matrix34 operator *(Matrix33 m1, Matrix34 m2) { return op_MultiplyM33M34(m1, m2); }
%}
%extend Matrix33_tpl<f32> {
public:
	static Matrix34 op_MultiplyM33M34(Matrix33 m1, Matrix34 m2) { return m1 * m2; }
}
%import "../../../../CryEngine/CryCommon/CryMath/Cry_Matrix33.h"
%template(Matrix33) Matrix33_tpl<f32>;
%typemap(cscode) Matrix34_tpl<f32>
%{
	public static Matrix34 operator *(Matrix34 a, Matrix34 b) { return op_MultiplyMM(a, b); }
	public static Vec3 operator *(Matrix34 a, Vec3 b) { return op_MultiplyMV(a, b); }
	public static Matrix34 operator *(Matrix34 a, float b) { return op_MultiplyMF(a, b); }
%}
%extend Matrix34_tpl<f32>{
	static Matrix34_tpl<f32> op_MultiplyMM(Matrix34_tpl<f32> a, Matrix34_tpl<f32> b) { return a * b; }
	static Vec3_tpl<f32> op_MultiplyMV(Matrix34_tpl<f32> a, Vec3_tpl<f32> b) { return a * b; }
	static Matrix34_tpl<f32> op_MultiplyMF(Matrix34_tpl<f32> a, float b) { return a * b; }
}
%import "../../../../CryEngine/CryCommon/CryMath/Cry_Matrix34.h"
%template(Matrix34) Matrix34_tpl<f32>;
%typemap(cscode) Matrix44_tpl<f32>
%{
	public static Matrix44 operator *(Matrix44 a, Matrix44 b) { return op_MultiplyMM(a, b); }
%}
%extend Matrix44_tpl<f32>{
	static Matrix44_tpl<f32> op_MultiplyMM(Matrix44_tpl<f32> a, Matrix44_tpl<f32> b) { return a * b; }
}
%import "../../../../CryEngine/CryCommon/CryMath/Cry_Matrix44.h"
%template(Matrix44) Matrix44_tpl<f32>;
%typemap(cscode) Quat_tpl<f32>
%{
	public static Quat operator +(Quat a, Quat b) { return op_Plus(a, b); }
	public static Quat operator -(Quat a, Quat b) { return op_Minus(a, b); }
	public static Quat operator *(Quat a, Quat b) { return op_MultiplyQQ(a, b); }
	public static Quat operator *(Quat a, float f) { return op_MultiplyQF(a, f); }
	public static Quat operator /(Quat a, float f) { return op_DivideQF(a, f); }
	
	public static Quat Identity { get { return Quat.CreateIdentity(); } }
	public Vec3 ForwardDirection { get { return new Vec3(GetFwdX(), GetFwdY(), GetFwdZ()); } }
%}
%extend Quat_tpl<f32>{
	static Quat_tpl<f32> op_Plus(Quat_tpl<f32> a, Quat_tpl<f32> b) { return a + b; }
	static Quat_tpl<f32> op_Minus(Quat_tpl<f32> a, Quat_tpl<f32> b) { return a - b; }
	static Quat_tpl<f32> op_MultiplyQQ(Quat_tpl<f32> a, Quat_tpl<f32> b) { return a * b; }
	static Quat_tpl<f32> op_MultiplyQF(Quat_tpl<f32> a, float f) { return a * f; }
	static Quat_tpl<f32> op_DivideQF(Quat_tpl<f32> a, float f) { return a / f; }
}
%import "../../../../CryEngine/CryCommon/CryMath/Cry_Quat.h"
%template(Quat) Quat_tpl<f32>;
%template(QuatTS) QuatTS_tpl<f32>;
%import "../../../../CryEngine/CryCommon/CryMath/Cry_HWVector3.h"
%import "../../../../CryEngine/CryCommon/CryMath/Cry_HWMatrix.h"
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

%include "../../../CryEngine/CryCommon/CryExtension/CryCreateClassInstance.h"
%include "../../../CryEngine/CryCommon/CryExtension/CryGUID.h"
%include "../../../CryEngine/CryCommon/CryExtension/CryTypeID.h"
%include "../../../CryEngine/CryCommon/CryExtension/ICryFactory.h"
%include "../../../CryEngine/CryCommon/CryExtension/ICryFactoryRegistry.h"
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
%template(SmartObjectNavDataPtr) _smart_ptr<PathPointDescriptor::SmartObjectNavData>;
SMART_PTR_TEMPLATE(SCrySessionID)
SMART_PTR_TEMPLATE(SCryUserID)
SMART_PTR_TEMPLATE(SMFXResourceList)
SMART_PTR_TEMPLATE(IParticleEffectIterator)

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
