// cookie_jar.hpp
/*
 *  Copyright (c) 2018 Leigh Johnston.
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
#include <vector>
#include <set>
#include <mutex>
#include <atomic>
#include <boost/stacktrace.hpp>

namespace neolib
{
	typedef uint32_t cookie;
	constexpr cookie no_cookie = cookie{};

	class i_cookie_jar_item
	{
	public:
		virtual ~i_cookie_jar_item() {}
	public:
		virtual neolib::cookie cookie() const = 0;
	};

	inline cookie item_cookie(const i_cookie_jar_item& aItem)
	{
		return aItem.cookie();
	}

	inline cookie item_cookie(const i_cookie_jar_item* aItem)
	{
		return aItem->cookie();
	}

	class cookie_auto_ref;

	class i_cookie_consumer
	{
	public:
		struct invalid_release : std::logic_error { invalid_release() : std::logic_error("neolib::i_cookie_consumer::invalid_release") {} };
	public:
		virtual ~i_cookie_consumer() {}
	public:
		virtual void add_ref(cookie aCookie) = 0;
		virtual void release(cookie aCookie) = 0;
		virtual long use_count(cookie aCookie) const = 0;
	};

	class cookie_auto_ref
	{
	public:
		cookie_auto_ref() :
			iConsumer{ nullptr },
			iCookie{ no_cookie }
		{
		}
		cookie_auto_ref(i_cookie_consumer& aConsumer, neolib::cookie aCookie) :
			iConsumer{ &aConsumer },
			iCookie{ aCookie }
		{
			add_ref();
		}
		~cookie_auto_ref()
		{
			release();
		}
		cookie_auto_ref(const cookie_auto_ref& aOther) :
			iConsumer{ aOther.iConsumer },
			iCookie{ aOther.iCookie }
		{
			add_ref();
		}
		cookie_auto_ref(cookie_auto_ref&& aOther) :
			iConsumer{ aOther.iConsumer },
			iCookie{ aOther.iCookie }
		{
			add_ref();
			aOther.release();
		}
	public:
		cookie_auto_ref& operator=(const cookie_auto_ref& aOther)
		{
			if (&aOther == this)
				return *this;
			cookie_auto_ref temp{ std::move(*this) };
			iConsumer = aOther.iConsumer;
			iCookie = aOther.iCookie;
			add_ref();
			return *this;
		}
		cookie_auto_ref& operator=(cookie_auto_ref&& aOther)
		{
			if (&aOther == this)
				return *this;
			cookie_auto_ref temp{ std::move(*this) };
			iConsumer = aOther.iConsumer;
			iCookie = aOther.iCookie;
			add_ref();
			aOther.release();
			return *this;
		}
	public:
		bool operator==(const cookie_auto_ref& aRhs) const
		{
			return iConsumer == aRhs.iConsumer && iCookie == aRhs.iCookie;
		}
		bool operator!=(const cookie_auto_ref& aRhs) const
		{
			return !(*this == aRhs);
		}
		bool operator<(const cookie_auto_ref& aRhs) const
		{
			return std::tie(iConsumer, iCookie) < std::tie(aRhs.iConsumer, aRhs.iCookie);
		}
	public:
		bool valid() const
		{
			return have_consumer() && have_cookie();
		}
		bool expired() const
		{
			return !valid();
		}
		neolib::cookie cookie() const
		{
			return iCookie;
		}
	private:
		void add_ref() const
		{
			if (!valid())
				return;
			consumer().add_ref(cookie());
		}
		void release() const
		{
			if (!valid())
				return;
			consumer().release(cookie());
			iConsumer = nullptr;
			iCookie = no_cookie;
		}
		bool have_consumer() const
		{
			return iConsumer != nullptr;
		}
		i_cookie_consumer& consumer() const
		{
			return *iConsumer;
		}
		bool have_cookie() const
		{
			return iCookie != no_cookie;
		}
	private:
		mutable i_cookie_consumer* iConsumer;
		mutable neolib::cookie iCookie;
	};

	template <typename T, typename MutexType = std::recursive_mutex>
	class cookie_jar
	{
	public:
		struct invalid_cookie : std::logic_error { invalid_cookie() : std::logic_error("neolib::cookie_jar::invalid_cookie") {} };
		struct cookie_already_added : std::logic_error { cookie_already_added() : std::logic_error("neolib::cookie_jar::cookie_already_added") {} };
		struct cookies_exhausted : std::logic_error { cookies_exhausted() : std::logic_error("neolib::cookie_jar::cookies_exhausted") {} };
	public:
		typedef T value_type;
		typedef std::vector<value_type> jar_t;
		typedef typename jar_t::const_iterator const_iterator;
		typedef typename jar_t::iterator iterator;
		typedef MutexType mutex_type;
	private:
		typedef typename jar_t::size_type reverse_index_t;
		typedef std::vector<reverse_index_t> reverse_indices_t;
		typedef std::vector<neolib::cookie> free_cookies_t;
	private:
		static constexpr neolib::cookie INVALID_COOKIE = neolib::cookie{ ~0ul };
		static constexpr reverse_index_t INVALID_REVERSE_INDEX = reverse_index_t{ ~0ul };
	public:
		cookie_jar() : iNextAvailableCookie{ 0ul }
		{
		}
	public:
		bool contains(neolib::cookie aCookie) const
		{
			std::lock_guard<mutex_type> lg{ mutex() };
			return aCookie < reverse_indices().size() && reverse_indices()[aCookie] != INVALID_REVERSE_INDEX;
		}
		const value_type& operator[](neolib::cookie aCookie) const
		{
			std::lock_guard<mutex_type> lg{ mutex() };
			if (aCookie >= reverse_indices().size())
				throw invalid_cookie();
			auto reverseIndex = reverse_indices()[aCookie];
			if (reverseIndex == INVALID_REVERSE_INDEX)
				throw invalid_cookie();
			return jar()[reverseIndex];
		}
		value_type& operator[](neolib::cookie aCookie)
		{
			return const_cast<value_type&>(const_cast<const cookie_jar&>(*this)[aCookie]);
		}
		iterator add(const value_type& aItem)
		{
			std::lock_guard<mutex_type> lg{ mutex() };
			auto cookie = item_cookie(aItem);
			if (reverse_indices().size() <= cookie)
				reverse_indices().resize(cookie + 1, INVALID_REVERSE_INDEX);
			if (reverse_indices()[cookie] != INVALID_REVERSE_INDEX)
				throw cookie_already_added();
			auto result = jar().insert(jar().end(), aItem);
			reverse_indices()[cookie] = jar().size() - 1;
			return result;
		}
		iterator remove(const value_type& aItem)
		{
			std::lock_guard<mutex_type> lg{ mutex() };
			return remove(item_cookie(aItem));
		}
		iterator remove(neolib::cookie aCookie)
		{
			std::lock_guard<mutex_type> lg{ mutex() };
			auto& reverseIndex = reverse_indices()[aCookie];
			if (reverseIndex == INVALID_REVERSE_INDEX)
				throw invalid_cookie();
			if (reverseIndex < jar().size() - 1)
			{
				auto& item = jar()[reverseIndex];
				std::swap(item, jar().back());
				reverse_indices()[item_cookie(item)] = reverseIndex;
			}
			jar().pop_back();
			iterator result = std::next(jar().begin(), reverseIndex);
			reverseIndex = INVALID_REVERSE_INDEX;
			return_cookie(aCookie);
			return result;
		}
	public:
		neolib::cookie next_cookie()
		{
			std::lock_guard<mutex_type> lg{ mutex() };
			if (!free_cookies().empty())
			{
				auto nextCookie = free_cookies().back();
				free_cookies().pop_back();
				return nextCookie;
			}
			auto nextCookie = ++iNextAvailableCookie;
			if (nextCookie == INVALID_COOKIE)
				throw cookies_exhausted();
			return nextCookie;
		}
		void return_cookie(neolib::cookie aCookie)
		{
			std::lock_guard<mutex_type> lg{ mutex() };
			free_cookies().push_back(aCookie);
		}
	public:
		mutex_type& mutex() const
		{
			return iMutex;
		}
		const_iterator cbegin() const
		{
			return jar().begin();
		}
		const_iterator begin() const
		{
			return cbegin();
		}
		iterator begin()
		{
			return jar().begin();
		}
		const_iterator cend() const
		{
			return jar().end();
		}
		const_iterator end() const
		{
			return cend();
		}
		iterator end()
		{
			return jar().end();
		}
	public:
		void clear()
		{
			std::lock_guard<mutex_type> lg{ mutex() };
			iNextAvailableCookie = 0ul;
			free_cookies().clear();
			jar().clear();
			reverse_indices().clear();
		}
	private:
		const jar_t& jar() const
		{
			return iJar;
		}
		jar_t& jar()
		{
			return iJar;
		}
		const reverse_indices_t& reverse_indices() const
		{
			return iReverseIndices;
		}
		reverse_indices_t& reverse_indices()
		{
			return iReverseIndices;
		}
		const free_cookies_t& free_cookies() const
		{
			return iFreeCookies;
		}
		free_cookies_t& free_cookies()
		{
			return iFreeCookies;
		}
	private:
		mutable mutex_type iMutex;
		mutable std::atomic<neolib::cookie> iNextAvailableCookie;
		mutable free_cookies_t iFreeCookies;
		jar_t iJar;
		reverse_indices_t iReverseIndices;
	};
}
