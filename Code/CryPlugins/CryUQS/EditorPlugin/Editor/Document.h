// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>
#include <CrySerialization/StringList.h>
class PropertyTree;

#include "Settings.h"
#include "ItemTypeName.h"

namespace UQS
{
namespace Core
{
class CTextualQueryBlueprint;
}
namespace Shared
{
class CTypeInfo;
}   // namespace Shared
namespace DataSource
{
struct IEditorLibraryProvider;
}
};

namespace UQSEditor
{
class CQueryBlueprint;
} // namespace UQSEditor

struct SItemTypeName;
class CUqsEditorContext;

class CUqsQueryDocument
{
public:
	explicit CUqsQueryDocument(const CUqsEditorContext& editorContext);

	~CUqsQueryDocument();

	bool                        CreateNew(const char* szQueryName, stack_string& outErrorMessage);
	void                        Delete();
	bool                        Load(const char* szQueryName);
	bool                        Save();

	void                        Serialize(Serialization::IArchive& archive);

	void                        AttachToTree(PropertyTree* pTree);
	void                        DetachFromTree();

	void                        ApplySettings(const SDocumentSettings& settings);

	UQSEditor::CQueryBlueprint* GetQueryBlueprintPtr() const { return m_pQueryBlueprint.get(); }
	const string& GetName() const              { return m_queryName; }
	bool          IsNeverSaved() const         { return m_bNeverSaved; }

	void          OnTreeChanged();
private:

	void ReoutputTree();
	void BeforeManualRevert();
	void AfterManualRevertDone();
private:

	friend class CUqsDocSerializationContext;

	PropertyTree* m_pTree;

	std::unique_ptr<UQSEditor::CQueryBlueprint> m_pQueryBlueprint;
	string                   m_queryName;

	SDocumentSettings        m_settings;

	const CUqsEditorContext& m_editorContext;

	bool                     m_bNeverSaved;
};
