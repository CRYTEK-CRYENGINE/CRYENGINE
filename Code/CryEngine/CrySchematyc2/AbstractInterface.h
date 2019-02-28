// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/IAbstractInterface.h>

namespace Schematyc2
{
	// Abstract interface function.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	class CAbstractInterfaceFunction : public IAbstractInterfaceFunction
	{
	public:

		CAbstractInterfaceFunction(const SGUID& guid, const SGUID& ownerGUID);

		// IAbstractInterfaceFunction
		virtual SGUID GetGUID() const override;
		virtual SGUID GetOwnerGUID() const override;
		virtual void SetName(const char* szName) override;
		virtual const char* GetName() const override;
		virtual void SetFileName(const char* szFileName, const char* szProjectDir) override;
		virtual const char* GetFileName() const override;
		virtual void SetAuthor(const char* szAuthor) override;
		virtual const char* GetAuthor() const override;
		virtual void SetDescription(const char* szDescription) override;
		virtual const char* GetDescription() const override;
		virtual void AddInput(const char* name, const char* szDescription, const IAnyPtr& pValue) override;
		virtual size_t GetInputCount() const override;
		virtual const char* GetInputName(size_t iInput) const override;
		virtual const char* GetInputDescription(size_t iInput) const override;
		virtual IAnyConstPtr GetInputValue(size_t iInput) const override;
		virtual TVariantConstArray GetVariantInputs() const override;
		virtual void AddOutput(const char* name, const char* szDescription, const IAnyPtr& pValue) override;
		virtual size_t GetOutputCount() const override;
		virtual const char* GetOutputName(size_t iOutput) const override;
		virtual const char* GetOutputDescription(size_t iOutput) const override;
		virtual IAnyConstPtr GetOutputValue(size_t iOutput) const override;
		virtual TVariantConstArray GetVariantOutputs() const override;
		// ~IAbstractInterfaceFunction

	private:

		struct SParam
		{
			SParam(const char* _name, const char* _szDescription, const IAnyPtr& _pValue);

			string	name;
			string	description;
			IAnyPtr	pValue;
		};

		typedef std::vector<SParam> TParamVector;

		SGUID						m_guid;
		SGUID						m_ownerGUID;
		string					m_name;
		string					m_fileName;
		string					m_author;
		string					m_description;
		TParamVector		m_inputs;
		TVariantVector	m_variantInputs;
		TParamVector		m_outputs;
		TVariantVector	m_variantOutputs;
	};

	DECLARE_SHARED_POINTERS(CAbstractInterfaceFunction)

	// Abstract interface.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	class CAbstractInterface : public IAbstractInterface
	{
	public:

		CAbstractInterface(const SGUID& guid);

		// IAbstractInterface
		virtual SGUID GetGUID() const override;
		virtual void SetName(const char* name) override;
		virtual const char* GetName() const override;
		virtual void SetNamespace(const char* szNamespace) override;
		virtual const char* GetNamespace() const override;
		virtual void SetFileName(const char* fileName, const char* projectDir) override;
		virtual const char* GetFileName() const override;
		virtual void SetAuthor(const char* author) override;
		virtual const char* GetAuthor() const override;
		virtual void SetDescription(const char* szDescription) override;
		virtual const char* GetDescription() const override;
		virtual IAbstractInterfaceFunctionPtr AddFunction(const SGUID& guid) override;
		virtual size_t GetFunctionCount() const override;
		virtual IAbstractInterfaceFunctionConstPtr GetFunction(size_t iFunction) const override;
		// ~IAbstractInterface

	private:

		typedef std::vector<CAbstractInterfaceFunctionPtr> TFunctionVector;

		SGUID						m_guid;
		string					m_name;
		string					m_namespace;
		string					m_fileName;
		string					m_author;
		string					m_description;
		TFunctionVector	m_functions;
	};
}
