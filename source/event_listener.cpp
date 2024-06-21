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

	void EventListener::Register(GrayEngine* e, GREvent::MousePosition x)
	{
		std::for_each(mousemove.begin(), mousemove.end(), [=](void(*f)(GrayEngine*, GREvent::MousePosition))
			{
				f(e, x);
			});
	}

	void EventListener::Register(GrayEngine* e, GREvent::ScrollDelta x)
	{
		std::for_each(scroll.begin(), scroll.end(), [=](void(*f)(GrayEngine*, GREvent::ScrollDelta))
			{
				f(e, x);
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

	void EventListener::Subscribe(void(*f)(GrayEngine*, GREvent::MousePosition))
	{
		mousemove.push_back(f);
	}

	void EventListener::Subscribe(void(*f)(GrayEngine*, GREvent::ScrollDelta))
	{
		scroll.push_back(f);
	}
};