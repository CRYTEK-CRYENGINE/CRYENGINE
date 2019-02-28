// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QWidget>

#include <CrySandbox/CrySignal.h>

#include "EditorCommonAPI.h"
#include "ProxyModels/ItemModelAttribute.h"
#include "ProxyModels/AttributeFilterProxyModel.h"
#include "EditorFramework/StateSerializable.h"

class CAbstractMenu;
class QVBoxLayout;
class QHBoxLayout;
class CItemModelAttribute;
class QLineEdit;
class QToolButton;
class QSplitter;
class QGridLayout;
class QSearchBox;

class EDITOR_COMMON_API QFilteringPanel : public QWidget, public IStateSerializable
{
public:
	//! module name needs to be unique across the editor to save properly in personalization
	QFilteringPanel(const char* uniqueName, QAttributeFilterProxyModel* pModel, QWidget* pParent = nullptr);
	virtual ~QFilteringPanel();

	//! Only propagates changes to the model after the timer is expired when enabled, true by default
	void        EnableDelayedSearch(bool bEnable, float timerMsec = 100);

	void        SetModel(QAttributeFilterProxyModel* pModel);
	void        SetContent(QWidget* widget);

	QSearchBox* GetSearchBox() { return m_pSearchBox; }

	//! Used to insert more options into the filter panel.
	//!The grid layout comes by default filled with the "AddFilter" button in cell(0,0)
	QGridLayout*        GetOptionsLayout() { return m_optionsLayout; }

	void                Clear();

	virtual void        SetState(const QVariantMap& state) override;
	virtual QVariantMap GetState() const override;

	bool                HasActiveFilters() const;

	//! Fills the provided menu with the names of the saved filters, 
	//! when user clicks on an element, the selected filter is applied.
	//! \param pMenu An instance of menu to be filled in.
	//! \param submenuName If not an empty string, the function will put all elements in a new submenu.
	void FillMenu(CAbstractMenu* pMenu, QString& submenuName = QString());

	//! Called when the models are updated, after the filters are changed
	CCrySignal<void()> signalOnFiltered;

private:

	class CFilterWidget;
	class CSavedFiltersWidget;
	class CSavedFiltersModel;

	CFilterWidget*                           AddFilter();

	const std::vector<CItemModelAttribute*>& GetAttributes() const { return m_attributes; }

	void                                     OnFilterChanged();
	void                                     OnSearch();

	void                                     OnModelUpdated();

	bool                                     IsExpanded() const;
	void                                     SetExpanded(bool expanded);

	void                                     OnAddFilter(CFilterWidget* filter);
	void                                     OnRemoveFilter(CFilterWidget* filter);

	void                                     UpdateOptionsIcon();

	void                                     ShowFavoriteFilter(bool show, bool isFiltering);

	//Filter loading and saving.
	void             ShowSavedFilters();
	void             SaveFilter(const char* filterName);
	void             LoadFilter(const QString& filter);
	void             DeleteFilter(const char* filterName);
	void             RenameFilter(const char* filterName, const char* filterNewName);
	QVector<QString> GetSavedFilters() const;

	QVariant         GetFilterState() const;
	void             SetFilterState(const QVariant& state);

	std::vector<CItemModelAttribute*> m_attributes;
	std::vector<CFilterWidget*>       m_filters;
	QSearchBox*                       m_pSearchBox;
	QWidget*                          m_pFiltersWidget;
	QVBoxLayout*                      m_pFiltersLayout;
	QAttributeFilterProxyModel*       m_pModel;
	QToolButton*                      m_optionsButton;
	QToolButton*                      m_addFilterButton;
	QToolButton*                      m_saveLoadButton;
	QSplitter*                        m_splitter;
	QGridLayout*                      m_optionsLayout;
	float                             m_timerMsec;
	QTimer*                           m_timer;
	QToolButton*                      m_favoritesButton;
	AttributeFilterSharedPtr          m_favoritesFilter;
	const char*                       m_uniqueName;
	CSavedFiltersWidget*              m_savedFiltersWidget;
};

