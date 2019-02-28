// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CrySerialization/Math.h>

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace StdLib
	{

		template <typename T>
		T GetValueInitializedItem()
		{
			return T();
		}

		// for Client::STypeWrapper<> types whose underlying type is Vec3
		template <class TVec3BasedTypeWrapper>
		TVec3BasedTypeWrapper GetVec3BasedTypeWrapperZero()
		{
			return TVec3BasedTypeWrapper(Vec3(ZERO));
		}

#if UQS_SCHEMATYC_SUPPORT
		void ConvertEntityIdToEntityIdWrapper(const EntityId& input, EntityIdWrapper& output)
		{
			output.value = input;
		}

		void ConvertEntityIdWrapperToEntityId(const EntityIdWrapper& input, EntityId& output)
		{
			output = input.value;
		}

		template <class TVec3BasedTypeWrapper>
		void ConvertVec3ToVec3BasedWrapper(const Vec3& input, TVec3BasedTypeWrapper& output)
		{
			output.value = input;
		}

		template <class TVec3BasedTypeWrapper>
		void ConvertVec3BasedWrapperToVec3(const TVec3BasedTypeWrapper& input, Vec3& output)
		{
			output = input.value;
		}
#endif

		void CStdLibRegistration::InstantiateItemFactoriesForRegistration()
		{
			// std::EntityId
			{
				Client::CItemFactory<EntityIdWrapper>::SCtorParams ctorParams;

				ctorParams.szName = "EntityId";
				ctorParams.guid = "1ad38a1e-1fd6-469e-a03a-4d600b334e62"_cry_guid;
				ctorParams.callbacks.pAddItemToDebugRenderWorld = &EntityId_AddToDebugRenderWorld;
				ctorParams.callbacks.pCreateItemDebugProxy = &EntityId_CreateItemDebugProxyForItem;
#if UQS_SCHEMATYC_SUPPORT
				ctorParams.callbacks.itemConverters.AddFromForeignTypeConverter<EntityId, &ConvertEntityIdToEntityIdWrapper>("EntityId", "std::EntityIdWrapper", "345ae48a-ad37-4458-94b9-64806c2df087"_cry_guid);
				ctorParams.callbacks.itemConverters.AddToForeignTypeConverter<EntityId, &ConvertEntityIdWrapperToEntityId>("std::EntityIdWrapper", "EntityId", "82bd2238-dd8e-44ae-af30-a58e36f3b9a3"_cry_guid);
#endif

				static const Client::CItemFactory<EntityIdWrapper> itemFactory_entityId(ctorParams);
			}

			// std::int
			{
				Client::CItemFactory<int>::SCtorParams ctorParams;

				ctorParams.szName = "std::int";
				ctorParams.guid = "da2d5ee0-ad34-4cfd-88bb-a4111701a648"_cry_guid;
				ctorParams.callbacks.pSerialize = &Client::SerializeItem<int>;
				ctorParams.callbacks.pCreateDefaultObject = &GetValueInitializedItem<int>;

				static const Client::CItemFactory<int> itemFactory_int(ctorParams);
			}

			// std::bool
			{
				Client::CItemFactory<bool>::SCtorParams ctorParams;

				ctorParams.szName = "std::bool";
				ctorParams.guid = "9605e7fe-d519-49e9-8ded-3e60170df1df"_cry_guid;
				ctorParams.callbacks.pSerialize = &Client::SerializeItem<bool>;
				ctorParams.callbacks.pCreateDefaultObject = &GetValueInitializedItem<bool>;

				static const Client::CItemFactory<bool> itemFactory_bool(ctorParams);
			}

			// std::float
			{
				Client::CItemFactory<float>::SCtorParams ctorParams;

				ctorParams.szName = "std::float";
				ctorParams.guid = "73bcca90-c7c2-47be-94ac-6f306e82e257"_cry_guid;
				ctorParams.callbacks.pSerialize = &Client::SerializeItem<float>;
				ctorParams.callbacks.pCreateDefaultObject = &GetValueInitializedItem<float>;

				static const Client::CItemFactory<float> itemFactory_float(ctorParams);
			}

			// std::Pos3
			{
				Client::CItemFactory<Pos3>::SCtorParams ctorParams;

				ctorParams.szName = "std::Pos3";
				ctorParams.guid = "1b363d0a-dc71-45d4-9a8d-0fdb7d9e228c"_cry_guid;
				ctorParams.szDescription = "Vec3-based type intended for global positions in the 3D world";
				ctorParams.callbacks.pSerialize = &Client::SerializeTypeWrappedItem<Pos3>;
				ctorParams.callbacks.pCreateDefaultObject = &GetVec3BasedTypeWrapperZero<Pos3>;
				ctorParams.callbacks.pAddItemToDebugRenderWorld = &Pos3_AddToDebugRenderWorld;
				ctorParams.callbacks.pCreateItemDebugProxy = &Pos3_CreateItemDebugProxyForItem;
#if UQS_SCHEMATYC_SUPPORT
				ctorParams.callbacks.itemConverters.AddFromForeignTypeConverter<Vec3, &ConvertVec3ToVec3BasedWrapper<Pos3>>("Vec3", "std::Pos3", "30aca024-1fff-4fb5-8956-cf54085c46dd"_cry_guid);
				ctorParams.callbacks.itemConverters.AddToForeignTypeConverter<Vec3, &ConvertVec3BasedWrapperToVec3<Pos3>>("std::Pos3", "Vec3", "9c90027d-0a4e-4420-9069-8797e69d37bc"_cry_guid);
#endif
				static const Client::CItemFactory<Pos3> itemFactory_Pos3(ctorParams);
			}

			// std::Ofs3
			{
				Client::CItemFactory<Ofs3>::SCtorParams ctorParams;

				ctorParams.szName = "std::Ofs3";
				ctorParams.guid = "41e2adc2-8318-43c4-b6f0-9bd549586dcf"_cry_guid;
				ctorParams.szDescription = "Vec3-based type intended for use as offsets (i. e. local positions)";
				ctorParams.callbacks.pSerialize = &Client::SerializeTypeWrappedItem<Ofs3>;
				ctorParams.callbacks.pCreateDefaultObject = &GetVec3BasedTypeWrapperZero<Ofs3>;
#if UQS_SCHEMATYC_SUPPORT
				ctorParams.callbacks.itemConverters.AddFromForeignTypeConverter<Vec3, &ConvertVec3ToVec3BasedWrapper<Ofs3>>("Vec3", "std::Ofs3", "f36a4dde-2995-4f96-bda4-f8546b0074a9"_cry_guid);
				ctorParams.callbacks.itemConverters.AddToForeignTypeConverter<Vec3, &ConvertVec3BasedWrapperToVec3<Ofs3>>("std::Ofs3", "Vec3", "5ff51b20-affa-4bf1-9bed-ec478a3c4f8c"_cry_guid);
#endif
				static const Client::CItemFactory<Ofs3> itemFactory_Ofs3(ctorParams);
			}

			// std::Dir3
			{
				Client::CItemFactory<Dir3>::SCtorParams ctorParams;

				ctorParams.szName = "std::Dir3";
				ctorParams.guid = "eca80817-8631-48ba-9a1b-37d187ec92a2"_cry_guid;
				ctorParams.szDescription = "Vec3-based type intended for use as normalized 3D vector";
				ctorParams.callbacks.pSerialize = &Client::SerializeTypeWrappedItem<Dir3>;
				ctorParams.callbacks.pCreateDefaultObject = &GetVec3BasedTypeWrapperZero<Dir3>;
#if UQS_SCHEMATYC_SUPPORT
				ctorParams.callbacks.itemConverters.AddFromForeignTypeConverter<Vec3, &ConvertVec3ToVec3BasedWrapper<Dir3>>("Vec3", "std::Dir3", "95e16187-ac55-4dd7-80dd-0ffa42edbcdc"_cry_guid);
				ctorParams.callbacks.itemConverters.AddToForeignTypeConverter<Vec3, &ConvertVec3BasedWrapperToVec3<Dir3>>("std::Dir3", "Vec3", "f525d01e-272e-4f38-a480-ab119d12b766"_cry_guid);
#endif
				static const Client::CItemFactory<Dir3> itemFactory_Dir3(ctorParams);
			}

			// std::NavigationAgentTypeID
			{
				Client::CItemFactory<NavigationAgentTypeID>::SCtorParams ctorParams;

				ctorParams.szName = "std::NavigationAgentTypeID";
				ctorParams.guid = "4338dd81-718a-4365-a21a-65d5b5d08bd3"_cry_guid;
				ctorParams.szDescription = "Type of the agent for use in the Navigation Mesh";
				ctorParams.callbacks.pSerialize = &Serialize;

				static const Client::CItemFactory<NavigationAgentTypeID> itemFactory_NavigationAgentTypeID(ctorParams);
			}
		}

	}
}
