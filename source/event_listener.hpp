#pragma once
#include "core.hpp"
#include "enums.hpp"

namespace GR
{
	class GrayEngine;

	class EventListener
	{
		friend class GrayEngine;

		EventListener();

		~EventListener();

		std::vector<void(*)(GrayEngine*, EKey, EAction)> keypress;
		std::vector<void(*)(GrayEngine*, EMouse, EAction)> mousepress;
		std::vector<void(*)(GrayEngine*, double, double)> mousemove;

		void Register(GrayEngine*, EKey, EAction);

		void Register(GrayEngine*, EMouse, EAction);

		void Register(GrayEngine*, double, double);

	public:
		GRAPI void Subscribe(void(*key_press_callback_func)(GrayEngine*, EKey, EAction));

		GRAPI void Subscribe(void(*mouse_press_callback_func)(GrayEngine*, EMouse, EAction));

		GRAPI void Subscribe(void(*mouse_move_callback_func)(GrayEngine*, double, double));
	};
};