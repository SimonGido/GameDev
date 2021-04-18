#pragma once
#include "Window.h"
#include "LayerStack.h"

#include "XYZ/IG/IGLayer.h"

/**	@class Application
 *  Singleton class.
 *	Represents main application, stores and updates the Layers and the Window.
 *
 */

namespace XYZ {
	class Application
	{
	public:
		/**
		* Constructor
		*/
		Application();

		/**
		* Destructor
		* virtual destructor
		*/
		virtual ~Application();

		/**
		* Main loop of application
		*/
		void Run();

		/**
		* Push layer to the vector of layers, calls OnAttach function for the pushed layer
		* @param layer pointer to the Layer
		*/
		void PushLayer(Layer* layer);

		/**
		* Push layer to the back of the vector of layers, calls OnAttach function for the 
		* pushed layer
		* @param layer pointer to the Layer
		*/
		void PushOverlay(Layer* overlayer);

		/**
		* Finds layer in linear time, calls OnDetach function for the layer and erases it from
		* the container
		* @param layer pointer to the Layer
		*/
		void PopLayer(Layer* layer);


		bool OnEvent(Event& event);

		/**
		* Return handler of the main Window
		* @param[out] reference to the Window
		*/
		Window& GetWindow() { return *m_Window; }
		const std::string& GetApplicationDir() const { return m_ApplicationDir; }

		void Stop();
		/**
		* Return handler of the Application
		* @param[out] reference to the Application
		*/
		inline static Application& Get() { return *s_Application; }

		/**
		* Create the application and returns the pointer
		* @param[out] pointer to the Application
		*/
		static Application* CreateApplication();

	private:
		bool onWindowResized(WindowResizeEvent& event);
		bool onWindowClosed(WindowCloseEvent& event);

	private:
		LayerStack m_LayerStack;
		IGLayer* m_IGLayer;

		std::unique_ptr<Window> m_Window;

		bool m_Running;
		float m_LastFrameTime = 0.0f;

		std::string m_ApplicationDir;

		static Application* s_Application;
	};

}