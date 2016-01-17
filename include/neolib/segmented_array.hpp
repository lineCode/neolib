/*
*  segmented_array.hpp - v3.1
*
*  Copyright (c) 2014 Leigh Johnston.
*
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions are
*  met:
*
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*
*     * Neither the name of Leigh Johnston nor the names of any
*       other contributors to this software may be used to endorse or
*       promote products derived from this software without specific prior
*       written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
*  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
*  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
*  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
*  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
*  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
*  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
*  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
*  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
*  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
*  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "neolib.hpp"
#include <memory>
#include <iterator>
#include "vecarray.hpp"
#include "array_tree.hpp"

namespace neolib
{
	template <typename T, std::size_t SegmentSize = 64, typename Alloc = std::allocator<T> >
	class segmented_array : private array_tree<Alloc>
	{
	public:
		typedef T value_type;
		typedef Alloc allocator_type;
		typedef typename allocator_type::reference reference;
		typedef typename allocator_type::pointer pointer;
		typedef typename allocator_type::const_reference const_reference;
		typedef typename allocator_type::const_pointer const_pointer;
		typedef typename allocator_type::size_type size_type;
		typedef typename allocator_type::difference_type difference_type;
	private:
		typedef array_tree<Alloc> base;
		typedef neolib::vecarray<T, SegmentSize, SegmentSize, neolib::nocheck> segment_type;
		class node : public base::node
		{
		public:
			node()
			{
			}

		public:
			const segment_type& segment() const
			{
				return iSegment;
			}
			segment_type& segment()
			{
				return iSegment;
			}

		private:
			segment_type iSegment;
		};
		typedef typename allocator_type:: template rebind<node>::other node_allocator_type;
	public:
		class iterator : public std::iterator<std::random_access_iterator_tag, value_type, difference_type, pointer, reference>
		{
			friend segmented_array;
			friend class const_iterator;
		private:
			typedef std::iterator<std::random_access_iterator_tag, value_type, difference_type, pointer, reference> base;

		public:
			iterator() :
				iContainer(), iNode(), iContainerPosition(), iSegmentPosition()
			{
			}
			iterator(const iterator& aOther) :
				iContainer(aOther.iContainer), iNode(aOther.iNode), iContainerPosition(aOther.iContainerPosition), iSegmentPosition(aOther.iSegmentPosition)
			{
			}
			iterator& operator=(const iterator& aOther)
			{
				iContainer = aOther.iContainer;
				iNode = aOther.iNode;
				iContainerPosition = aOther.iContainerPosition;
				iSegmentPosition = aOther.iSegmentPosition;
				return *this;
			}
		private:
			iterator(segmented_array& aContainer, size_type aContainerPosition) :
				iContainer(&aContainer), iNode(0), iContainerPosition(aContainerPosition), iSegmentPosition(0)
			{
				iNode = iContainer->find_node(aContainerPosition, iSegmentPosition);
				if (iNode->is_nil())
					*this = iContainer->end();
			}
			iterator(segmented_array& aContainer, node* aNode, size_type aContainerPosition, size_type aSegmentPosition) :
				iContainer(&aContainer), iNode(aNode), iContainerPosition(aContainerPosition), iSegmentPosition(aSegmentPosition)
			{
			}

		public:
			iterator& operator++()
			{
				++iContainerPosition;
				if (++iSegmentPosition == static_cast<node*>(iNode)->segment().size())
				{
					if (iNode != iContainer->back_node())
					{
						iNode = static_cast<node*>(iNode->next());
						iSegmentPosition = 0;
					}
				}
				return *this;
			}
			iterator& operator--()
			{
				--iContainerPosition;
				if (iSegmentPosition-- == 0)
				{
					iNode = static_cast<node*>(iNode->previous());
					iSegmentPosition = static_cast<node*>(iNode)->segment().size() - 1;
				}
				return *this;
			}
			iterator operator++(int) { iterator ret(*this); operator++(); return ret; }
			iterator operator--(int) { iterator ret(*this); operator--(); return ret; }
			iterator& operator+=(difference_type aDifference)
			{
				if (aDifference < 0)
					return operator-=(-aDifference);
				else if (iNode == 0 || aDifference >= static_cast<difference_type>(segment().size() - iSegmentPosition))
					*this = iterator(*iContainer, iContainerPosition + aDifference);
				else
				{
					iContainerPosition += aDifference;
					iSegmentPosition += aDifference;
				}
				return *this;
			}
			iterator& operator-=(difference_type aDifference)
			{
				if (aDifference < 0)
					return operator+=(-aDifference);
				else if (aDifference > static_cast<difference_type>(iSegmentPosition))
					*this = iterator(*iContainer, iContainerPosition - aDifference);
				else
				{
					iContainerPosition -= aDifference;
					iSegmentPosition -= aDifference;
				}
				return *this;
			}
			iterator operator+(difference_type aDifference) const { iterator result(*this); result += aDifference; return result; }
			iterator operator-(difference_type aDifference) const { iterator result(*this); result -= aDifference; return result; }
			reference operator[](difference_type aDifference) const { return *((*this) + aDifference); }
			difference_type operator-(const iterator& aOther) const { return static_cast<difference_type>(iContainerPosition)-static_cast<difference_type>(aOther.iContainerPosition); }
			reference operator*() const { return segment()[iSegmentPosition]; }
			pointer operator->() const { return &operator*(); }
			bool operator==(const iterator& aOther) const { return iContainerPosition == aOther.iContainerPosition; }
			bool operator!=(const iterator& aOther) const { return iContainerPosition != aOther.iContainerPosition; }
			bool operator<(const iterator& aOther) const { return iContainerPosition < aOther.iContainerPosition; }

		private:
			segment_type& segment() const { return iNode->segment(); }

		private:
			segmented_array* iContainer;
			node* iNode;
			size_type iContainerPosition;
			size_type iSegmentPosition;
		};
		class const_iterator : public std::iterator<std::random_access_iterator_tag, value_type, difference_type, const_pointer, const_reference>
		{
			friend segmented_array;
		private:
			typedef std::iterator<std::random_access_iterator_tag, value_type, difference_type, const_pointer, const_reference> base;

		public:
			const_iterator() :
				iContainer(), iNode(), iContainerPosition(), iSegmentPosition()
			{
			}
			const_iterator(const const_iterator& aOther) :
				iContainer(aOther.iContainer), iNode(aOther.iNode), iContainerPosition(aOther.iContainerPosition), iSegmentPosition(aOther.iSegmentPosition)
			{
			}
			const_iterator(const typename segmented_array::iterator& aOther) :
				iContainer(aOther.iContainer), iNode(aOther.iNode), iContainerPosition(aOther.iContainerPosition), iSegmentPosition(aOther.iSegmentPosition)
			{
			}
			const_iterator& operator=(const const_iterator& aOther)
			{
				iContainer = aOther.iContainer;
				iNode = aOther.iNode;
				iContainerPosition = aOther.iContainerPosition;
				iSegmentPosition = aOther.iSegmentPosition;
				return *this;
			}
			const_iterator& operator=(const typename segmented_array::iterator& aOther)
			{
				iContainer = aOther.iContainer;
				iNode = aOther.iNode;
				iContainerPosition = aOther.iContainerPosition;
				iSegmentPosition = aOther.iSegmentPosition;
				return *this;
			}
		private:
			const_iterator(const segmented_array& aContainer, size_type aContainerPosition) :
				iContainer(&aContainer), iNode(0), iContainerPosition(aContainerPosition), iSegmentPosition(0)
			{
				iNode = iContainer->find_node(aContainerPosition, iSegmentPosition);
				if (iNode->is_nil())
					*this = iContainer->end();
			}
			const_iterator(const segmented_array& aContainer, node* aNode, size_type aContainerPosition, size_type aSegmentPosition) :
				iContainer(&aContainer), iNode(aNode), iContainerPosition(aContainerPosition), iSegmentPosition(aSegmentPosition)
			{
			}

		public:
			const_iterator& operator++()
			{
				++iContainerPosition;
				if (++iSegmentPosition == static_cast<node*>(iNode)->segment().size())
				{
					if (iNode != iContainer->back_node())
					{
						iNode = static_cast<node*>(iNode->next());
						iSegmentPosition = 0;
					}
				}
				return *this;
			}
			const_iterator& operator--()
			{
				--iContainerPosition;
				if (iSegmentPosition-- == 0)
				{
					iNode = static_cast<node*>(iNode->previous());
					iSegmentPosition = static_cast<node*>(iNode)->segment().size() - 1;
				}
				return *this;
			}
			const_iterator operator++(int) { const_iterator ret(*this); operator++(); return ret; }
			const_iterator operator--(int) { const_iterator ret(*this); operator--(); return ret; }
			const_iterator& operator+=(difference_type aDifference)
			{
				if (aDifference < 0)
					return operator-=(-aDifference);
				else if (iNode == 0 || aDifference >= static_cast<difference_type>(segment().size() - iSegmentPosition))
					*this = const_iterator(*iContainer, iContainerPosition + aDifference);
				else
				{
					iContainerPosition += aDifference;
					iSegmentPosition += aDifference;
				}
				return *this;
			}
			const_iterator& operator-=(difference_type aDifference)
			{
				if (aDifference < 0)
					return operator+=(-aDifference);
				else if (aDifference > static_cast<difference_type>(iSegmentPosition))
					*this = const_iterator(*iContainer, iContainerPosition - aDifference);
				else
				{
					iContainerPosition -= aDifference;
					iSegmentPosition -= aDifference;
				}
				return *this;
			}
			const_iterator operator+(difference_type aDifference) const { const_iterator result(*this); result += aDifference; return result; }
			const_iterator operator-(difference_type aDifference) const { const_iterator result(*this); result -= aDifference; return result; }
			const_reference operator[](difference_type aDifference) const { return *((*this) + aDifference); }
			difference_type operator-(const const_iterator& aOther) const { return static_cast<difference_type>(iContainerPosition)-static_cast<difference_type>(aOther.iContainerPosition); }
			const_reference operator*() const { return segment()[iSegmentPosition]; }
			const_pointer operator->() const { return &operator*(); }
			bool operator==(const const_iterator& aOther) const { return iContainerPosition == aOther.iContainerPosition; }
			bool operator!=(const const_iterator& aOther) const { return iContainerPosition != aOther.iContainerPosition; }
			bool operator<(const const_iterator& aOther) const { return iContainerPosition < aOther.iContainerPosition; }

		private:
			segment_type& segment() const { return iNode->segment(); }

		private:
			const segmented_array* iContainer;
			node* iNode;
			size_type iContainerPosition;
			size_type iSegmentPosition;
		};
		typedef std::reverse_iterator<iterator> reverse_iterator;
		typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

	public:
		segmented_array(const Alloc& aAllocator = Alloc()) :
			iAllocator(aAllocator), iSize(0)
		{
		}
		segmented_array(const size_type aCount, const value_type& aValue, const Alloc& aAllocator = Alloc()) :
			iAllocator(aAllocator), iSize(0)
		{
			insert(begin(), aCount, aValue);
		}
		template <typename InputIterator>
		segmented_array(InputIterator aFirst, InputIterator aLast, const Alloc& aAllocator = Alloc()) :
			iAllocator(aAllocator), iSize(0)
		{
			insert(begin(), aFirst, aLast);
		}
		~segmented_array()
		{
			erase(begin(), end());
		}

	public:
		size_type size() const
		{
			return iSize;
		}
		bool empty() const
		{
			return iSize == 0;
		}
		const_iterator begin() const
		{
			return const_iterator(*this, static_cast<node*>(base::front_node()), 0, 0);
		}
		const_iterator end() const
		{
			return const_iterator(*this, static_cast<node*>(base::back_node()), iSize, base::back_node() ? static_cast<node*>(base::back_node())->segment().size() : 0);
		}
		iterator begin()
		{
			return iterator(*this, static_cast<node*>(base::front_node()), 0, 0);
		}
		iterator end()
		{
			return iterator(*this, static_cast<node*>(base::back_node()), iSize, base::back_node() ? static_cast<node*>(base::back_node())->segment().size() : 0);
		}
		const_reverse_iterator rbegin() const
		{
			return const_reverse_iterator(end());
		}
		const_reverse_iterator rend() const
		{
			return const_reverse_iterator(begin());
		}
		reverse_iterator rbegin()
		{
			return reverse_iterator(end());
		}
		reverse_iterator rend()
		{
			return reverse_iterator(begin());
		}
		const_reference front() const
		{
			return *begin();
		}
		reference front()
		{
			return *begin();
		}
		const_reference back() const
		{
			return *--end();
		}
		reference back()
		{
			return *--end();
		}
		iterator insert(const_iterator aPosition, const value_type& aValue)
		{
			return insert(aPosition, 1, aValue);
		}
		const_reference operator[](size_type aIndex) const
		{
			return *(begin() + aIndex);
		}
		reference operator[](size_type aIndex)
		{
			return *(begin() + aIndex);
		}
		template <class InputIterator>
		typename std::enable_if<!std::is_integral<InputIterator>::value, iterator>::type
		insert(const_iterator aPosition, InputIterator aFirst, InputIterator aLast)
		{
			return do_insert(aPosition, aFirst, aLast);
		}
		iterator insert(const_iterator aPosition, size_type aCount, const value_type& aValue)
		{
			if (aCount == 0)
				return iterator(*this, aPosition.iNode, aPosition.iContainerPosition, aPosition.iSegmentPosition);
			while (aCount > 0)
			{
				aPosition = insert(aPosition, &aValue, &aValue+1);
				++aPosition;
				--aCount;
			}
			return iterator(*this, aPosition.iContainerPosition);
		}
		void push_front(const value_type& aValue)
		{
			insert(begin(), aValue);
		}
		void push_back(const value_type& aValue)
		{
			insert(end(), aValue);
		}
		void resize(std::size_t aNewSize, const value_type& aValue = value_type())
		{
			if (size() < aNewSize)
				insert(end(), aNewSize - size(), aValue);
			else
				erase(begin() + aNewSize, end());
		}
		iterator erase(const_iterator aPosition)
		{
			erase(aPosition, aPosition + 1);
			return iterator(*this, aPosition.iContainerPosition);
		}
		iterator erase(const_iterator aFirst, const_iterator aLast)
		{
			if (aFirst == aLast)
				return iterator(*this, aFirst.iNode, aFirst.iContainerPosition, aFirst.iSegmentPosition);
			segment_type& segmentFirst = aFirst.iNode->segment();
			if (aFirst.iNode == aLast.iNode)
			{
				segmentFirst.erase(segmentFirst.begin() + aFirst.iSegmentPosition, segmentFirst.begin() + aLast.iSegmentPosition);
				iSize -= (aLast.iSegmentPosition - aFirst.iSegmentPosition);
				aFirst.iNode->set_size(aFirst.iNode->size() - (aLast.iSegmentPosition - aFirst.iSegmentPosition));
				if (segmentFirst.empty())
					free_node(aFirst.iNode);
			}
			else
			{
				segment_type& segmentLast = aLast.iNode->segment();
				for (node* inbetweenNode = static_cast<node*>(aFirst.iNode->next()); inbetweenNode != aLast.iNode;)
				{
					node* next = static_cast<node*>(inbetweenNode->next());
					size_type inbetweenRemoved = inbetweenNode->segment().size();
					free_node(inbetweenNode);
					iSize -= inbetweenRemoved;
					inbetweenNode = next;
				}
				size_type firstRemoved = segmentFirst.size() - aFirst.iSegmentPosition;
				size_type secondRemoved = aLast.iSegmentPosition;
				segmentFirst.erase(segmentFirst.begin() + aFirst.iSegmentPosition, segmentFirst.end());
				segmentLast.erase(segmentLast.begin(), segmentLast.begin() + aLast.iSegmentPosition);
				if (segmentFirst.empty())
					free_node(aFirst.iNode);
				else
					aFirst.iNode->set_size(aFirst.iNode->size() - firstRemoved);
				iSize -= firstRemoved;
				if (segmentLast.empty())
					free_node(aLast.iNode);
				else
					aLast.iNode->set_size(aLast.iNode->size() - secondRemoved);
				iSize -= secondRemoved;
			}
			return iterator(*this, aFirst.iContainerPosition);
		}
		void pop_front()
		{
			erase(begin());
		}
		void pop_back()
		{
			erase(--end());
		}
		void swap(segmented_array& aOther)
		{
			base::swap(aOther);
			std::swap(iSize, aOther.iSize);
		}

	private:
		template <class InputIterator>
		typename std::enable_if<!std::is_same<typename std::iterator_traits<InputIterator>::iterator_category, std::input_iterator_tag>::value, iterator>::type
		do_insert(const_iterator aPosition, InputIterator aFirst, InputIterator aLast)
		{
			size_type count = std::distance(aFirst, aLast);
			if (count == 0)
				return iterator(*this, aPosition.iNode, aPosition.iContainerPosition, aPosition.iSegmentPosition);
			node* before = aPosition.iNode;
			node* after = aPosition.iNode ? static_cast<node*>(aPosition.iNode->next()) : 0;
			node* lastNode = aPosition.iNode;
			if (aPosition.iNode != 0 && count <= static_cast<node*>(aPosition.iNode)->segment().available())
			{
				segment_type& segment = static_cast<node*>(aPosition.iNode)->segment();
				segment.insert(segment.begin() + aPosition.iSegmentPosition, aFirst, aLast);
				iSize += count;
				aPosition.iNode->set_size(aPosition.iNode->size() + count);
			}
			else
			{
				lastNode = allocate_space(aPosition, count);
				if (aPosition.iNode == 0)
					aPosition = begin();
				segment_type& segment = aPosition.iNode->segment();
				typename segment_type::const_iterator tailEnd = segment.end();
				typename segment_type::const_iterator tailStart = tailEnd - std::min(segment.size() - aPosition.iSegmentPosition, count);
				if (tailStart != tailEnd)
				{
					typename segment_type::const_iterator tailMid = tailEnd - std::min(static_cast<size_type>(tailEnd - tailStart), lastNode->segment().available());
					lastNode->segment().insert(lastNode->segment().begin(), tailMid, tailEnd);
					lastNode->set_size(lastNode->size() + (tailEnd - tailMid));
					if (tailMid != tailStart)
					{
						static_cast<node*>(lastNode->previous())->segment().insert(static_cast<node*>(lastNode->previous())->segment().end(), tailStart, tailMid);
						lastNode->previous()->set_size(lastNode->previous()->size() + (tailMid - tailStart));
					}
					segment.erase(tailStart, tailEnd);
					aPosition.iNode->set_size(aPosition.iNode->size() - (tailEnd - tailStart));
				}
				for (node* nextNode = aPosition.iNode; count > 0 && nextNode != lastNode; nextNode = static_cast<node*>(nextNode->next()))
				{
					size_type addCount = std::min(count, nextNode->segment().available());
					if (addCount != 0)
					{
						InputIterator stop = aFirst;
						std::advance(stop, addCount);
						nextNode->segment().insert(nextNode->segment().begin() + (nextNode == aPosition.iNode ? aPosition.iSegmentPosition : 0), aFirst, stop);
						aFirst = stop;
						iSize += addCount;
						count -= addCount;
						nextNode->set_size(nextNode->size() + addCount);
					}
				}
				if (count > 0)
				{
					InputIterator stop = aFirst;
					std::advance(stop, count);
					lastNode->segment().insert(lastNode->segment().begin() + (lastNode == aPosition.iNode ? aPosition.iSegmentPosition : 0), aFirst, stop);
					lastNode->set_size(lastNode->size() + count);
					iSize += count;
				}
			}
			size_type index = aPosition.iContainerPosition - aPosition.iSegmentPosition;
			for (node* newNode = aPosition.iNode;; newNode = static_cast<node*>(newNode->next()))
			{
				if (newNode != before && newNode != after)
				{
					base::insert_node(newNode, index);
				}
				index += newNode->segment().size();
				if (newNode == lastNode)
					break;
			}
			if (aPosition.iSegmentPosition != aPosition.iNode->segment().size()) // was not end
				return iterator(*this, aPosition.iNode, aPosition.iContainerPosition, aPosition.iSegmentPosition);
			else
				return iterator(*this, static_cast<node*>(aPosition.iNode->next()), aPosition.iContainerPosition, 0);
		}
		template <class InputIterator>
		typename std::enable_if<std::is_same<typename std::iterator_traits<InputIterator>::iterator_category, std::input_iterator_tag>::value, iterator>::type
		do_insert(const_iterator aPosition, InputIterator aFirst, InputIterator aLast)
		{
			while (aFirst != aLast)
			{
				aPosition = insert(aPosition, 1, *aFirst++);
				++aPosition;
			}
			return iterator(*this, aPosition.iContainerPosition);
		}
		node* find_node(size_type aContainerPosition, size_type& aSegmentPosition) const
		{
			size_type nodeIndex = 0;
			node* result = static_cast<node*>(base::find_node(aContainerPosition, nodeIndex));
			aSegmentPosition = aContainerPosition - nodeIndex;
			return result;
		}
		node* allocate_space(const_iterator aPosition, size_type aCount)
		{
			if (aCount == 0)
				return aPosition.iNode;
			if (aPosition.iNode)
				aCount -= std::min(aCount, (aPosition.iNode->segment().available()));
			node* lastNode = 0;
			if (aCount > 0 && aPosition.iNode && aPosition.iNode->next() != 0 && !static_cast<node*>(aPosition.iNode->next())->segment().full())
			{
				lastNode = static_cast<node*>(aPosition.iNode->next());
				aCount -= std::min(aCount, lastNode->segment().available());
			}
			node* nextNode = aPosition.iNode;
			while (aCount > 0)
				aCount -= std::min(aCount, (nextNode = allocate_node(nextNode))->segment().available());
			return lastNode ? lastNode : nextNode;
		}
		node* allocate_node(node* aAfter)
		{
			node* newNode = iAllocator.allocate(1);
			iAllocator.construct(newNode, node());
			if (aAfter == 0)
			{
				base::set_front_node(newNode);
				base::set_back_node(newNode);
			}
			else
			{
				newNode->set_previous(aAfter);
				if (aAfter->next() != 0)
				{
					newNode->set_next(aAfter->next());
					aAfter->next()->set_previous(newNode);
				}
				aAfter->set_next(newNode);
				if (base::back_node() == aAfter)
					base::set_back_node(newNode);
			}
			return newNode;
		}
		void free_node(node* aNode)
		{
			if (aNode)
			{
				if (aNode->next())
					aNode->next()->set_previous(aNode->previous());
				if (aNode->previous())
					aNode->previous()->set_next(aNode->next());
				if (base::back_node() == aNode)
					base::set_back_node(aNode->previous());
				if (base::front_node() == aNode)
					base::set_front_node(aNode->next());
				base::delete_node(aNode);
			}
			iAllocator.destroy(aNode);
			iAllocator.deallocate(aNode, 1);
		}

	private:
		node_allocator_type iAllocator;
		size_type iSize;
	};
}