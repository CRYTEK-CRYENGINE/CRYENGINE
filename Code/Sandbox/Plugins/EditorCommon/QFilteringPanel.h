// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"
#include "ProxyModels/ItemModelAttribute.h"
#include "ProxyModels/AttributeFilterProxyModel.h"
#include "EditorFramework/StateSerializable.h"

#include <CrySandbox/CrySignal.h>

#include <QWidget>

class CAbstractMenu;
class CItemModelAttribute;
class QGridLayout;
class QSearchBox;
class QSplitter;
class QToolButton;
class QVBoxLayout;

class EDITOR_COMMON_API QFilteringPanel : public QWidget, public IStateSerializable
{
	Q_OBJECT

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

	//! Creates and fills the filter menu and attaches to the provided parent menu,
	//! \param pParentMenu The menu where the filter menu will be placed.
	void CreateMenu(CAbstractMenu* pParentMenu);

	//! Adds a new filter with the provided parameters.
	//! \sa CItemModelAttribute
	void AddFilter(const QString& attributeName, const QString& operatorName, const QString& filterValue);

	//! Overrides values provided by CItemModelAttributeEnum attributes for this instance of the filtering panel.
	//! Allows you to define entries even for attributes of a non-enum type. This feature can be used with a custom implementation of the IAttributeFilterOperator.
	//! \sa IAttributeFilterOperator::CreateEditWidget
	//! \sa CItemModelAttributeEnum::GetEnumEntries
	void OverrideAttributeEnumEntries(const QString& attributeName, const QStringList& values);

	bool IsExpanded() const;
	void SetExpanded(bool expanded);

	//! Called when the models are updated, after the filters are changed
	CCrySignal<void()> signalOnFiltered;
protected:

	void paintEvent(QPaintEvent* pEvent) override;

private:

	class CFilterWidget;
	class CSavedFiltersWidget;
	class CSavedFiltersModel;

	// Fills the provided menu with all the existing filters
	void                                     FillMenu(CAbstractMenu* pMenu);

	CFilterWidget*                           AddFilter();

	const std::vector<CItemModelAttribute*>& GetAttributes() const { return m_attributes; }
	const QStringList*                       GetAttributeEnumEntries(const QString& attributeName);

	void                                     OnFilterChanged();
	void                                     OnSearch();

	void                                     OnModelUpdated();

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

	bool             IsFilterSet(const QString& filterName) const;
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
	std::map<QString, QStringList>    m_attributeValues;
};
