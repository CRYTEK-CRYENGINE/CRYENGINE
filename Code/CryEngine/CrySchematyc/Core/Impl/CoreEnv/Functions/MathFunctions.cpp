// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CrySerialization/Forward.h>
#include <CryMath/Cry_Math.h>
#include <CryMath/Random.h>
#include <CryMath/Angle.h>

#include <CrySchematyc/CoreAPI.h>

namespace Schematyc
{
namespace MathUtils
{

inline Matrix33 CreateOrthonormalTM33(Vec3 at, Vec3 up = Vec3(0.0f, 0.0f, 1.0f))
{
	SCHEMATYC_ENV_ASSERT(at.IsUnit())
	SCHEMATYC_ENV_ASSERT(up.IsUnit());
	const Vec3 right = at.Cross(up).GetNormalized();
	up = right.Cross(at).GetNormalized();
	return Matrix33::CreateFromVectors(right, at, up);
}

} // MathUtils

namespace Vector2
{

Vec2 Create(float x, float y)
{
	return Vec2(x, y);
}

void Expand(const Vec2& input, float& x, float& y)
{
	x = input.x;
	y = input.y;
}

float Length(const Vec2& input)
{
	return input.GetLength();
}

float LengthSq(const Vec2& input)
{
	return input.GetLengthSquared();
}

float Distance(const Vec2& a, const Vec2& b)
{
	const Vec2 delta = a - b;
	return delta.GetLength();
}

float DistanceSq(const Vec2& a, const Vec2& b)
{
	const Vec2 delta = a - b;
	return delta.GetLengthSquared();
}

Vec2 Normalize(const Vec2& input)
{
	return input.GetNormalized();
}

Vec2 Add(const Vec2& a, const Vec2& b)
{
	return a + b;
}

Vec2 Subtract(const Vec2& a, const Vec2& b)
{
	return a - b;
}

Vec2 Clamp(const Vec2& value, const Vec2& min, const Vec2& max)
{
	return crymath::clamp(value, min, max);
}

Vec2 Scale(const Vec2& input, float scale)
{
	return input * scale;
}

float DotProduct(const Vec2& a, const Vec2& b)
{
	return a.Dot(b);
}

float Angle(const Vec2& a, const Vec2& b)
{
	const float deg = RAD2DEG(atan2(a.y, a.x) - atan2(b.y, b.x));
	return deg > 0.0f ? deg : 360.0f + deg;
}

void ToString(const Vec2& input, CSharedString& output)
{
	Schematyc::ToString(output, input);
}

static void RegisterFunctions(IEnvRegistrar& registrar)
{
	CEnvRegistrationScope scope = registrar.Scope(GetTypeDesc<Vec2>().GetGUID());
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Create, "19f18792-2347-4385-bd90-433c903260aa"_cry_guid, "Create");
		pFunction->SetDescription("Create Vector2");
		pFunction->BindInput(1, 'x', "X");
		pFunction->BindInput(2, 'y', "Y");
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Expand, "e03eebae-2f92-4a1d-b5e3-81d5a69dcabf"_cry_guid, "Expand");
		pFunction->SetDescription("Expand Vector2");
		pFunction->BindInput(1, 'val', "Input", nullptr, Vec2(ZERO)); // #SchematycTODO : Rename 'Value'!
		pFunction->BindOutput(2, 'x', "X");
		pFunction->BindOutput(3, 'y', "Y");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Length, "796a20cd-dc27-47c5-99b3-292f16f46e46"_cry_guid, "Length");
		pFunction->SetDescription("Calculate length of vector");
		pFunction->BindInput(1, 'val', "Input", nullptr, Vec2(ZERO)); // #SchematycTODO : Rename 'Value'!
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&LengthSq, "85f725fa-0676-47f5-ad2e-f2fa2393fae4"_cry_guid, "LengthSq");
		pFunction->SetDescription("Calculate squared length of vector");
		pFunction->BindInput(1, 'val', "Input", nullptr, Vec2(ZERO)); // #SchematycTODO : Rename 'Value'!
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Distance, "6ad3e5fa-715d-4e4a-bd03-ffa5ba7d9d6d"_cry_guid, "Distance");
		pFunction->SetDescription("Calculate distance between two points");
		pFunction->BindInput(1, 'a', "A", nullptr, Vec2(ZERO));
		pFunction->BindInput(2, 'b', "B", nullptr, Vec2(ZERO));
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&DistanceSq, "4f626698-3eae-4a73-81e2-eae835d22688"_cry_guid, "DistanceSq");
		pFunction->SetDescription("Calculate squared distance between two points");
		pFunction->BindInput(1, 'a', "A", nullptr, Vec2(ZERO));
		pFunction->BindInput(2, 'b', "B", nullptr, Vec2(ZERO));
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Normalize, "7aa8e16e-3318-427d-980e-2213c0fdBd13"_cry_guid, "Normalize");
		pFunction->SetDescription("Normalize vector");
		pFunction->BindInput(1, 'val', "Input", nullptr, Vec2(ZERO)); // #SchematycTODO : Rename 'Value'!
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Add, "d1b45d39-bef5-4f18-b7e1-7fad1ea78dd3"_cry_guid, "Add");
		pFunction->SetDescription("Add A to B");
		pFunction->BindInput(1, 'b', "A", nullptr, Vec2(ZERO));
		pFunction->BindInput(2, 'a', "B", nullptr, Vec2(ZERO));
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Subtract, "edc23583-23ee-4a1c-b24b-1fbd6a503b03"_cry_guid, "Subtract");
		pFunction->SetDescription("Subtract B from A");
		pFunction->BindInput(1, 'a', "A", nullptr, Vec2(ZERO));
		pFunction->BindInput(2, 'b', "B", nullptr, Vec2(ZERO));
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Clamp, "f51e6481-2666-4a1c-99e7-c121d6f5682d"_cry_guid, "Clamp");
		pFunction->SetDescription("Clamp the value between min and max");
		pFunction->BindInput(1, 'val', "Value", nullptr, Vec2(ZERO));
		pFunction->BindInput(2, 'min', "Min", nullptr, Vec2(ZERO));
		pFunction->BindInput(3, 'max', "Max", nullptr, Vec2(ZERO));
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Scale, "db715617-aaf7-4f0a-a7d4-ecd0e6967c80"_cry_guid, "Scale");
		pFunction->SetDescription("Scale vector");
		pFunction->BindInput(1, 'val', "Input", nullptr, Vec2(ZERO)); // #SchematycTODO : Rename 'Value'!
		pFunction->BindInput(2, 'scl', "Scale", nullptr, 1.0f);
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&DotProduct, "283b624b-c536-4448-8daf-92f22d0cd02d"_cry_guid, "DotProduct");
		pFunction->SetDescription("Calculate dot product of two vectors");
		pFunction->BindInput(1, 'a', "A", nullptr, Vec2(ZERO));
		pFunction->BindInput(2, 'b', "B", nullptr, Vec2(ZERO));
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Angle, "20d0ed4b-70ef-4210-8349-2966d7619126"_cry_guid, "Angle");
		pFunction->SetDescription("Calculate angle between two vectors");
		pFunction->BindInput(1, 'a', "A", nullptr, Vec2(ZERO));
		pFunction->BindInput(2, 'b', "B", nullptr, Vec2(ZERO));
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&ToString, "548ed62f-76e5-4b44-b247-751195b89605"_cry_guid, "ToString");
		pFunction->SetDescription("Convert Vector2 to String");
		pFunction->BindInput(1, 'val', "Input", nullptr, Vec2(ZERO)); // #SchematycTODO : Rename 'Value'!
		pFunction->BindOutput(2, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
}

} // Vector2

