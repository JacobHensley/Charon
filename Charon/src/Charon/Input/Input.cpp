#include "pch.h"
#include "Input.h"
#include "Charon/Core/Application.h"
#include <GLFW/glfw3.h>

namespace Charon {

	Input* Input::s_Instance = new Input();

	bool Input::IsKeyPressed(int keycode)
	{
		GLFWwindow* window = Application::GetApp().GetWindow()->GetWindowHandle();
		return glfwGetKey(window, keycode);
	}

	bool Input::IsMouseButtonPressed(int button)
	{
		GLFWwindow* window = Application::GetApp().GetWindow()->GetWindowHandle();
		return glfwGetMouseButton(window, button);
	}

	glm::vec2 Input::GetMousePos()
	{
		GLFWwindow* window = Application::GetApp().GetWindow()->GetWindowHandle();
		double x, y;
		glfwGetCursorPos(window, &x, &y);

		return glm::vec2(x, y);
	}

	float Input::GetMouseX()
	{  
		return GetMousePos().x;
	}

	float Input::GetMouseY()
	{
		return GetMousePos().y;
	}

}