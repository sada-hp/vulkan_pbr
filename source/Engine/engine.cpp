#include "pch.hpp"
#include "engine.hpp"
#include "shapes.hpp"

std::string exec_path = "";

namespace GR
{
	GrayEngine::GrayEngine(int argc, char** argv, ApplicationSettings& Settings)
	{
		assert(argc > 0);

		exec_path = argv[0];
		exec_path = exec_path.substr(0, exec_path.find_last_of('\\') + 1);

#ifdef INCLUDE_GUI
		m_GuiContext = ImGui::CreateContext();
		ImGui::SetCurrentContext(m_GuiContext);
#endif

		m_Window._init(m_Registry, Settings);
		m_Context = { &m_Listener , &GetRenderer(), this };

		glfwSetWindowUserPointer(m_Window._wptr(), &m_Context);
		glfwSetWindowSizeCallback(m_Window._wptr(), glfw_resize);
		glfwSetKeyCallback(m_Window._wptr(), glfw_key_press);
		glfwSetMouseButtonCallback(m_Window._wptr(), glfw_mouse_press);
		glfwSetCursorPosCallback(m_Window._wptr(), glfw_mouse_move);
		glfwSetScrollCallback(m_Window._wptr(), glfw_scroll);
	}

	GrayEngine::~GrayEngine()
	{
		m_Window._destroy();
		m_Registry.clear();
	}

	void GrayEngine::StartGameLoop()
	{
		while (!glfwWindowShouldClose(m_Window._wptr()))
		{
			glfwPollEvents();

			double frame_time = GetTime();
			m_Delta = frame_time - m_Time;
			m_Time = frame_time;

#ifdef INCLUDE_GUI
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
#endif

			std::for_each(m_Inputs.begin(), m_Inputs.end(), [&](void(*func)(GrayEngine*, double))
			{
				func(this, m_Delta);
			});

#ifdef INCLUDE_GUI
			ImGui::Render();
#endif
			GetRenderer()._step(m_Delta);
		}
	}

	void GrayEngine::AddInputFunction(void(*function)(GrayEngine*, double))
	{
		m_Inputs.push_back(function);
	}

	double GrayEngine::GetTime() const
	{
		return glfwGetTime();
	}

	Entity GrayEngine::AddMesh(const std::string& MeshPath)
	{
		return GetRenderer().AddMesh(MeshPath);
	}

	Entity GrayEngine::AddShape(const GRShape::Shape& Descriptor)
	{
		return GetRenderer().AddShape(Descriptor);
	}

	void GrayEngine::BindImage(GRComponents::Resource<Texture>& Resource, const std::string& path, EImageType type)
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

		Resource.Set(GetRenderer()._loadImage(path, format));
	}

	Window& GrayEngine::GetWindow()
	{
		return m_Window;
	}

	Camera& GrayEngine::GetMainCamera()
	{
		return GetRenderer().m_Camera;
	}

	EventListener& GrayEngine::GetEventListener()
	{
		return m_Listener;
	}

	VulkanBase& GrayEngine::GetRenderer()
	{
		return GetWindow().GetRenderer();
	}

	void GrayEngine::ClearEntities()
	{
		GetRenderer().Wait();
		m_Registry.clear();
	}
};