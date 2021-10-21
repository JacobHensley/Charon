#pragma once
#include <string>

namespace Charon {

	class Layer
	{
	public:
		Layer(const std::string& name);
		virtual ~Layer();

	public:
		virtual void OnAttach() {}
		virtual void OnDetach() {}

		virtual void OnUpdate() {}
		virtual void OnRender() {}

		virtual void OnImGUIRender() {}

	protected:
		const std::string m_Name;
	};

}