namespace Vector3
{

Vec3 Create(float x, float y, float z)
{
	return Vec3(x, y, z);
}

void Expand(const Vec3& input, float& x, float& y, float& z)
{
	x = input.x;
	y = input.y;
	z = input.z;
}

float Length(const Vec3& input)
{
	return input.GetLength();
}

float LengthSq(const Vec3& input)
{
	return input.GetLengthSquared();
}

float Distance(const Vec3& a, const Vec3& b)
{
	const Vec3 delta = a - b;
	return delta.GetLength();
}

float DistanceSq(const Vec3& a, const Vec3& b)
{
	const Vec3 delta = a - b;
	return delta.GetLengthSquared();
}

Vec3 Normalize(const Vec3& input)
{
	return input.GetNormalized();
}

Vec3 Add(const Vec3& a, const Vec3& b)
{
	return a + b;
}

Vec3 Subtract(const Vec3& a, const Vec3& b)
{
	return a - b;
}

Vec3 Clamp(const Vec3& value, const Vec3& min, const Vec3& max)
{
	return crymath::clamp(value, min, max);
}

Vec3 Scale(const Vec3& input, float scale)
{
	return input * scale;
}

float DotProduct(const Vec3& a, const Vec3& b)
{
	return a.Dot(b);
}

Vec3 CrossProduct(const Vec3& a, const Vec3& b)
{
	return a.Cross(b);
}

Vec3 Project(const Vec3& direction, const Vec3& normal)
{
	return Vec3::CreateProjection(direction, normal);
}

Vec3 Reflect(const Vec3& direction, const Vec3& normal)
{
	return Vec3::CreateReflection(direction, normal);
}

void ToString(const Vec3& input, CSharedString& output)
{
	Schematyc::ToString(output, input);
}

static void RegisterFunctions(IEnvRegistrar& registrar)
{
	CEnvRegistrationScope scope = registrar.Scope(GetTypeDesc<Vec3>().GetGUID());
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Create, "3496016c-8092-4a63-9198-9adce657fd1a"_cry_guid, "Create");
		pFunction->SetDescription("Create Vector3");
		pFunction->BindInput(1, 'x', "X");
		pFunction->BindInput(2, 'y', "Y");
		pFunction->BindInput(3, 'z', "Z");
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Expand, "e0170ced-db1d-4438-ba58-02fa2b713f59"_cry_guid, "Expand");
		pFunction->SetDescription("Expand Vector3");
		pFunction->BindInput(1, 'val', "Input", nullptr, Vec3(ZERO)); // #SchematycTODO : Rename 'Value'!
		pFunction->BindOutput(2, 'x', "X");
		pFunction->BindOutput(3, 'y', "Y");
		pFunction->BindOutput(4, 'z', "Z");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Length, "e8020fdb-a227-4431-b888-816ba10b6bcf"_cry_guid, "Length");
		pFunction->SetDescription("Calculate length of vector");
		pFunction->BindInput(1, 'val', "Input", nullptr, Vec3(ZERO)); // #SchematycTODO : Rename 'Value'!
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&LengthSq, "0705e0d3-fbe6-4e7c-afca-e3739c7df982"_cry_guid, "LengthSq");
		pFunction->SetDescription("Calculate squared length of vector");
		pFunction->BindInput(1, 'val', "Input", nullptr, Vec3(ZERO)); // #SchematycTODO : Rename 'Value'!
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Distance, "6138150A-CCD7-40D8-A696-0100516441D8"_cry_guid, "Distance");
		pFunction->SetDescription("Calculate distance between two points");
		pFunction->BindInput(1, 'a', "A", nullptr, Vec3(ZERO));
		pFunction->BindInput(2, 'b', "B", nullptr, Vec3(ZERO));
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&DistanceSq, "E2A07540-D821-4C11-A1C2-D40C1D179469"_cry_guid, "DistanceSq");
		pFunction->SetDescription("Calculate squared distance between two points");
		pFunction->BindInput(1, 'b', "A", nullptr, Vec3(ZERO));
		pFunction->BindInput(2, 'a', "B", nullptr, Vec3(ZERO));
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Normalize, "d953506a-6710-472d-9017-5a969f936b19"_cry_guid, "Normalize");
		pFunction->SetDescription("Normalize vector");
		pFunction->BindInput(1, 'val', "Input", nullptr, Vec3(ZERO)); // #SchematycTODO : Rename 'Value'!
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Add, "e50a9ee9-99dd-4011-82f7-05c67e013abb"_cry_guid, "Add");
		pFunction->SetDescription("Add A to B");
		pFunction->BindInput(1, 'a', "A", nullptr, Vec3(ZERO));
		pFunction->BindInput(2, 'b', "B", nullptr, Vec3(ZERO));
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Subtract, "dbafe377-fce6-430f-92e5-38b579e9833b"_cry_guid, "Subtract");
		pFunction->SetDescription("Subtract B from A");
		pFunction->BindInput(1, 'a', "A", nullptr, Vec3(ZERO));
		pFunction->BindInput(2, 'b', "B", nullptr, Vec3(ZERO));
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Clamp, "13ad859b-2611-4465-860a-8e6a1327ecd2"_cry_guid, "Clamp");
		pFunction->SetDescription("Clamp the value between min and max");
		pFunction->BindInput(1, 'val', "Value", nullptr, Vec3(ZERO));
		pFunction->BindInput(2, 'min', "Min", nullptr, Vec3(ZERO));
		pFunction->BindInput(3, 'max', "Max", nullptr, Vec3(ZERO));
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}	
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Scale, "03d7b941-57f8-4731-b518-781ea3e1dd16"_cry_guid, "Scale");
		pFunction->SetDescription("Scale vector");
		pFunction->BindInput(1, 'val', "Input", nullptr, Vec3(ZERO)); // #SchematycTODO : Rename 'Value'!
		pFunction->BindInput(2, 'scl', "Scale", nullptr, 1.0f);
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&DotProduct, "b2806327-f747-4dbb-905c-fadc027dbe2c"_cry_guid, "DotProduct");
		pFunction->SetDescription("Calculate dot product of two vectors");
		pFunction->BindInput(1, 'a', "A", nullptr, Vec3(ZERO));
		pFunction->BindInput(2, 'b', "B", nullptr, Vec3(ZERO));
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CrossProduct, "17b6dcf4-6cc9-4a04-a5a9-876857245411"_cry_guid, "CrossProduct");
		pFunction->SetDescription("Calculate cross product of two vectors");
		pFunction->BindInput(1, 'a', "A", nullptr, Vec3(ZERO));
		pFunction->BindInput(2, 'b', "B", nullptr, Vec3(ZERO));
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Project, "{C35F9856-14B4-4180-A000-0A1FF64851F5}"_cry_guid, "Project");
		pFunction->SetDescription("Projects a vector to a plane");
		pFunction->BindInput(1, 'dir', "Direction", nullptr, Vec3(0, 1, 0));
		pFunction->BindInput(2, 'norm', "Normal", nullptr, Vec3(0, 0, 1));
		pFunction->BindOutput(0, 'res', "Result");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Reflect, "{E1C1BD22-1A30-4137-B314-273265DF0093}"_cry_guid, "Reflect");
		pFunction->SetDescription("Reflects the direction along the normal");
		pFunction->BindInput(1, 'dir', "Direction", nullptr, Vec3(0, 1, 0));
		pFunction->BindInput(2, 'norm', "Normal", nullptr, Vec3(0, 0, 1));
		pFunction->BindOutput(0, 'res', "Result");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&ToString, "4564e7fe-adcb-4545-8009-4acc713e305f"_cry_guid, "ToString");
		pFunction->SetDescription("Convert Vector3 to String");
		pFunction->BindInput(1, 'val', "Input", nullptr, Vec3(ZERO)); // #SchematycTODO : Rename 'Value'!
		pFunction->BindOutput(2, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
}

} // Vector3

