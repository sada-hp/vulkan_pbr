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

	void EventListener::Register(GrayEngine* c, GREvent::KeyPress e)
	{
		std::for_each(keypress.begin(), keypress.end(), [=](void(*f)(GrayEngine*, GREvent::KeyPress))
		{
			f(c, e);
		});
	}

	void EventListener::Register(GrayEngine* c, GREvent::MousePress e)
	{
		std::for_each(mousepress.begin(), mousepress.end(), [=](void(*f)(GrayEngine*, GREvent::MousePress))
		{
			f(c, e);
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

	void EventListener::Subscribe(void(*f)(GrayEngine*, GREvent::KeyPress))
	{
		keypress.push_back(f);
	}

	void EventListener::Subscribe(void(*f)(GrayEngine*, GREvent::MousePress))
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