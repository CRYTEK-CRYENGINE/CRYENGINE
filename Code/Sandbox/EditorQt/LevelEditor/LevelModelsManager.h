// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

//Not forward declared since they will most likely need to be included by files using LevelModelsManager

#include "LevelModel.h"
#include "LevelLayerModel.h"

class CMergingProxyModel;
class CMountingProxyModel;
struct CLayerChangeEvent;

//! This is a repository for all models related to the loaded level and its contents.
//! For most applications, do not instantiate the models directly, as they are meant to be shared objects, so request them from this manager.
//! This also manages the lifetime and update and models for you, inherits from QObject for this reason
class CLevelModelsManager : public QObject, public IEditorNotifyListener
{
	Q_OBJECT
public:
	static CLevelModelsManager& GetInstance();

	//! Returns the model describing the layer hierarchy of the model
	CLevelModel* GetLevelModel() const;

	//! Returns a model for a layer, if it exists. Do not forget to attach to destroyed() signal of this model in case the layer gets destroyed
	CLevelLayerModel* GetLayerModel(CObjectLayer* layer) const;

	//! Returns a model describing the full tree of the level, with layers and their contents
	QAbstractItemModel* GetFullHierarchyModel() const;

	//! Returns a model listing all layer contents as a flat list
	QAbstractItemModel* GetAllObjectsListModel() const;

	//! Emitted when layer models have been created or destroyed
	CCrySignal<void()> signalLayerModelsUpdated;

private:
	CLevelModelsManager();
	~CLevelModelsManager();

	void                DeleteLayerModels();
	void                CreateLayerModels();

	void                OnEditorNotifyEvent(EEditorNotifyEvent event);
	void                OnLayerChange(const CLayerChangeEvent& event);

	QAbstractItemModel* CreateLayerModelFromIndex(const QModelIndex& sourceIndex);

	void OnBatchProcessStarted(const std::vector<CBaseObject*>& objects);
	void OnBatchProcessFinished();

	void OnLayerUpdate(const CLayerChangeEvent& event);

	void DisconnectChildrenModels(CObjectLayer* pLayer);

	std::unordered_map<CObjectLayer*, CLevelLayerModel*> m_layerModels;
	std::vector<CLevelLayerModel*> m_layerModelsInvolvedInBatching;
	CLevelModel* m_levelModel{ nullptr };
	CMergingProxyModel* m_allObjectsModel{ nullptr };
	CMountingProxyModel* m_fullLevelModel{ nullptr };
	int m_batchProcessStackIndex{ 0 };
	bool m_ignoreLayerNotify{ false };
};