namespace Rotation
{

CRotation CreateFromAngles3(const CryTransform::CAngles3& angle)
{
	return CRotation(angle);
}

CRotation CreateFromAngles(CryTransform::CAngle x, CryTransform::CAngle y, CryTransform::CAngle z)
{
	return CRotation(CryTransform::CAngles3(x, y, z));
}

CRotation CreateFromAnglesRadians(float x, float y, float z)
{
	return CRotation(CryTransform::CAngles3(CryTransform::CAngle::FromRadians(x), CryTransform::CAngle::FromRadians(y), CryTransform::CAngle::FromRadians(z)));
}

CRotation CreateFromAnglesDegrees(float x, float y, float z)
{
	return CRotation(CryTransform::CAngles3(CryTransform::CAngle::FromDegrees(x), CryTransform::CAngle::FromDegrees(y), CryTransform::CAngle::FromDegrees(z)));
}

CryTransform::CAngles3 ToAngles3(const CRotation& input)
{
	return input.ToAngles();
}

void ToAngles(const CRotation& input, CryTransform::CAngle& x, CryTransform::CAngle& y, CryTransform::CAngle& z)
{
	CryTransform::CAngles3 angles = input.ToAngles();
	x = angles.x;
	y = angles.y;
	z = angles.z;
}

void ToAnglesRadians(const CRotation& input, float& x, float& y, float& z)
{
	CryTransform::CAngles3 angles = input.ToAngles();
	x = angles.x.ToRadians();
	y = angles.y.ToRadians();
	z = angles.z.ToRadians();
}

void ToAnglesDegrees(const CRotation& input, float& x, float& y, float& z)
{
	CryTransform::CAngles3 angles = input.ToAngles();
	x = angles.x.ToDegrees();
	y = angles.y.ToDegrees();
	z = angles.z.ToDegrees();
}

CRotation Look(const Vec3& forward, const Vec3& up)
{
	return CRotation(Quat(MathUtils::CreateOrthonormalTM33(forward.GetNormalizedSafe(Vec3Constants<float>::fVec3_OneY), up.GetNormalizedSafe(Vec3Constants<float>::fVec3_OneZ))));
}

void ToVector3(const CRotation& input, Vec3& right, Vec3& forward, Vec3& up)
{
	const Quat quat = input.ToQuat();
	right = quat.GetColumn0();
	forward = quat.GetColumn1();
	up = quat.GetColumn2();
}

Vec3 TransformVector3(const CRotation& rotation, const Vec3& vector)
{
	return rotation.ToQuat() * vector;
}

void ToString(const CRotation& input, CSharedString& output)
{
	Schematyc::ToString(output, input);
}

inline CRotation Multiply(const CRotation& a, const CRotation& b)
{
	return CryTransform::Multiply(a,b);
}

static void RegisterFunctions(IEnvRegistrar& registrar)
{
	CEnvRegistrationScope scope = registrar.Scope(GetTypeDesc<CRotation>().GetGUID());
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Multiply, "837695ad-ed6e-4003-b0db-f6f0127c4909"_cry_guid, "Multiply");
		pFunction->SetDescription("Multiply A by B");
		pFunction->BindInput(1, 'a', "A");
		pFunction->BindInput(2, 'b', "B");
		pFunction->BindOutput(0, 'res', "Result");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CreateFromAngles3, "{0EEAECF8-C443-48A7-98E2-E254535D5270}"_cry_guid, "CreateFromAngles3");
		pFunction->SetDescription("Create rotation");
		pFunction->BindInput(1, 'ang', "Angles", nullptr, CryTransform::CAngles3());
		pFunction->BindOutput(0, 'res', "Result");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CreateFromAngles, "38ef172c-89cb-4310-812b-c358bccd396a"_cry_guid, "CreateFromAngles");
		pFunction->SetDescription("Create rotation");
		pFunction->BindInput(1, 'x', "X", nullptr, CryTransform::CAngle());
		pFunction->BindInput(2, 'y', "Y", nullptr, CryTransform::CAngle());
		pFunction->BindInput(3, 'z', "Z", nullptr, CryTransform::CAngle());
		pFunction->BindOutput(0, 'res', "Result");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CreateFromAnglesDegrees, "{E82C7D4F-90E8-4601-B228-5EE0B93D03CF}"_cry_guid, "CreateFromAnglesInDegrees");
		pFunction->SetDescription("Create rotation");
		pFunction->BindInput(1, 'x', "X", nullptr, 0.f);
		pFunction->BindInput(2, 'y', "Y", nullptr, 0.f);
		pFunction->BindInput(3, 'z', "Z", nullptr, 0.f);
		pFunction->BindOutput(0, 'res', "Result");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CreateFromAnglesRadians, "{B8289EA7-C682-4A41-8E6B-EA785A0E9587}"_cry_guid, "CreateFromAnglesInRadians");
		pFunction->SetDescription("Create rotation");
		pFunction->BindInput(1, 'x', "X", nullptr, 0.f);
		pFunction->BindInput(2, 'y', "Y", nullptr, 0.f);
		pFunction->BindInput(3, 'z', "Z", nullptr, 0.f);
		pFunction->BindOutput(0, 'res', "Result");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&ToAngles, "{DE75EB12-1EE1-47C9-BFA4-A662171D9880}"_cry_guid, "ToAngles");
		pFunction->SetDescription("Get angles from rotation");
		pFunction->BindInput(1, 'val', "Input");
		pFunction->BindOutput(2, 'x', "X");
		pFunction->BindOutput(3, 'y', "Y");
		pFunction->BindOutput(4, 'z', "Z");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&ToAnglesRadians, "{D5B434A9-28F6-4B08-A7EE-B8F599C14218}"_cry_guid, "ToAnglesInRadians");
		pFunction->SetDescription("Get angles in radians from rotation");
		pFunction->BindInput(1, 'val', "Input");
		pFunction->BindOutput(2, 'x', "X");
		pFunction->BindOutput(3, 'y', "Y");
		pFunction->BindOutput(4, 'z', "Z");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&ToAnglesDegrees, "{EDA19590-E087-42CD-8FC2-BACF4772FCD7}"_cry_guid, "ToAnglesInDegrees");
		pFunction->SetDescription("Get angles in degrees from rotation");
		pFunction->BindInput(1, 'val', "Input");
		pFunction->BindOutput(2, 'x', "X");
		pFunction->BindOutput(3, 'y', "Y");
		pFunction->BindOutput(4, 'z', "Z");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&ToAngles3, "{B34513D1-1C5C-4D4B-B6C2-7103DAB6A959}"_cry_guid, "ToAngles3");
		pFunction->SetDescription("Get angles from rotation");
		pFunction->BindInput(1, 'val', "Input");
		pFunction->BindOutput(0, 'ang', "Angles");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Look, "0261e9ac-0b2e-4e71-b04d-9027a9cbd3c8"_cry_guid, "Look");
		pFunction->SetDescription("Create rotation from two direction vectors");
		pFunction->BindInput(1, 'fwd', "Forward", nullptr, Vec3Constants<float>::fVec3_OneY);
		pFunction->BindInput(2, 'up', "Up", nullptr, Vec3Constants<float>::fVec3_OneZ);
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&ToVector3, "e53f4ebd-db4a-4218-8f97-6480e6308965"_cry_guid, "ToVector3");
		pFunction->SetDescription("Convert rotation to three direction vectors");
		pFunction->BindInput(1, 'val', "Input"); // #SchematycTODO : Rename 'Value'!
		pFunction->BindOutput(2, 'rt', "Right");
		pFunction->BindOutput(3, 'fwd', "Forward");
		pFunction->BindOutput(4, 'up', "Up");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&TransformVector3, "1abea881-67ed-43b5-8e18-2ff79623e692"_cry_guid, "TransformVector3");
		pFunction->SetDescription("Use rotation to transform vector");
		pFunction->BindInput(1, 'rot', "Rotation");
		pFunction->BindInput(2, 'vec', "Vector");
		pFunction->BindOutput(0, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&ToString, "F2CAE291-9373-482C-9554-36D9FF49BE08"_cry_guid, "ToString");
		pFunction->SetDescription("Convert rotation to string");
		pFunction->BindInput(1, 'val', "Input"); // #SchematycTODO : Rename 'Value'!
		pFunction->BindOutput(2, 'res', "Output"); // #SchematycTODO : Rename 'Result'!
		scope.Register(pFunction);
	}
}

} // Rotation

