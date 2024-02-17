#include "pch.hpp"
#include "event_listener.hpp"

namespace GR
{
	EventListener::EventListener()
	{
	}

	EventListener::~EventListener()
	{
	}

	void EventListener::Register(GrayEngine* e, EKey c, EAction a)
	{
		std::for_each(keypress.begin(), keypress.end(), [=](void(*f)(GrayEngine*, EKey, EAction))
		{
			f(e, c, a);
		});
	}

	void EventListener::Register(GrayEngine* e, EMouse m, EAction a)
	{
		std::for_each(mousepress.begin(), mousepress.end(), [=](void(*f)(GrayEngine*, EMouse, EAction))
		{
			f(e, m, a);
		});
	}

	void EventListener::Register(GrayEngine* e, double x, double y)
	{
		std::for_each(mousemove.begin(), mousemove.end(), [=](void(*f)(GrayEngine*, double, double))
			{
				f(e, x, y);
			});
	}

	void EventListener::Subscribe(void(*f)(GrayEngine*, EKey, EAction))
	{
		keypress.push_back(f);
	}

	void EventListener::Subscribe(void(*f)(GrayEngine*, EMouse, EAction))
	{
		mousepress.push_back(f);
	}

	void EventListener::Subscribe(void(*f)(GrayEngine*, double, double))
	{
		mousemove.push_back(f);
	}
};