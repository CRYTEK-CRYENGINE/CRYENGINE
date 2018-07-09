// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CrySerialization/Forward.h>
#include <CrySchematyc2/Any.h>
#include <CrySchematyc2/Env/EnvFunctionDescriptor.h>

#include "UnitTests/UnitTestRegistrar.h"

namespace Schematyc2
{
	namespace EnvFunctionDescriptorUnitTests
	{
		struct SParam0
		{
			void Serialize(Serialization::IArchive&) {}

			inline bool operator == (const SParam0& rhs) const
			{
				return value == rhs.value;
			}

			int value;
		};

		struct SParam1
		{
			void Serialize(Serialization::IArchive&) {}

			inline bool operator == (const SParam1& rhs) const
			{
				return value == rhs.value;
			}

			int value;
		};

		struct SParam2
		{
			void Serialize(Serialization::IArchive&) {}

			inline bool operator == (const SParam2& rhs) const
			{
				return value == rhs.value;
			}

			int value;
		};

		struct SParam3
		{
			void Serialize(Serialization::IArchive&) {}

			inline bool operator == (const SParam3& rhs) const
			{
				return value == rhs.value;
			}

			int value;
		};

		struct SParam4
		{
			void Serialize(Serialization::IArchive&) {}

			inline bool operator == (const SParam4& rhs) const
			{
				return value == rhs.value;
			}

			int value;
		};

		struct SParam5
		{
			void Serialize(Serialization::IArchive&) {}

			inline bool operator == (const SParam5& rhs) const
			{
				return value == rhs.value;
			}

			int value;
		};

		struct SParam6
		{
			void Serialize(Serialization::IArchive&) {}

			inline bool operator == (const SParam6& rhs) const
			{
				return value == rhs.value;
			}

			int value;
		};

		struct SParam7
		{
			void Serialize(Serialization::IArchive&) {}

			inline bool operator == (const SParam7& rhs) const
			{
				return value == rhs.value;
			}

			int value;
		};

		struct SParam8
		{
			void Serialize(Serialization::IArchive&) {}

			inline bool operator == (const SParam8& rhs) const
			{
				return value == rhs.value;
			}

			int value;
		};

		struct SParam9
		{
			void Serialize(Serialization::IArchive&) {}

			inline bool operator == (const SParam9& rhs) const
			{
				return value == rhs.value;
			}

			int value;
		};

		SEnvFunctionResult GlobalFunction00(const SEnvFunctionContext&) { return SEnvFunctionResult(); }
		SEnvFunctionResult GlobalFunction01(const SEnvFunctionContext&, SParam0) { return SEnvFunctionResult(); }
		SEnvFunctionResult GlobalFunction02(const SEnvFunctionContext&, SParam0, SParam1) { return SEnvFunctionResult(); }
		SEnvFunctionResult GlobalFunction03(const SEnvFunctionContext&, SParam0, SParam1, SParam2) { return SEnvFunctionResult(); }
		SEnvFunctionResult GlobalFunction04(const SEnvFunctionContext&, SParam0, SParam1, SParam2, SParam3) { return SEnvFunctionResult(); }
		SEnvFunctionResult GlobalFunction05(const SEnvFunctionContext&, SParam0, SParam1, SParam2, SParam3, SParam4) { return SEnvFunctionResult(); }
		SEnvFunctionResult GlobalFunction06(const SEnvFunctionContext&, SParam0, SParam1, SParam2, SParam3, SParam4, SParam5) { return SEnvFunctionResult(); }
		SEnvFunctionResult GlobalFunction07(const SEnvFunctionContext&, SParam0, SParam1, SParam2, SParam3, SParam4, SParam5, SParam6) { return SEnvFunctionResult(); }
		SEnvFunctionResult GlobalFunction08(const SEnvFunctionContext&, SParam0, SParam1, SParam2, SParam3, SParam4, SParam5, SParam6, SParam7) { return SEnvFunctionResult(); }
		SEnvFunctionResult GlobalFunction09(const SEnvFunctionContext&, SParam0, SParam1, SParam2, SParam3, SParam4, SParam5, SParam6, SParam7, SParam8) { return SEnvFunctionResult(); }
		SEnvFunctionResult GlobalFunction10(const SEnvFunctionContext&, SParam0, SParam1, SParam2, SParam3, SParam4, SParam5, SParam6, SParam7, SParam8, SParam9) { return SEnvFunctionResult(); }