namespace Angle
{
	CryTransform::CAngle CreateFromDegrees(float angle)
	{
		return CryTransform::CAngle::FromDegrees(angle);
	}

	CryTransform::CAngle CreateFromRadians(float angle)
	{
		return CryTransform::CAngle::FromRadians(angle);
	}

	float ToRadians(CryTransform::CAngle angle)
	{
		return angle.ToRadians();
	}

	float ToDegrees(CryTransform::CAngle angle)
	{
		return angle.ToDegrees();
	}

	CryTransform::CAngle Add(CryTransform::CAngle a, CryTransform::CAngle b)
	{
		return a + b;
	}

	CryTransform::CAngle Subtract(CryTransform::CAngle a, CryTransform::CAngle b)
	{
		return a - b;
	}

	CryTransform::CAngle Multiply(CryTransform::CAngle a, float b)
	{
		return a * b;
	}

	CryTransform::CAngle Clamp(CryTransform::CAngle value, CryTransform::CAngle min, CryTransform::CAngle max)
	{
		return CryTransform::CAngle::FromRadians(crymath::clamp(value.ToRadians(), min.ToRadians(), max.ToRadians()));
	}

	CryTransform::CAngle Divide(CryTransform::CAngle a, float b)
	{
		return b != 0.0f ? a / b : CryTransform::CAngle();
	}

