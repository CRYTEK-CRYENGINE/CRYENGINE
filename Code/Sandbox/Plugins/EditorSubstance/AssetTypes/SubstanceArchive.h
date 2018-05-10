// Copyright 2001-2017 Crytek GmbH. All rights reserved.

#pragma once

#include <AssetSystem/AssetType.h>
#include <AssetSystem/EditableAsset.h>
#include "SubstanceCommon.h"

class QMenu;

namespace EditorSubstance
{
namespace AssetTypes
{

class CSubstanceArchiveType : public CAssetType
{
public:
	DECLARE_ASSET_TYPE_DESC(CSubstanceArchiveType);

	class SSubstanceArchiveCache
	{
		friend class CSubstanceArchiveType;
	public:
		class Graph
		{
			friend class CSubstanceArchiveType;
		public:
			Graph();
			const std::vector<string>&           GetOutputNames() const          { return m_outputs; }
			const std::vector<SSubstanceOutput>& GetOutputsConfiguration() const { return m_outputsConfiguration; }
			const Vec2i                          GetResolution() const;
			void                                 Serialize(Serialization::IArchive& ar);
		protected:
			string                        m_name;
			std::vector<string>           m_outputs;
			std::vector<SSubstanceOutput> m_outputsConfiguration;
			int                           m_resolutionX;
			int                           m_resolutionY;
		};
		const Graph&                    GetGraph(const string& name) const;
		const bool                      HasGraph(const string& name) const;
		const std::vector<string>       GetGraphNames();
		const std::vector<const Graph*> GetGraphs();
		void                            Serialize(Serialization::IArchive& ar);
	protected:
		std::map<string /*name*/, Graph> m_graphs;

	};

	virtual const char*    GetTypeName() const override       { return "SubstanceDefinition"; }
	virtual const char*    GetUiTypeName() const override     { return QT_TR_NOOP("Substance Archive"); }
	virtual bool           IsImported() const override        { return true; }
	virtual bool           CanBeEdited() const override       { return true; }
	virtual CryIcon        GetIcon() const override;
	virtual bool           HasThumbnail() const override      { return false; }
	virtual const char*    GetFileExtension() const override  { return "sbsar"; }
	virtual QColor         GetThumbnailColor() const override { return QColor(79, 187, 185); }

	virtual void           AppendContextMenuActions(const std::vector<CAsset*>& assets, CAbstractMenu* menu) const override;
	virtual CAssetEditor*  Edit(CAsset* asset) const override;

	void                   SetArchiveCache(CEditableAsset* editableAsset, const std::map<string, std::vector<string>>& archiveData) const;
	SSubstanceArchiveCache GetArchiveCache(CAsset* asset) const;
	void                   SetGraphOutputsConfiguration(CEditableAsset* editableAsset, const string& name, const std::vector<SSubstanceOutput>& outputs, const Vec2i& resolution) const;
private:
	void                   SaveGraphCache(CEditableAsset* editableAsset, const CSubstanceArchiveType::SSubstanceArchiveCache& cache) const;
};
}
}
