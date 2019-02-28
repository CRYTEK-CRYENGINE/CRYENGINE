// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Serialize.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		bool Serialize(Serialization::IArchive& ar, CTimeValue& timeValue, const char* szName, const char* szLabel)
		{
			int64 value = 0;

			if (ar.isOutput())
			{
				// serialize
				value = timeValue.GetValue();
				return ar(value, szName);
			}
			else if (ar.isInput())
			{
				// de-serialize
				const bool result = ar(value, szName);
				timeValue.SetValue(value);
				return result;
			}

			return false;
		}

		bool Serialize(Serialization::IArchive& ar, OBB& obb, const char* szName, const char* szLabel)
		{
			stack_string tmpName;
			stack_string tmpLabel;

			if (!szName)
				szName = "";

			if (!szLabel)
				szLabel = "";

			tmpName.Format("%s.m33", szName);
			tmpLabel.Format("%s.m33", szLabel);
			if (!ar(obb.m33, tmpName.c_str(), tmpLabel.c_str()))
				return false;

			tmpName.Format("%s.h", szName);
			tmpLabel.Format("%s.h", szLabel);
			if (!ar(obb.h, tmpName.c_str(), tmpLabel.c_str()))
				return false;

			tmpName.Format("%s.c", szName);
			tmpLabel.Format("%s.c", szLabel);
			if (!ar(obb.c, tmpName.c_str(), tmpLabel.c_str()))
				return false;

			return true;
		}

		template<typename T>
		struct SSharedPtrSerializer
		{
			std::shared_ptr<T>& ptr;

			SSharedPtrSerializer(std::shared_ptr<T>& ptr_) : ptr(ptr_) {}

			void Serialize(Serialization::IArchive& ar)
			{
				bool bPtrExists = (ptr != nullptr);
				if (ar.isInput())
				{
					if (ar(bPtrExists, "exists"))
					{
						if (bPtrExists)
						{
							ptr.reset(new T);
							ar(*ptr, "ptr");
						}
						else
						{
							ptr.reset();
						}
					}
				}
				else
				{
					ar(bPtrExists, "exists");
					if (bPtrExists)
					{
						ar(*ptr, "ptr");
					}
				}
			}
		};

		bool Serialize(Serialization::IArchive& ar, std::shared_ptr<CHistoricQuery>& pHistoricQuery, const char* szName, const char* szLabel)
		{
			SSharedPtrSerializer<CHistoricQuery> ser(pHistoricQuery);
			return ar(ser, szName, szLabel);
		}

	}
}
