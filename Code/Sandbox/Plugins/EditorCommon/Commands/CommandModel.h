// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"

#include <QWidget>
#include <QAbstractItemModel>

class CCommand;
class CCommandModuleDescription;
class QCommandAction;

Q_DECLARE_METATYPE(CCommand*);

class EDITOR_COMMON_API CCommandModel : public QAbstractItemModel
{
protected:
	struct CommandModule
	{
		QString                          m_name;
		std::vector<CCommand*>           m_commands;
		const CCommandModuleDescription* m_desc;
	};

public:
	CCommandModel();
	CCommandModel(const std::vector<CCommand*>& commands);

	enum class Roles : int
	{
		SearchRole = Qt::UserRole, //QString
		CommandDescriptionRole,
		CommandPointerRole, //CCommand*
		Max,
	};

	virtual ~CCommandModel();

	void EnableDragAndDropSupport(bool enableSupport) { m_supportsDragAndDrop = enableSupport; }

	//QAbstractItemModel implementation begin
	virtual Qt::DropActions supportedDropActions() const override;
	virtual QMimeData* mimeData(const QModelIndexList& indexes) const override;

	virtual int           columnCount(const QModelIndex& parent /* = QModelIndex() */) const override { return s_ColumnCount; }
	virtual QVariant      data(const QModelIndex& index, int role /* = Qt::DisplayRole */) const override;
	virtual bool          hasChildren(const QModelIndex& parent /* = QModelIndex() */) const override;
	virtual QVariant      headerData(int section, Qt::Orientation orientation, int role /* = Qt::DisplayRole */) const override;
	virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
	virtual QModelIndex   index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	virtual QModelIndex   parent(const QModelIndex& index) const override;
	virtual bool          setData(const QModelIndex& index, const QVariant& value, int role /*= Qt::EditRole*/) override;
	virtual int           rowCount(const QModelIndex& parent /* = QModelIndex() */) const override;
	//QAbstractItemModel implementation end

	static const char* GetCommandMimeType() { return s_commandMimeType; }

protected:
	void    Initialize();
	void    Initialize(const std::vector<CCommand*>& commands);

	void    Rebuild();

	QModelIndex     GetIndex(CCommand* command, uint column = 0) const;
	CCommand*       GetCommand(const QModelIndex& index) const;
	QCommandAction* GetAction(uint moduleIndex, uint commandIndex) const;
	CommandModule&  FindOrCreateModule(const QString& moduleName);

protected:
	static const int           s_ColumnCount = 3;
	static const char*         s_ColumnNames[3];
	static const char*         s_commandMimeType;
	bool                       m_supportsDragAndDrop;

	std::vector<CommandModule> m_modules;
};
