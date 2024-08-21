#pragma once
#include "core.hpp"
#include "Engine/math.hpp"
#include "Vulkan/renderer.hpp"

class GrayEnigne;

namespace GR
{
	class Window
	{
	private:
		friend class GrayEngine;

		GLFWwindow* m_GlfwWindow;

	protected:
		Window(const char* title, int width, int height);

		~Window();

	public:
		/*
		* !@brief Sets the title of the window
		* 
		* @param[in] title - new title string
		*/
		GRAPI void SetTitle(const char* title);
		/*
		* !@brief Set the size of the window
		* 
		* @param[in] width - new width of the window
		* @param[in] height - new height of the window
		*/
		GRAPI void SetWindowSize(int width, int height);
		/*
		* !@brief Minimizes (iconifies) the window
		*/
		GRAPI void MinimizeWindow();
		/*
		* !@brief Get the size of the window
		* 
		* @return Vector containing integer width, height of the window
		*/
		GRAPI TIVec2 GetWindowSize() const;
		/*
		* !@brief Get the curent position of the cursor, relative to window
		* 
		* @return Vector containing x, y coordinate of cursor
		*/
		GRAPI TVec2 GetCursorPos() const;
		/*
		* !@brief Get aspect ration of the window
		* 
		* @return Double representing the ratio between width and height of the window
		*/
		GRAPI double GetAspectRatio() const;
		/*
		* !@brief Set cursor position on the window
		* 
		* @param[in] xpos - new x coordinate of the cursor
		* @param[in] ypos - new y coordinate of the cursor
		*/
		GRAPI void SetCursorPos(double xpos, double ypos);
		/*
		* !@brief Hides/Shows cursor when it hovers the window
		* 
		* @param[in] bShow - new state of the cursor
		*/
		GRAPI void ShowCursor(bool bShow);
		/*
		* !@brief Hides/Shows and locks/unlocks the cursor
		* 
		* @param[in] bDisabled - new state of the cursor
		*/
		GRAPI void DisableCursor(bool bDisabled);
		/*
		* !@brief Sets other GLFW attributes for the window
		* 
		* @param[in] attrib - GLFW attribute
		* @param[in] value - value of the attribute
		*/
		GRAPI void SetAttribute(int attrib, int value);
	};
}