// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>
#include <QAbstractItemModel>

class CUiCommand;
class CCommandModuleDescription;

Q_DECLARE_METATYPE(CCommand*);

class CommandModelFactory
{
public:
	template<typename T>
	static T* Create()
	{
		T* pModel = new T();
		pModel->Initialize();
		return pModel;
	}
};

class CommandModel : public QAbstractItemModel
{
	friend CommandModelFactory;
protected:
	struct CommandModule
	{
		QString                          m_name;
		std::vector<CCommand*>           m_commands;
		const CCommandModuleDescription* m_desc;
	};

public:

	enum class Roles : int
	{
		SearchRole = Qt::UserRole, //QString
		CommandDescriptionRole,
		CommandPointerRole, //CCommand*
		Max,
	};

	virtual ~CommandModel();

	virtual void Initialize();

	//QAbstractItemModel implementation begin
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

protected:
	CommandModel();

	virtual void    Rebuild();

	QModelIndex     GetIndex(CCommand* command, uint column = 0) const;
	CCommand*       GetCommand(const QModelIndex& index) const;
	QCommandAction* GetAction(uint moduleIndex, uint commandIndex) const;
	CommandModule&  FindOrCreateModule(const QString& moduleName);

protected:
	static const int           s_ColumnCount = 3;
	static const char*         s_ColumnNames[3];

	std::vector<CommandModule> m_modules;
};

