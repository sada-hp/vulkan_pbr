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

	struct CameraCustomization
	{
		glm::vec3 camera_pyr = glm::vec3(0.f);
		float camera_zoom = -200.f;
	} CC;

	render.userPointer1 = &CC;

	glfwSetKeyCallback(pWindow, [](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			VulkanBase* rndr = static_cast<VulkanBase*>(glfwGetWindowUserPointer(window));
			CameraCustomization& cam_customs = *static_cast<CameraCustomization*>(rndr->userPointer1);

			switch (key)
			{
			case GLFW_KEY_RIGHT:
				cam_customs.camera_pyr.y -= 10.f;
				break;
			case GLFW_KEY_LEFT:
				cam_customs.camera_pyr.y += 10.f;
				break;
			case GLFW_KEY_UP:
				cam_customs.camera_pyr.x += 10.f;
				break;
			case GLFW_KEY_DOWN:
				cam_customs.camera_pyr.x -= 10.f;
				break;
			default:
				break;
			}

			glm::quat q = glm::quat_cast(glm::mat4(1.f));
			q = q * glm::angleAxis(glm::radians(cam_customs.camera_pyr.x), glm::vec3(1, 0, 0));
			q = q * glm::angleAxis(glm::radians(cam_customs.camera_pyr.y), glm::vec3(0, 1, 0));
			q = q * glm::angleAxis(glm::radians(cam_customs.camera_pyr.z), glm::vec3(0, 0, 1));
			rndr->camera.SetRotation(q);
			rndr->camera.SetPosition(glm::vec3(0.f, 0.f, cam_customs.camera_zoom) * q);
		});

	glfwSetScrollCallback(pWindow, [](GLFWwindow* window, double xoffset, double yoffset)
		{
			VulkanBase* rndr = static_cast<VulkanBase*>(glfwGetWindowUserPointer(window));
			CameraCustomization& cam_customs = *static_cast<CameraCustomization*>(rndr->userPointer1);

			cam_customs.camera_zoom += 7.5f * yoffset;
			rndr->camera.SetPosition(glm::vec3(0.f, 0.f, cam_customs.camera_zoom)* rndr->camera.GetOrientation());
		});

	render.camera.SetPosition({ render.camera .GetPosition().x, render.camera.GetPosition().y, CC.camera_zoom });

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

		if (frame % 10 == 0) {
			glfwSetWindowTitle(pWindow, ("Vulkan PBR " + std::to_string(int(1.0 / delta))).c_str());
		}


		glfwSwapBuffers(pWindow);
	}

	glfwDestroyWindow(pWindow);
	glfwTerminate();

	return 0;
}