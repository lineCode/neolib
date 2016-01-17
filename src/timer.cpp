// timer.cpp
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
#include "timer.hpp"

namespace neolib
{
	timer::timer(io_thread& aOwnerThread, uint32_t aDuration_ms, bool aInitialWait) : 
		iOwnerThread(aOwnerThread),
		iTimerObject(aOwnerThread.timer_io_service().native_object()),
		iDuration_ms(aDuration_ms), 
		iEnabled(true),
		iWaiting(false), 
		iCancelling(false),
		iDestroying(false),
		iInReady(false)
	{
		if (aInitialWait)
			again();
	}

	timer::timer(const timer& aOther) :
		iOwnerThread(aOther.iOwnerThread),
		iTimerObject(aOther.iOwnerThread.timer_io_service().native_object()),
		iDuration_ms(aOther.iDuration_ms), 
		iEnabled(aOther.iEnabled),
		iWaiting(false), 
		iCancelling(false),
		iDestroying(false),
		iInReady(false)
	{
		if (aOther.waiting())
			again();
	}
	
	timer& timer::operator=(const timer& aOther)
	{
		if (waiting())
		{
			neolib::destroyable::destroyed_flag destroyed(*this);
			cancel();
			if (destroyed)
				throw timer_destroyed();
		}
		iDuration_ms = aOther.iDuration_ms;
		iEnabled = aOther.iEnabled;
		if (aOther.waiting())
			again();
		return *this;
	}
	
	timer::~timer()
	{
		iDestroying = true;
		cancel();
	}

	io_thread& timer::owner_thread() const
	{
		return iOwnerThread;
	}

	void timer::enable(bool aWait)
	{
		if (iEnabled)
			throw already_enabled();
		iEnabled = true;
		if (aWait)
			again();
	}

	void timer::disable()
	{
		if (!iEnabled)
			throw already_disabled();
		if (waiting())
			cancel();
		iEnabled = false;
	}

	bool timer::enabled() const
	{
		return iEnabled;
	}

	bool timer::disabled() const
	{
		return !iEnabled;
	}

	void timer::again()
	{
		if (disabled())
			enable(false);
		if (waiting())
			throw already_waiting();
		if (cancelling())
			throw in_cancel();
		iTimerObject.expires_from_now(boost::posix_time::milliseconds(iDuration_ms));
		iTimerObject.async_wait(boost::bind(&timer::handler, this, boost::asio::placeholders::error));
		iWaiting = true;
	}

	void timer::again_if()
	{
		if (!waiting())
			again();
	}

	void timer::cancel()
	{
		if (cancelling())
			return;
		if (!waiting())
			return;
		iCancelling = true;
		if (iTimerObject.cancel() == 0)
		{
			iCancelling = false;
			return;
		}
		if (iDestroying && std::uncaught_exception())
			return;
		neolib::destroyable::destroyed_flag destroyed(*this);
		while (waiting())
		{
			iOwnerThread.timer_io_service().do_io(false);
			if (destroyed)
				return;
		}
		if (!iDestroying)
			iCancelling = false;
	}

	void timer::reset()
	{
		neolib::destroyable::destroyed_flag destroyed(*this);
		cancel();
		if (destroyed)
			return;
		again();
	}

	bool timer::waiting() const
	{
		return iWaiting;
	}

	bool timer::cancelling() const
	{
		return iCancelling;
	}

	uint32_t timer::duration() const
	{
		return iDuration_ms;
	}

	void timer::set_duration(uint32_t aDuration_ms, bool aEffectiveImmediately)
	{
		iDuration_ms = aDuration_ms;
		if (aEffectiveImmediately && waiting())
		{
			neolib::destroyable::destroyed_flag destroyed(*this);
			cancel();
			if (destroyed)
				return;
			again();
		}
	}

	uint32_t timer::duration_ms() const
	{
		return iDuration_ms;
	}

	void timer::handler(const boost::system::error_code& aError)
	{
		if (iInReady)
			return;
		iWaiting = false;
		if (!aError && enabled() && !cancelling())
		{
			try
			{
				iInReady = true;
				neolib::destroyable::destroyed_flag destroyed(*this);
				ready();
				if (destroyed)
					return;
				iInReady = false;
			}
			catch (...)
			{
				iInReady = false;
				throw;
			}
		}
	}

	callback_timer::callback_timer(io_thread& aOwnerThread, std::function<void(callback_timer&)> aCallback, uint32_t aDuration_ms, bool aInitialWait) :
		timer(aOwnerThread, aDuration_ms, aInitialWait),
		iCallback(aCallback)
	{
	}

	callback_timer::~callback_timer()
	{
		cancel();
	}

	void callback_timer::ready()
	{
		iCallback(*this);
	}
}