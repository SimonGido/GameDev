#include "GraphPanel.h"

namespace XYZ {
	GraphPanel::GraphPanel()
	{
		InGui::Begin("Graph", { 0,0 }, { 100,100 });
		InGui::End();
	}
	void GraphPanel::OnInGuiRender(float dt)
	{
		if (InGui::NodeWindow("Graph", { 0,0 }, { 100,100 }, dt))
		{
			if (m_Layout)
				m_Layout->OnInGuiRender();
		}
		InGui::NodeWindowEnd();
	}
}