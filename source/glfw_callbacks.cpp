#include "engine.hpp"

namespace GR
{
	void GrayEngine::glfw_key_press(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		EngineContext* context = static_cast<EngineContext*>(glfwGetWindowUserPointer(window));
		context->listener->Register(context->engine, (EKey)key, (EAction)action);

#ifdef INCLUDE_GUI
		ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
#endif
	}

	void GrayEngine::glfw_mouse_press(GLFWwindow* window, int button, int action, int mods)
	{
		EngineContext* context = static_cast<EngineContext*>(glfwGetWindowUserPointer(window));
		context->listener->Register(context->engine, (EMouse)button, (EAction)action);
	
#ifdef INCLUDE_GUI
		ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
#endif
	}

	void GrayEngine::glfw_mouse_move(GLFWwindow* window, double xpos, double ypos)
	{
		EngineContext* context = static_cast<EngineContext*>(glfwGetWindowUserPointer(window));
		context->listener->Register(context->engine, xpos, ypos);

#ifdef INCLUDE_GUI
		ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
#endif
	}

	void GrayEngine::glfw_resize(GLFWwindow* window, int width, int height)
	{
		EngineContext* context = static_cast<EngineContext*>(glfwGetWindowUserPointer(window));
		context->renderer->HandleResize();
	}
};