		struct SObject
		{
			SEnvFunctionResult MemberFunction00(const SEnvFunctionContext&) { return SEnvFunctionResult(); }
			SEnvFunctionResult MemberFunction01(const SEnvFunctionContext&, SParam0) { return SEnvFunctionResult(); }
			SEnvFunctionResult MemberFunction02(const SEnvFunctionContext&, SParam0, SParam1) { return SEnvFunctionResult(); }
			SEnvFunctionResult MemberFunction03(const SEnvFunctionContext&, SParam0, SParam1, SParam2) { return SEnvFunctionResult(); }
			SEnvFunctionResult MemberFunction04(const SEnvFunctionContext&, SParam0, SParam1, SParam2, SParam3) { return SEnvFunctionResult(); }
			SEnvFunctionResult MemberFunction05(const SEnvFunctionContext&, SParam0, SParam1, SParam2, SParam3, SParam4) { return SEnvFunctionResult(); }
			SEnvFunctionResult MemberFunction06(const SEnvFunctionContext&, SParam0, SParam1, SParam2, SParam3, SParam4, SParam5) { return SEnvFunctionResult(); }
			SEnvFunctionResult MemberFunction07(const SEnvFunctionContext&, SParam0, SParam1, SParam2, SParam3, SParam4, SParam5, SParam6) { return SEnvFunctionResult(); }
			SEnvFunctionResult MemberFunction08(const SEnvFunctionContext&, SParam0, SParam1, SParam2, SParam3, SParam4, SParam5, SParam6, SParam7) { return SEnvFunctionResult(); }
			SEnvFunctionResult MemberFunction09(const SEnvFunctionContext&, SParam0, SParam1, SParam2, SParam3, SParam4, SParam5, SParam6, SParam7, SParam8) { return SEnvFunctionResult(); }
			SEnvFunctionResult MemberFunction10(const SEnvFunctionContext&, SParam0, SParam1, SParam2, SParam3, SParam4, SParam5, SParam6, SParam7, SParam8, SParam9) { return SEnvFunctionResult(); }

			SEnvFunctionResult ConstMemberFunction00(const SEnvFunctionContext&) const { return SEnvFunctionResult(); }
			SEnvFunctionResult ConstMemberFunction01(const SEnvFunctionContext&, SParam0) const { return SEnvFunctionResult(); }
			SEnvFunctionResult ConstMemberFunction02(const SEnvFunctionContext&, SParam0, SParam1) const { return SEnvFunctionResult(); }
			SEnvFunctionResult ConstMemberFunction03(const SEnvFunctionContext&, SParam0, SParam1, SParam2) const { return SEnvFunctionResult(); }
			SEnvFunctionResult ConstMemberFunction04(const SEnvFunctionContext&, SParam0, SParam1, SParam2, SParam3) const { return SEnvFunctionResult(); }
			SEnvFunctionResult ConstMemberFunction05(const SEnvFunctionContext&, SParam0, SParam1, SParam2, SParam3, SParam4) const { return SEnvFunctionResult(); }
			SEnvFunctionResult ConstMemberFunction06(const SEnvFunctionContext&, SParam0, SParam1, SParam2, SParam3, SParam4, SParam5) const { return SEnvFunctionResult(); }
			SEnvFunctionResult ConstMemberFunction07(const SEnvFunctionContext&, SParam0, SParam1, SParam2, SParam3, SParam4, SParam5, SParam6) const { return SEnvFunctionResult(); }
			SEnvFunctionResult ConstMemberFunction08(const SEnvFunctionContext&, SParam0, SParam1, SParam2, SParam3, SParam4, SParam5, SParam6, SParam7) const { return SEnvFunctionResult(); }
			SEnvFunctionResult ConstMemberFunction09(const SEnvFunctionContext&, SParam0, SParam1, SParam2, SParam3, SParam4, SParam5, SParam6, SParam7, SParam8) const { return SEnvFunctionResult(); }
			SEnvFunctionResult ConstMemberFunction10(const SEnvFunctionContext&, SParam0, SParam1, SParam2, SParam3, SParam4, SParam5, SParam6, SParam7, SParam8, SParam9) const { return SEnvFunctionResult(); }
		};

