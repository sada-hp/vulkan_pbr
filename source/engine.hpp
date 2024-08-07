#pragma once

#include "core.hpp"
#include "glm/glm.hpp"
#include "entt/entt.hpp"
#include "window.hpp"
#include "event_listener.hpp"
#include "math.hpp"
#include "renderer.hpp"

#ifdef INCLUDE_GUI
#include "imgui/imgui.h"
#include "imgui/imgui_impl_vulkan.h"
#include "imgui/imgui_impl_glfw.h"
#endif

namespace GR
{
	using Entity = entt::entity;

	class GrayEnigne;
	/*
	* !@brief Main engine class
	*/
	class GrayEngine
	{
	private:
		TVector<void(*)(GrayEngine*, double)> m_Inputs;
		entt::registry m_Registry;
		EventListener* m_Listener;
		VulkanBase* m_Renderer;
		Window* m_Window;
		double m_Delta = 0.0;
		double m_Time = 0;

		struct EngineContext
		{
			EventListener* listener;
			VulkanBase* renderer;
			GrayEngine* engine;
		} m_Context;

		// !@brief Defined in glfw_callbacks.cpp
		static void glfw_key_press(GLFWwindow* window, int, int, int, int);

		// !@brief Defined in glfw_callbacks.cpp
		static void glfw_mouse_press(GLFWwindow* window, int, int, int);

		// !@brief Defined in glfw_callbacks.cpp
		static void glfw_mouse_move(GLFWwindow* window, double, double);

		// !@brief Defined in glfw_callbacks.cpp
		static void glfw_resize(GLFWwindow* window, int, int);

		// !@brief Defined in glfw_callbacks.cpp
		static void glfw_scroll(GLFWwindow* window, double, double);

#ifdef INCLUDE_GUI
		ImGuiContext* m_GuiContext;
#endif
	public:
#ifdef INCLUDE_GUI
		/*
		* !@brief Get the associated ImGui context
		* 
		* @return ImGui context associated with engine context
		*/
		GRAPI ImGuiContext* GetGUIContext()
		{
			return m_GuiContext;
		}
#endif
		/*
		* !@brief User defined pointer, renderer itself does not reference it
		*/
		void* userPointer1 = nullptr;
		/*
		* !@brief User defined pointer, renderer itself does not reference it
		*/
		void* userPointer2 = nullptr;
		/*
		* !@brief User defined pointer, renderer itself does not reference it
		*/
		void* userPointer3 = nullptr;
		/*
		* !@brief User defined pointer, renderer itself does not reference it
		*/
		void* userPointer4 = nullptr;

		GRAPI GrayEngine(int argc, char** argv, ApplicationSettings& Settings);

		GRAPI ~GrayEngine();
		/*
		* !@breif Begins the game loop that lasts until program termination or window closing
		*/
		GRAPI void StartGameLoop();
		/*
		* !@brief These functions will be called once every frame
		*
		* @param[in] Engine contex from which the call was made
		* @param[in] Time elapsed since last frame
		*
		*/
		GRAPI void AddInputFunction(void(*function)(GrayEngine*, double));
		/*
		* !@brief Get time in seconds since application launch
		* 
		* @return Time in seconds
		*/
		GRAPI double GetTime() const;
		/*
		* !@brief Get window abstraction of the context
		* 
		* @return Context's window
		*/
		GRAPI Window& GetWindow() const;
		/*
		* !@brief Load mesh from file
		* 
		* @param[in] MeshPath - local path to the mesh file
		* 
		* @return New entity handle
		*/
		GRAPI Entity AddMesh(const std::string& MeshPath) const;
		/*
		* !@brief Generate mesh from shape descriptor
		* 
		* @param[in] Descriptor - shape descriptor from GRShape
		* 
		* @return New entity handle
		*/
		GRAPI Entity AddShape(const GRShape::Shape& Descriptor) const;
		/*
		* !@brief Get active viewport camera
		* 
		* @return Camera descriptor
		*/
		GRAPI Camera& GetMainCamera() const;
		/*
		* !@brief Get associated engine event listener
		* 
		* @return Context's event listener
		*/
		GRAPI EventListener& GetEventListener();
		/*
		* !@brief Unsafe! Get raw renderer reference
		*/
		GRAPI VulkanBase& GetRenderer();
		/*
		* !@brief Emplace or update entity's component of <Type>
		* 
		* @param[in] ent - entity to apply component to
		* @param[in] args - arguments to construct the component
		* 
		* @return Created component
		*/
		template<typename Type, typename... Args>
		GRAPI decltype(auto) EmplaceComponent(Entity ent, Args&& ...args)
		{
			return m_Registry.emplace_or_replace<Type>(ent, args...);
		}
		/*
		* !@brief Get <Type> component of an entity
		* 
		* @param[in] ent - entity id
		* 
		* @return Component of <Type>
		*/
		template<typename... Type>
		GRAPI decltype(auto) GetComponent(const Entity ent)
		{
			return m_Registry.get<Type...>(ent);
		}
		/*
		* !@brief Load an image from file and binds it to the specified shader location
		* 
		* @param[in] Resource - component, representing shader binding
		* @param[in] path - local path to image file
		* @param[in] type - helper on how to interpret image format
		*/
		GRAPI void BindImage(GRComponents::Resource<Image>& Resource, const std::string& path, EImageType type);
		/*
		* !@brief Erase all entities from the scene
		*/
		GRAPI void ClearEntities();
	};
};