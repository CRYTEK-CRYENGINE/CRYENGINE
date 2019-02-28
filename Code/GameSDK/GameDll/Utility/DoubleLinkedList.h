// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/******************************************************************************
** DoubleLinkedList.h
** 23/04/10
******************************************************************************/

#ifndef __DOUBLELINKEDLIST_H__
#define __DOUBLELINKEDLIST_H__

class CDoubleLinkedList;

class CDoubleLinkedElement
{
	friend class CDoubleLinkedList;

	private:
										CDoubleLinkedElement(const CDoubleLinkedElement &inCopyMe)
										{
											// invalid operation - construct normally
											m_prev=m_next=this;
										}

										// cppcheck-suppress operatorEqVarError
										CDoubleLinkedElement &operator=(const CDoubleLinkedElement &inCopyMe)
										{
											// invalid operation - do nothing
											return *this;
										}

	protected:
		CDoubleLinkedElement			*m_prev,*m_next;

	public:
										CDoubleLinkedElement()
										{
											m_prev=m_next=this;
										}

										~CDoubleLinkedElement()
										{
											Unlink();
										}

		CDoubleLinkedElement			*Next() const		{ return m_next; }
		CDoubleLinkedElement			*Prev()	const		{ return m_prev; }

										// take the element out of a linked list, element still allocated
		void							Unlink()
										{
											m_prev->m_next=m_next;
											m_next->m_prev=m_prev;

											m_prev=m_next=this;
										}

		void							InsertAfter(
											CDoubleLinkedElement	*inLinkMe)
										{
											CRY_ASSERT_MESSAGE(!inLinkMe->IsInList(),"Adding an element to a linked list that is already in a linked list - this will corrupt the lists - Unlink() before adding");

											inLinkMe->m_next=m_next;
											m_next->m_prev=inLinkMe;

											inLinkMe->m_prev=this;
											m_next=inLinkMe;
										}

		bool							IsInList() const
										{
											bool		linkPrev=(m_prev==this);
											bool		linkNext=(m_next==this);

											CRY_ASSERT_MESSAGE(linkPrev==linkNext,"Linked list element in inconsistant state");

											return !linkPrev;
										}

};

class CDoubleLinkedList
{
	protected:
		CDoubleLinkedElement		m_contents;			// next is list head, prev is list tail


	public:
		CDoubleLinkedElement		*Head() const		{ return m_contents.m_next; }
		CDoubleLinkedElement		*Tail()	const		{ return m_contents.m_prev; }

		void						AddTail(
										CDoubleLinkedElement	*inLink)
									{
										CDoubleLinkedElement	*tail=Tail();
										tail->InsertAfter(inLink);
									}

		const CDoubleLinkedElement *GetLink()		{ return &m_contents; }

		template <class T>
		struct base_iterator
		{
			T			*element;

			base_iterator(T *inElement) :
				element(inElement)
			{
			}

			const T* operator*() const	{ return element; }
			const T* operator->() const { return element; }

			bool operator==(base_iterator<T> const &rhs) const { return element==rhs.element; }
			bool operator!=(base_iterator<T> const &rhs) const { return element!=rhs.element; }
		};

		template <class T>
		struct forward_iteratorT : public base_iterator<T>
		{
			forward_iteratorT(T *inElement) :
				base_iterator<T>(inElement)
			{
			}

			forward_iteratorT &operator++()
			{
				base_iterator<T>::element=base_iterator<T>::element->Next();
				return *this;
			}
		};

		template <class T>
		struct reverse_iteratorT : public base_iterator<T>
		{
			reverse_iteratorT(T *inElement) :
				base_iterator<T>(inElement)
			{
			}

			reverse_iteratorT &operator++()
			{
				base_iterator<T>::element=base_iterator<T>::element->Prev();

				return *this;
			}
		};

		typedef forward_iteratorT<CDoubleLinkedElement>			iterator;
		typedef reverse_iteratorT<CDoubleLinkedElement>			reverse_iterator;
		typedef forward_iteratorT<const CDoubleLinkedElement>	const_iterator;
		typedef reverse_iteratorT<const CDoubleLinkedElement>	const_reverse_iterator;

		const_iterator					begin() const		{ return const_iterator(Head()); }
		const_iterator					end() const			{ return const_iterator(&m_contents); }
		iterator						begin()				{ return iterator(Head()); }
		iterator						end()				{ return iterator(&m_contents); }

		const_reverse_iterator			rbegin() const		{ return const_reverse_iterator(Tail()); }
		const_reverse_iterator			rend() const		{ return const_reverse_iterator(&m_contents); }
		reverse_iterator				rbegin()			{ return reverse_iterator(Tail()); }
		reverse_iterator				rend()				{ return reverse_iterator(&m_contents); }

		size_t							size() const
										{
											int		count=0;

											for (const_iterator iter=begin(), iend=end(); iter!=iend; ++iter)
											{
												count++;
											}

											return count;
										}

		bool							empty() const
										{
											return !m_contents.IsInList();
										}
};


#endif // __DOUBLELINKEDLIST_H__

