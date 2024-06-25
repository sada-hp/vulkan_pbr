#include "pch.hpp"
#include "engine.hpp"
#include "shapes.hpp"

std::string exec_path = "";

namespace GR
{
	GrayEngine::GrayEngine(const std::string& ExecutablePath, ApplicationSettings& Settings)
	{
		assert(ExecutablePath != "");

		exec_path = ExecutablePath;

#ifdef INCLUDE_GUI
		GuiContext = ImGui::CreateContext();
		ImGui::SetCurrentContext(GuiContext);
#endif

		listener = new EventListener();
		window = new Window(Settings.ApplicationName.c_str(), Settings.WindowExtents.x, Settings.WindowExtents.y);
		renderer = new VulkanBase(window->glfwWindow, registry);

		context = { listener , renderer, this };

		glfwSetWindowUserPointer(window->glfwWindow, &context);
		glfwSetWindowSizeCallback(window->glfwWindow, glfw_resize);
		glfwSetKeyCallback(window->glfwWindow, glfw_key_press);
		glfwSetMouseButtonCallback(window->glfwWindow, glfw_mouse_press);
		glfwSetCursorPosCallback(window->glfwWindow, glfw_mouse_move);
		glfwSetScrollCallback(window->glfwWindow, glfw_scroll);
	}

	GrayEngine::~GrayEngine()
	{
		delete listener;
		delete renderer;
		delete window;
		registry.clear();
	}

	void GrayEngine::StartGameLoop()
	{
		while (!glfwWindowShouldClose(window->glfwWindow))
		{
			glfwPollEvents();

			double frame_time = GetTime();
			delta = frame_time - total_time;
			total_time = frame_time;

#ifdef INCLUDE_GUI
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
#endif

			std::for_each(inputs.begin(), inputs.end(), [&](void(*func)(GrayEngine*, double))
			{
				func(this, delta);
			});

#ifdef INCLUDE_GUI
			ImGui::Render();
#endif
			renderer->Step(delta);

			//glfwSwapBuffers(window->glfwWindow);
		}
	}

	void GrayEngine::AddInputFunction(void(*function)(GrayEngine*, double))
	{
		inputs.push_back(function);
	}

	double GrayEngine::GetTime() const
	{
		return glfwGetTime();
	}

	Entity GrayEngine::AddMesh(const std::string& MeshPath) const
	{
		return renderer->AddMesh(MeshPath);
	}

	Entity GrayEngine::AddShape(const GRShape::Shape& Descriptor) const
	{
		return renderer->AddShape(Descriptor);
	}

	void GrayEngine::BindImage(GRComponents::Resource<Image>& Resource, const std::string& path, EImageType type)
	{
		VkFormat format;

		switch (type)
		{
		case GR::EImageType::RGBA_UNORM:
			format = VK_FORMAT_R8G8B8A8_UNORM;
			break;
		case GR::EImageType::RGBA_FLOAT:
			format = VK_FORMAT_R32G32B32A32_SFLOAT;
			break;
		default:
			format = VK_FORMAT_R8G8B8A8_SRGB;
			break;
		}

		Resource.Set(renderer->LoadImage(path, format));
	}

	Window& GrayEngine::GetWindow() const
	{
		return *window;
	}

	Camera& GrayEngine::GetMainCamera() const
	{
		return renderer->camera;
	}

	EventListener& GrayEngine::GetEventListener()
	{
		return *listener;
	}

	VulkanBase& GrayEngine::GetRenderer()
	{
		return *renderer;
	}

	void GrayEngine::ClearEntities()
	{
		renderer->Wait();
		registry.clear();
	}
};