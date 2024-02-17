#include "pch.hpp"
#include "window.hpp"

namespace GR
{
	Window::Window(const char* title, int width, int height)
	{
		glfwInit();
		assert(glfwVulkanSupported());
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindow = glfwCreateWindow(width, height, title, nullptr, nullptr);
	}

	Window::~Window()
	{
		glfwTerminate();
		glfwDestroyWindow(glfwWindow);
	}

	void Window::SetTitle(const char* title)
	{
		glfwSetWindowTitle(glfwWindow, title);
	}

	void Window::SetWindowSize(int width, int height)
	{
		glfwSetWindowSize(glfwWindow, width, height);
	}

	void Window::MinimizeWindow()
	{
		glfwIconifyWindow(glfwWindow);
	}

	TIVec2 Window::GetWindowSize() const
	{
		int width, int height;
		glfwGetWindowSize(glfwWindow, &width, &height);
		return TIVec2{ width, height };
	}

	TVec2 Window::GetCursorPos() const
	{
		double xpos, ypos;
		glfwGetCursorPos(glfwWindow, &xpos, &ypos);
		return TVec2{ xpos, ypos };
	}

	void Window::SetCursorPos(double xpos, double ypos)
	{
		glfwSetCursorPos(glfwWindow, xpos, ypos);
	}

	void Window::SetAttribute(int attrib, int value)
	{
		glfwSetWindowAttrib(glfwWindow, attrib, value);
	}
};