#pragma once
#include "Window.h"
#include "LayerStack.h"
#include "ThreadPool.h"

#include "XYZ/ImGui/ImGuiLayer.h"

namespace XYZ {
	class Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();
		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlayer);
		void PopLayer(Layer* layer);
		void Stop();


		bool OnEvent(Event& event);

		Window&			   GetWindow()				 { return *m_Window; }
		ThreadPool&		   GetThreadPool()			 { return m_ThreadPool; }
		ImGuiLayer*		   GetImGuiLayer()			 { return m_ImGuiLayer; }
		const std::string& GetApplicationDir() const { return m_ApplicationDir; }

		inline static Application& Get() { return *s_Application; }

		static Application* CreateApplication();

	private:
		bool onWindowResized(WindowResizeEvent& event);
		bool onWindowClosed(WindowCloseEvent& event);
		void updateTimestep();
		void onImGuiRender();

	private:
		LayerStack m_LayerStack;
		ImGuiLayer* m_ImGuiLayer;
		std::unique_ptr<Window> m_Window;


		bool       m_Running;
		float      m_LastFrameTime;
		Timestep   m_Timestep;
		ThreadPool m_ThreadPool;

		std::string m_ApplicationDir;

		static Application* s_Application;
	};

}