	CryTransform::CAngle Modulus(CryTransform::CAngle a, CryTransform::CAngle b)
	{
		return CryTransform::CAngle::FromRadians(b.ToRadians() != 0.0f ? fmodf(a.ToRadians(), b.ToRadians()) : 0.f);
	}

	float Sin(CryTransform::CAngle x)
	{
		return sin_tpl(x.ToRadians());
	}

	CryTransform::CAngle ArcSin(float x)
	{
		return CryTransform::CAngle::FromRadians(asin_tpl(x));
	}

	float Cos(CryTransform::CAngle x)
	{
		return cos_tpl(x.ToRadians());
	}

	CryTransform::CAngle ArcCos(float x)
	{
		return CryTransform::CAngle::FromRadians(acos_tpl(x));
	}

	float Tan(CryTransform::CAngle x)
	{
		return tan_tpl(x.ToRadians());
	}

	CryTransform::CAngle ArcTan(float x)
	{
		return CryTransform::CAngle::FromRadians(atan_tpl(x));
	}

	bool Equal(CryTransform::CAngle a, CryTransform::CAngle b)
	{
		return a.ToRadians() == b.ToRadians();
	}

	bool NotEqual(CryTransform::CAngle a, CryTransform::CAngle b)
	{
		return a.ToRadians() != b.ToRadians();
	}

