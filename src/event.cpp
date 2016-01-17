// event.cpp - v2.0
/*
 *  Copyright (c) 2012 Leigh Johnston.
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

#include "neolib.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>
#include "event.hpp"
#include "thread.hpp"

namespace neolib
{
	event::event() : iReady(false), iTotalWaiting(0)
	{
	}

	void event::signal_one() const
	{
		boost::lock_guard<boost::mutex> lock(iMutex);
		iReady = true;
		iSignalType = SignalOne;
		iCondVar.notify_one();
	}

	void event::signal_all() const
	{
		boost::lock_guard<boost::mutex> lock(iMutex);
		iReady = true;
		iSignalType = SignalAll;
		iCondVar.notify_all();
	}

	void event::wait() const
	{
		boost::unique_lock<boost::mutex> lock(iMutex);
		++iTotalWaiting;
		while (!iReady)
			iCondVar.wait(lock);
		--iTotalWaiting;
		if (iSignalType == SignalOne || iTotalWaiting == 0)
			iReady = false;
	}

	bool event::wait(uint32_t aTimeout_ms) const
	{
		bool result = true;
		boost::unique_lock<boost::mutex> lock(iMutex);
		++iTotalWaiting;
		if (!iReady)
			result = iCondVar.timed_wait(lock, boost::posix_time::milliseconds(aTimeout_ms));
		--iTotalWaiting;
		if (result && iSignalType == SignalOne || iTotalWaiting == 0)
			iReady = false;
		return result;
	}

	bool event::msg_wait(const message_queue& aMessageQueue) const
	{
		for(;;)
		{
			if (wait(0))
				return true;
			else if (aMessageQueue.have_message())
				return false;
			thread::sleep(1);
		}
	}

	bool event::msg_wait(const message_queue& aMessageQueue, uint32_t aTimeout_ms) const
	{
		boost::posix_time::ptime startTime = boost::posix_time::microsec_clock::local_time();
		for(;;)
		{
			if (wait(0))
				return true;
			else if (aMessageQueue.have_message())
				return false;
			else if ((boost::posix_time::microsec_clock::local_time() - startTime).total_milliseconds() > aTimeout_ms)
				return false;
			thread::sleep(1);
		}
	}

	void event::reset() const
	{
		boost::lock_guard<boost::mutex> lock(iMutex);
		iReady = false;
	}

	wait_result event_list::wait() const
	{
		for(;;)
		{
			for (list_type::const_iterator i = iEvents.begin(); i != iEvents.end(); ++i)
				if ((**i).wait(0))
					return wait_result_event(**i);
			thread::sleep(1);
		}
	}

	wait_result event_list::wait(const waitable& aWaitable) const
	{
		for(;;)
		{
			for (list_type::const_iterator i = iEvents.begin(); i != iEvents.end(); ++i)
				if ((**i).wait(0))
					return wait_result_event(**i);
			if (aWaitable.waitable_ready())
				return wait_result_waitable();
			thread::sleep(1);
		}
	}

	wait_result event_list::msg_wait(const message_queue& aMessageQueue) const
	{
		for(;;)
		{
			for (list_type::const_iterator i = iEvents.begin(); i != iEvents.end(); ++i)
				if ((**i).wait(0))
					return wait_result_event(**i);
			if (aMessageQueue.have_message())
				return wait_result_message();
			thread::sleep(1);
		}
	}

	wait_result event_list::msg_wait(const message_queue& aMessageQueue, const waitable& aWaitable) const
	{
		for(;;)
		{
			for (list_type::const_iterator i = iEvents.begin(); i != iEvents.end(); ++i)
				if ((**i).wait(0))
					return wait_result_event(**i);
			if (aMessageQueue.have_message())
				return wait_result_message();
			if (aWaitable.waitable_ready())
				return wait_result_waitable();
			thread::sleep(1);
		}
	}
} // namespace neolib