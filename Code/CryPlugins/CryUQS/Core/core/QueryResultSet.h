// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CQueryResultSet
		//
		//===================================================================================

		class CQueryResultSet final : public IQueryResultSet
		{
		public:
			explicit                          CQueryResultSet(const IItemList& generatedItems, std::vector<size_t>&& itemIndexes, std::vector<float>&& itemScores);

			// IQueryResultSet
			virtual Client::IItemFactory&     GetItemFactory() const override;
			virtual size_t                    GetResultCount() const override;
			virtual SResultSetEntry           GetResult(size_t index) const override;
			virtual const CQueryResultSet&    GetImplementation() const override;
			// ~IQueryResultSet

			const CItemList&                  GetItemList() const;

		private:
			                                  UQS_NON_COPYABLE(CQueryResultSet);

			// IQueryResultSet
			virtual void                      DeleteSelf() override;
			// ~IQueryResultSet

		private:
			CItemList                         m_items;
			std::vector<float>                m_scores;
		};

	}
}