	bool LessThan(CryTransform::CAngle a, CryTransform::CAngle b)
	{
		return a.ToRadians() < b.ToRadians();
	}

	bool LessThanOrEqual(CryTransform::CAngle a, CryTransform::CAngle b)
	{
		return a.ToRadians() <= b.ToRadians();
	}

	bool GreaterThan(CryTransform::CAngle a, CryTransform::CAngle b)
	{
		return a.ToRadians() > b.ToRadians();
	}

	bool GreaterThanOrEqual(CryTransform::CAngle a, CryTransform::CAngle b)
	{
		return a.ToRadians() >= b.ToRadians();
	}

	static void RegisterFunctions(IEnvRegistrar& registrar)
	{
		CEnvRegistrationScope scope = registrar.Scope(GetTypeDesc<CryTransform::CAngle>().GetGUID());
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CreateFromDegrees, "{0B430180-7923-407D-9F7C-1253C56A34DA}"_cry_guid, "CreateFromDegrees");
			pFunction->SetDescription("Creates an angle from a float measured in degrees");
			pFunction->BindInput(1, 'a', "Angle", nullptr, 0.f);
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CreateFromRadians, "{303A2F39-9FE8-40B9-A60B-6A43C9CCF138}"_cry_guid, "CreateFromRadians");
			pFunction->SetDescription("Creates an angle from a float measured in radians");
			pFunction->BindInput(1, 'a', "Angle", nullptr, 0.f);
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&ToDegrees, "{8F3F2782-AF57-4CFC-98AF-BC980DD0DCD9}"_cry_guid, "ToDegrees");
			pFunction->SetDescription("Converts an angle to degrees");
			pFunction->BindInput(1, 'a', "Angle", nullptr, CryTransform::CAngle());
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&ToRadians, "{D45965D5-8195-4DEE-8174-87288A6C18C3}"_cry_guid, "ToRadians");
			pFunction->SetDescription("Converts an angle to radians");
			pFunction->BindInput(1, 'a', "Angle", nullptr, CryTransform::CAngle());
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Sin, "{51614186-AA32-48C9-8AC9-C726A951071A}"_cry_guid, "Sin");
			pFunction->SetDescription("Calculate sine of X (degrees)");
			pFunction->BindInput(1, 'x', "X");
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&ArcSin, "{C99E8C0D-4FC7-491F-A986-EFBADE88808B}"_cry_guid, "ArcSin");
			pFunction->SetDescription("Calculate arc-sine of X (degrees)");
			pFunction->BindInput(1, 'x', "X");
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Cos, "{BE50C456-D8B0-468D-9D2E-D2589606CABB}"_cry_guid, "Cos");
			pFunction->SetDescription("Calculate cosine of X (degrees)");
			pFunction->BindInput(1, 'x', "X");
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&ArcCos, "{2F427C96-534A-481F-A6FE-6A93913167F4}"_cry_guid, "ArcCos");
			pFunction->SetDescription("Calculate arc-cosine of X (degrees)");
			pFunction->BindInput(1, 'x', "X");
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Tan, "{CF2016B2-4B89-49DB-ADA6-CA132E816B94}"_cry_guid, "Tan");
			pFunction->SetDescription("Calculate tangent of X (degrees)");
			pFunction->BindInput(1, 'x', "X");
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&ArcTan, "{B8161CC3-E720-49DA-A30D-DCF4EE21B1EE}"_cry_guid, "ArcTan");
			pFunction->SetDescription("Calculate arc-tangent of X (degrees)");
			pFunction->BindInput(1, 'x', "X");
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Add, "{C3EEA3D7-4C40-4522-96D6-0ABB991DD362}"_cry_guid, "Add");
			pFunction->SetDescription("Add A to B");
			pFunction->BindInput(1, 'a', "A");
			pFunction->BindInput(2, 'b', "B");
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Subtract, "{D0FAEE13-3B34-428F-A0D5-CDE3B88E0A6B}"_cry_guid, "Subtract");
			pFunction->SetDescription("Subtract B from A");
			pFunction->BindInput(1, 'a', "A");
			pFunction->BindInput(2, 'b', "B");
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Multiply, "{5AED5C17-5EEA-4EB9-AA37-1493FD8D85E0}"_cry_guid, "Multiply");
			pFunction->SetDescription("Multiply A by B");
			pFunction->BindInput(1, 'a', "A");
			pFunction->BindInput(2, 'b', "B");
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Divide, "{8EEFDAE8-5097-4169-9DAC-F3343D31DE4D}"_cry_guid, "Divide");
			pFunction->SetDescription("Divide A by B");
			pFunction->BindInput(1, 'a', "A");
			pFunction->BindInput(2, 'b', "B");
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Equal, "{7A8B3711-7888-4A7E-9C77-1B898D5AC62C}"_cry_guid, "Equal");
			pFunction->SetDescription("Checks if A and B are equal");
			pFunction->BindInput(1, 'a', "A", nullptr, CryTransform::CAngle());
			pFunction->BindInput(2, 'b', "B", nullptr, CryTransform::CAngle());
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&NotEqual, "{4B2C6C3F-FB28-4DAB-9F69-6EF7312FBB6D}"_cry_guid, "NotEqual");
			pFunction->SetDescription("Checks if A and B are not equal");
			pFunction->BindInput(1, 'a', "A", nullptr, CryTransform::CAngle());
			pFunction->BindInput(2, 'b', "B", nullptr, CryTransform::CAngle());
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&LessThan, "{DAB4347D-C73F-4FB9-B7BC-D46064DCD0C1}"_cry_guid, "LessThan");
			pFunction->SetDescription("Checks if A is less than B");
			pFunction->BindInput(1, 'a', "A", nullptr, CryTransform::CAngle());
			pFunction->BindInput(2, 'b', "B", nullptr, CryTransform::CAngle());
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&LessThanOrEqual, "{64119B30-AB28-47B2-A77C-E6A8359CD2DE}"_cry_guid, "LessThanOrEqual");
			pFunction->SetDescription("Checks if A is less than or equals B");
			pFunction->BindInput(1, 'a', "A", nullptr, CryTransform::CAngle());
			pFunction->BindInput(2, 'b', "B", nullptr, CryTransform::CAngle());
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&GreaterThan, "{B2ACF303-CC8A-4414-8321-33E57E63066C}"_cry_guid, "GreaterThan");
			pFunction->SetDescription("Checks if A is greater than B");
			pFunction->BindInput(1, 'a', "A", nullptr, CryTransform::CAngle());
			pFunction->BindInput(2, 'b', "B", nullptr, CryTransform::CAngle());
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&GreaterThanOrEqual, "{9A2418F5-DB98-4C12-9D53-2A1AF7F770E6}"_cry_guid, "GreaterThanOrEqual");
			pFunction->SetDescription("Checks if A is greater than or equals B");
			pFunction->BindInput(1, 'a', "A", nullptr, CryTransform::CAngle());
			pFunction->BindInput(2, 'b', "B", nullptr, CryTransform::CAngle());
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
	}

} // Angle

