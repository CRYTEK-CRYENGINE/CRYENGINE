// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/Env/EnvTypeId.h"
#include "CrySchematyc2/GUID.h"

namespace Schematyc2
{
	struct IAny;
	struct IScriptFile;

	struct SValidatorLink
	{
		inline SValidatorLink(const SGUID& _itemGUID = SGUID(), const SGUID& _detailGUID = SGUID())
			: itemGUID(_itemGUID)
			, detailGUID(_detailGUID)
		{}

		SGUID itemGUID;
		SGUID detailGUID;
	};

	enum class ESerializationPass
	{
		PreLoad,  // Load simple header information (e.g. reference guids) required to work out what dependencies we have.
		Load,     // Load implementation. At this point all dependencies exist will be initialized and available.
		PostLoad, // Load additional data (e.g. graph nodes). At this point we can guarantee that all elements will be initialized and available.
		Save,     // Save data.
		Edit,     // Edit data.
		Validate, // Validate and collect warnings/errors.
		Undefined // Undefined
	};

	struct SSerializationContextParams
	{
		inline SSerializationContextParams(Serialization::IArchive& _archive, IScriptFile* _pScriptFile, ESerializationPass _pass)
			: archive(_archive)
			, pScriptFile(_pScriptFile)
			, pass(_pass)
		{}

		inline SSerializationContextParams(Serialization::IArchive& _archive, ESerializationPass _pass)
			: archive(_archive)
			, pScriptFile(nullptr)
			, pass(_pass)
		{}

		Serialization::IArchive& archive;
		IScriptFile*             pScriptFile;
		ESerializationPass       pass;
	};

	struct ISerializationContext
	{
		virtual ~ISerializationContext() {}

		virtual IScriptFile* GetScriptFile() const = 0;
		virtual ESerializationPass GetPass() const = 0;
		virtual void SetValidatorLink(const SValidatorLink& validatorLink) = 0;
		virtual const SValidatorLink& GetValidatorLink() const = 0;
		virtual void AddDependency(const void* pDependency, const EnvTypeId& typeId) = 0;
		 // #SchematycTODO : Do we also need to remove dependencies? Perhaps we could just bind all script element/item data in a single struct which is automatically unbound again after serialization?

		template <typename TYPE> inline const TYPE* GetDependency() const
		{
			return static_cast<const TYPE*>(GetDependency_Protected(GetEnvTypeId<TYPE>()));
		}

	protected:

		virtual const void* GetDependency_Protected(const EnvTypeId& typeId) const = 0;
	};

	DECLARE_SHARED_POINTERS(ISerializationContext)

	namespace SerializationContext
	{
		// #SchematycTODO : Reduce lookups by passing ISerializationContext pointer to utility functions?

		inline ISerializationContext* Get(Serialization::IArchive& archive)
		{
			return archive.context<ISerializationContext>();
		}

		inline IScriptFile* GetScriptFile(Serialization::IArchive& archive)
		{
			ISerializationContext* pSerializationContext = archive.context<ISerializationContext>();
			return pSerializationContext ? pSerializationContext->GetScriptFile() : nullptr;
		}

		inline ESerializationPass GetPass(Serialization::IArchive& archive)
		{
			ISerializationContext* pSerializationContext = archive.context<ISerializationContext>();
			////////////////////////////////////////////////////////////////////////////////////////////////////
			// #SchematycTODO : Ensure we always have a serialization context!!!
			if(!pSerializationContext)
			{
				if(archive.isValidation())
				{
					return ESerializationPass::Validate;
				}
				if(archive.isEdit())
				{
					return ESerializationPass::Edit;
				}
				if(archive.isOutput())
				{
					return ESerializationPass::Save;
				}
			}
			////////////////////////////////////////////////////////////////////////////////////////////////////
			return pSerializationContext ? pSerializationContext->GetPass() : ESerializationPass::Undefined;
		}

		inline void SetValidatorLink(Serialization::IArchive& archive, const SValidatorLink& validatorLink)
		{
			ISerializationContext* pSerializationContext = archive.context<ISerializationContext>();
			if(pSerializationContext)
			{
				pSerializationContext->SetValidatorLink(validatorLink);
			}
		}

		inline bool GetValidatorLink(Serialization::IArchive& archive, SValidatorLink& validatorLink)
		{
			ISerializationContext* pSerializationContext = archive.context<ISerializationContext>();
			if(pSerializationContext)
			{
				validatorLink = pSerializationContext->GetValidatorLink();
				return true;
			}
			return false;
		}
	}
}
