#include "pch.hpp"
#include "renderer.hpp"

std::string exec_path;

int main(int argc, char** argv)
{
	if (argc > 0)
	{
		exec_path = argv[0];
		exec_path = exec_path.substr(0, exec_path.find_last_of('\\') + 1);
	}

	GLFWwindow* pWindow = nullptr;
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	pWindow = glfwCreateWindow(1024, 720, "Vulkan PBR", nullptr, nullptr);

	assert(glfwVulkanSupported());

	VulkanBase render(pWindow);

	glfwSetWindowUserPointer(pWindow, &render);
	glfwSetWindowSizeCallback(pWindow, [](GLFWwindow* window, int width, int height)
		{
			VulkanBase* rndr = static_cast<VulkanBase*>(glfwGetWindowUserPointer(window));
			rndr->HandleResize();
		});

	glfwSetKeyCallback(pWindow, [](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			VulkanBase* rndr = static_cast<VulkanBase*>(glfwGetWindowUserPointer(window));
			TVec3 off = TVec3(0.0);
			TVec3 rot = TVec3(0.0);

			switch (key)
			{
			case GLFW_KEY_RIGHT:
				rot.y += 10.f;
				break;
			case GLFW_KEY_LEFT:
				rot.y -= 10.f;
				break;
			case GLFW_KEY_UP:
				rot.x += 10.f;
				break;
			case GLFW_KEY_DOWN:
				rot.x -= 10.f;
				break;
			case GLFW_KEY_W:
				off.z += 10.f;
				break;
			case GLFW_KEY_S:
				off.z -= 10.f;
				break;
			case GLFW_KEY_A:
				off.x += 10.f;
				break;
			case GLFW_KEY_D:
				off.x -= 10.f;
				break;
			default:
				break;
			}

			rndr->camera.Rotate(rot);
			rndr->camera.Translate(off);
		});

	render.camera.SetPosition({ render.camera .GetPosition().x, render.camera.GetPosition().y, -200.0 });

	entt::entity sword = render.AddGraphicsObject("content\\sword.fbx", "");
	WorldMatrix& wld = render.GetComponent<WorldMatrix>(sword);

	double delta = 0;
	double total_time = 0;
	unsigned long long frame = 0;
	while (!glfwWindowShouldClose(pWindow))
	{
		glfwPollEvents();
		render.Step(delta);

		double frame_time = glfwGetTime();
		delta = frame_time - total_time;
		total_time = frame_time;
		frame++;

		wld.SetOffset(glm::vec3(0.0, glm::sin(frame_time) * 5.0, 0.0));

		if (frame % 10 == 0) {
			glfwSetWindowTitle(pWindow, ("Vulkan PBR " + std::to_string(int(1.0 / delta))).c_str());
		}

		glfwSwapBuffers(pWindow);
	}

	glfwDestroyWindow(pWindow);
	glfwTerminate();

	return 0;
}