namespace Angles3
{
	CryTransform::CAngles3 Create(CryTransform::CAngle x, CryTransform::CAngle y, CryTransform::CAngle z)
	{
		return CryTransform::CAngles3(x, y, z);
	}

	CryTransform::CAngles3 CreateFromDegrees(float x, float y, float z)
	{
		return CryTransform::CAngles3(CryTransform::CAngle::FromDegrees(x), CryTransform::CAngle::FromDegrees(y), CryTransform::CAngle::FromDegrees(z));
	}

	CryTransform::CAngles3 CreateFromRadians(float x, float y, float z)
	{
		return CryTransform::CAngles3(CryTransform::CAngle::FromRadians(x), CryTransform::CAngle::FromRadians(y), CryTransform::CAngle::FromRadians(z));
	}

	void ToDegrees(const CryTransform::CAngles3& angles, float& x, float& y, float& z)
	{
		x = angles.x.ToDegrees();
		y = angles.y.ToDegrees();
		z = angles.z.ToDegrees();
	}

	void ToRadians(const CryTransform::CAngles3& angles, float& x, float& y, float& z)
	{
		x = angles.x.ToRadians();
		y = angles.y.ToRadians();
		z = angles.z.ToRadians();
	}

	void Expand(const CryTransform::CAngles3& angles, CryTransform::CAngle& x, CryTransform::CAngle& y, CryTransform::CAngle& z)
	{
		x = angles.x;
		y = angles.y;
		z = angles.z;
	}

