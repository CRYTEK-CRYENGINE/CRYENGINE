// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/ISignal.h>

namespace Schematyc2
{
	class CSignal : public ISignal
	{
	public:

		CSignal(const SGUID& guid, const SGUID& senderGUID = SGUID(), const char* szName = nullptr);

		// ISignal
		virtual SGUID GetGUID() const override;
		virtual void SetSenderGUID(const SGUID& senderGUID) override;
		virtual SGUID GetSenderGUID() const override;
		virtual void SetName(const char* szName) override;
		virtual const char* GetName() const override;
		virtual void SetNamespace(const char* szNamespace) override;
		virtual const char* GetNamespace() const override;
		virtual void SetFileName(const char* szFileName, const char* szProjectDir) override;
		virtual const char* GetFileName() const override;
		virtual void SetAuthor(const char* szAuthor) override;
		virtual const char* GetAuthor() const override;
		virtual void SetDescription(const char* szDescription) override;
		virtual const char* GetDescription() const override;
		virtual void SetWikiLink(const char* szWikiLink) override;
		virtual const char* GetWikiLink() const override;
		virtual size_t GetInputCount() const override;
		virtual const char* GetInputName(size_t inputIdx) const override;
		virtual const char* GetInputDescription(size_t inputIdx) const override;
		virtual TVariantConstArray GetVariantInputs() const override;
		virtual IAnyConstPtr GetInputValue(size_t inputIdx) const override;
		// ~ISignal

	protected:

		// ~ISignal
		virtual size_t AddInput_Protected(const char* szName, const char* szDescription, const IAny& value) override;
		// ~ISignal

	private:

		struct SInput
		{
			SInput(const char* _szName, const char* _szDescription, const IAnyPtr& _pValue);

			string  name;
			string  description;
			IAnyPtr pValue;
		};

		typedef std::vector<SInput> Inputs;

		SGUID          m_guid;
		SGUID          m_senderGUID;
		string         m_name;
		string         m_namespace;
		string         m_fileName;
		string         m_author;
		string         m_description;
		string         m_wikiLink;
		Inputs         m_inputs;
		TVariantVector m_variantInputs;
	};

	DECLARE_SHARED_POINTERS(CSignal)
}
