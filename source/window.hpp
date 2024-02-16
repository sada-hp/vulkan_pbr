#pragma once
#include "core.hpp"
#include "renderer.hpp"

class GrayEnigne;

namespace GR
{
	class Window
	{
		friend class GrayEngine;

		GLFWwindow* glfwWindow;

	protected:
		Window(const char* title, int width, int height);

		~Window();

	public:
		GRAPI void SetTitle(const char* title);

		GRAPI void SetWindowSize(int width, int height);

		GRAPI void MinimizeWindow();

		GRAPI void SetAttribute(int attrib, int value);
	};
}