	static void RegisterFunctions(IEnvRegistrar& registrar)
	{
		CEnvRegistrationScope scope = registrar.Scope(GetTypeDesc<CryTransform::CAngles3>().GetGUID());
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Create, "{B380F269-5C5B-4475-9445-721C8D842474}"_cry_guid, "Create");
			pFunction->SetDescription("Creates an angle from a float measured in degrees");
			pFunction->BindInput(1, 'x', "x", nullptr, CryTransform::CAngle());
			pFunction->BindInput(2, 'y', "y", nullptr, CryTransform::CAngle());
			pFunction->BindInput(3, 'z', "z", nullptr, CryTransform::CAngle());
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CreateFromDegrees, "{88AA9581-6D66-4361-9ACD-173E3581DE53}"_cry_guid, "CreateFromDegrees");
			pFunction->SetDescription("Creates a 3D angle from floats measured in degrees");
			pFunction->BindInput(1, 'x', "X", nullptr, 0.f);
			pFunction->BindInput(2, 'y', "Y", nullptr, 0.f);
			pFunction->BindInput(3, 'z', "Z", nullptr, 0.f);
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CreateFromRadians, "{711A55B7-7295-465E-A914-871A6FC83E47}"_cry_guid, "CreateFromRadians");
			pFunction->SetDescription("Creates a 3D angle from floats measured in radians");
			pFunction->BindInput(1, 'x', "X", nullptr, 0.f);
			pFunction->BindInput(2, 'y', "Y", nullptr, 0.f);
			pFunction->BindInput(3, 'z', "Z", nullptr, 0.f);
			pFunction->BindOutput(0, 'res', "Result");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&ToDegrees, "{76CCFBE0-1B06-432F-B772-6741E3E39C65}"_cry_guid, "ToDegrees");
			pFunction->SetDescription("Converts a 3D angle to degrees");
			pFunction->BindInput(1, 'ang', "Angle", nullptr, CryTransform::CAngles3());
			pFunction->BindOutput(2, 'x', "X");
			pFunction->BindOutput(3, 'y', "Y");
			pFunction->BindOutput(4, 'z', "Z");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&ToRadians, "{C7E01464-D6D8-4D97-8795-7637703E1E81}"_cry_guid, "ToRadians");
			pFunction->SetDescription("Converts a 3D angle to radians");
			pFunction->BindInput(1, 'ang', "Angle", nullptr, CryTransform::CAngles3());
			pFunction->BindOutput(2, 'x', "X");
			pFunction->BindOutput(3, 'y', "Y");
			pFunction->BindOutput(4, 'z', "Z");
			scope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Expand, "{64F9B46E-E45F-4BE8-AD32-8E00C5BBB833}"_cry_guid, "Expand");
			pFunction->SetDescription("Expands a 3D angles structure into three angles");
			pFunction->BindInput(1, 'a', "Angle", nullptr, CryTransform::CAngles3());
			pFunction->BindOutput(2, 'x', "X");
			pFunction->BindOutput(3, 'y', "Y");
			pFunction->BindOutput(4, 'z', "Z");
			scope.Register(pFunction);
		}
	}

} // Angles3

namespace Transform
{

CTransform Create(const Vec3& translation, const CRotation& rotation,const Vec3& scale)
{
	return CTransform(translation, rotation, scale);
}

void Expand(const CTransform& input, Vec3& translation, CRotation& rotation,Vec3& scale)
{
	translation = input.GetTranslation();
	rotation = input.GetRotation();
	scale = input.GetScale();
}

Vec3 TransformVector3(const CTransform& rotation, const Vec3& vector)
{
	return rotation.ToMatrix34() * vector;
}

inline CTransform Multiply(const CTransform& a, const CTransform& b)
{
	return CryTransform::Multiply(a,b);
}

static void RegisterFunctions(IEnvRegistrar& registrar)
{
	CEnvRegistrationScope scope = registrar.Scope(GetTypeDesc<CTransform>().GetGUID());
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Multiply, "79f1cc37-8283-49e9-93bd-8d9c9c9b3715"_cry_guid, "Multiply");
		pFunction->SetDescription("Multiply A by B");
		pFunction->BindInput(1, 'a', "A");
		pFunction->BindInput(2, 'b', "B");
		pFunction->BindOutput(0, 'res', "Result");
		scope.Register(pFunction);
	} 
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Create, "b901a1de-725e-4eb4-b7e8-bfdcb82a4bd5"_cry_guid, "Create");
		pFunction->SetDescription("Create transform");
		pFunction->BindInput(1, 'pos', "Position", nullptr, Vec3(ZERO));
		pFunction->BindInput(2, 'rot', "Rotation");
		pFunction->BindInput(3, 'scl', "Scale",nullptr, Vec3(1.0f,1.0f,1.0f));
		pFunction->BindOutput(0, 'res', "Result");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Expand, "0a70c719-9e3b-4fac-a47b-468ecabe9b56"_cry_guid, "Expand");
		pFunction->SetDescription("Expand transform");
		pFunction->BindInput(1, 'val', "Value");
		pFunction->BindOutput(2, 'trn', "Translation");
		pFunction->BindOutput(3, 'rot', "Rotation");
		pFunction->BindOutput(4, 'scl', "Scale");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&TransformVector3, "88d9848b-c40c-4a19-ab8a-950da20267eb"_cry_guid, "TransformVector3");
		pFunction->SetDescription("Transform vector");
		pFunction->BindInput(1, 'trn', "Transform");
		pFunction->BindInput(2, 'vec', "Vector");
		pFunction->BindOutput(0, 'res', "Result");
		scope.Register(pFunction);
	}
}

} // Transform

void RegisterMathFunctions(IEnvRegistrar& registrar)
{
	Vector2::RegisterFunctions(registrar);
	Vector3::RegisterFunctions(registrar);
	Rotation::RegisterFunctions(registrar);
	Angle::RegisterFunctions(registrar);
	Angles3::RegisterFunctions(registrar);
	Transform::RegisterFunctions(registrar);
}

} // Schematyc

CRY_STATIC_AUTO_REGISTER_FUNCTION(&Schematyc::RegisterMathFunctions)
