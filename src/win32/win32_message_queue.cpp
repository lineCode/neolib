#include "neolib.hpp"
#include "win32_message_queue.hpp"

namespace neolib
{
	std::map<UINT_PTR, win32_message_queue*> win32_message_queue::sTimerMap;

	win32_message_queue::win32_message_queue(io_thread& aIoThread, std::function<bool()> aIdleFunction, bool aCreateTimer) :
		iIoThread(aIoThread),
		iIdleFunction(aIdleFunction)
	{
		if (aCreateTimer)
		{
			iTimer = ::SetTimer(NULL, 0, 10, &win32_message_queue::timer_proc);
			sTimerMap[iTimer] = this;
		}
	}

	win32_message_queue::~win32_message_queue()
	{
		for (auto& t : sTimerMap)
			::KillTimer(NULL, t.first);
	}

	bool win32_message_queue::have_message() const
	{
		return ::PeekMessage(NULL, NULL, 0, 0, PM_NOREMOVE) != 0;
	}

	int win32_message_queue::get_message() const
	{
		MSG msg;
		int result = ::GetMessage(&msg, NULL, 0, 0);
		if (result)
		{
			if (result != -1)
			{
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
			}
		}
		return result;
	}

	void win32_message_queue::bump()
	{
		::PostMessage(NULL, WM_NULL, 0, 0);
	}

	void win32_message_queue::idle()
	{
		if (iIdleFunction)
			iIdleFunction();
	}

	void CALLBACK win32_message_queue::timer_proc(HWND, UINT, UINT_PTR aId, DWORD)
	{
		win32_message_queue& instance = *sTimerMap[aId];
		instance.idle();
	}
}