		EUnitTestResultFlags TestEnvFunctionDescriptor(const CEnvFunctionDescriptorPtr& pEnvFunctionDescriptor, EEnvFunctionFlags flags, uint32 paramCount)
		{
			if(!pEnvFunctionDescriptor)
			{
				return EUnitTestResultFlags::FatalError;
			}

			if(pEnvFunctionDescriptor->GetFlags() != flags)
			{
				return EUnitTestResultFlags::FatalError;
			}

			if(pEnvFunctionDescriptor->GetParamCount() != paramCount)
			{
				return EUnitTestResultFlags::FatalError;
			}

			SObject                  object;
			const EnvFunctionInputs  inputs = { nullptr };
			const EnvFunctionOutputs outputs = { nullptr };
			pEnvFunctionDescriptor->Execute(SEnvFunctionContext(), &object, inputs, outputs);
			
			// #SchematycTODO : Verify result of execution?

			return EUnitTestResultFlags::Success;
		}

		EUnitTestResultFlags Run()
		{
			EUnitTestResultFlags resultFlags = EUnitTestResultFlags::Success;

			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&GlobalFunction00), EEnvFunctionFlags::None, 0);
			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&SObject::MemberFunction00), EEnvFunctionFlags::Member, 0);
			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&SObject::ConstMemberFunction00), EEnvFunctionFlags::Member | EEnvFunctionFlags::Const, 0);

			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&GlobalFunction01), EEnvFunctionFlags::None, 1);
			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&SObject::MemberFunction01), EEnvFunctionFlags::Member, 1);
			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&SObject::ConstMemberFunction01), EEnvFunctionFlags::Member | EEnvFunctionFlags::Const, 1);

			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&GlobalFunction02), EEnvFunctionFlags::None, 2);
			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&SObject::MemberFunction02), EEnvFunctionFlags::Member, 2);
			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&SObject::ConstMemberFunction02), EEnvFunctionFlags::Member | EEnvFunctionFlags::Const, 2);

			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&GlobalFunction03), EEnvFunctionFlags::None, 3);
			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&SObject::MemberFunction03), EEnvFunctionFlags::Member, 3);
			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&SObject::ConstMemberFunction03), EEnvFunctionFlags::Member | EEnvFunctionFlags::Const, 3);

			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&GlobalFunction04), EEnvFunctionFlags::None, 4);
			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&SObject::MemberFunction04), EEnvFunctionFlags::Member, 4);
			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&SObject::ConstMemberFunction04), EEnvFunctionFlags::Member | EEnvFunctionFlags::Const, 4);

			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&GlobalFunction05), EEnvFunctionFlags::None, 5);
			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&SObject::MemberFunction05), EEnvFunctionFlags::Member, 5);
			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&SObject::ConstMemberFunction05), EEnvFunctionFlags::Member | EEnvFunctionFlags::Const, 5);

			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&GlobalFunction06), EEnvFunctionFlags::None, 6);
			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&SObject::MemberFunction06), EEnvFunctionFlags::Member, 6);
			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&SObject::ConstMemberFunction06), EEnvFunctionFlags::Member | EEnvFunctionFlags::Const, 6);

			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&GlobalFunction07), EEnvFunctionFlags::None, 7);
			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&SObject::MemberFunction07), EEnvFunctionFlags::Member, 7);
			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&SObject::ConstMemberFunction07), EEnvFunctionFlags::Member | EEnvFunctionFlags::Const, 7);

			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&GlobalFunction08), EEnvFunctionFlags::None, 8);
			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&SObject::MemberFunction08), EEnvFunctionFlags::Member, 8);
			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&SObject::ConstMemberFunction08), EEnvFunctionFlags::Member | EEnvFunctionFlags::Const, 8);

			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&GlobalFunction09), EEnvFunctionFlags::None, 9);
			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&SObject::MemberFunction09), EEnvFunctionFlags::Member, 9);
			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&SObject::ConstMemberFunction09), EEnvFunctionFlags::Member | EEnvFunctionFlags::Const, 9);

			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&GlobalFunction10), EEnvFunctionFlags::None, 10);
			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&SObject::MemberFunction10), EEnvFunctionFlags::Member, 10);
			resultFlags |= TestEnvFunctionDescriptor(MakeEnvFunctionDescriptorShared(&SObject::ConstMemberFunction10), EEnvFunctionFlags::Member | EEnvFunctionFlags::Const, 10);

			return resultFlags;
		}
	}

	SCHEMATYC2_REGISTER_UNIT_TEST(&EnvFunctionDescriptorUnitTests::Run, "EnvFunctionDescriptor")
}
