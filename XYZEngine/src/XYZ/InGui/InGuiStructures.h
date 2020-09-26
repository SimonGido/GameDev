#pragma once
#include "XYZ/Core/KeyCodes.h"
#include "XYZ/Core/MouseCodes.h"
#include "XYZ/Gui/Font.h"
#include "XYZ/Renderer/Material.h"
#include "XYZ/Renderer/SubTexture2D.h"
#include "XYZ/Renderer/Framebuffer.h"
#include "XYZ/Renderer/InGuiRenderer.h"
#include "InGuiCamera.h"


namespace XYZ {
	enum InGuiWindowFlags
	{
		Moved		   = BIT(0),
		Collapsed	   = BIT(1),
		MenuEnabled	   = BIT(2),
		Modified	   = BIT(3),
		EventListener  = BIT(4),
		Hoovered	   = BIT(5),
		LeftResizing   = BIT(6),
		RightResizing  = BIT(7),
		TopResizing    = BIT(8),
		BottomResizing = BIT(9),
		Visible		   = BIT(10),
		AutoPosition   = BIT(11),
		Docked		   = BIT(12),
		Resized		   = BIT(13),
		LeftClicked	   = BIT(14),
		RightClicked   = BIT(15)
	};

	enum InGuiPerFrameFlags
	{
		LeftMouseButtonPressed   = BIT(0),
		RightMouseButtonPressed  = BIT(1),
		LeftMouseButtonReleased  = BIT(2),
		RightMouseButtonReleased = BIT(3),
		ClickHandled			 = BIT(4),
		ReleaseHandled			 = BIT(5)
	};


	enum InGuiNodeFlags
	{
		NodeMoved		= BIT(0),
		NodeModified	= BIT(1),
		NodeHoovered	= BIT(2)
	};


	struct InGuiRenderConfiguration
	{
		InGuiRenderConfiguration();

		Ref<Font>		  Font;
		Ref<Material>	  InMaterial;
		Ref<SubTexture2D> ButtonSubTexture;
		Ref<SubTexture2D> CheckboxSubTextureChecked;
		Ref<SubTexture2D> CheckboxSubTextureUnChecked;
		Ref<SubTexture2D> SliderSubTexture;
		Ref<SubTexture2D> SliderHandleSubTexture;
		Ref<SubTexture2D> WindowSubTexture;
		Ref<SubTexture2D> MinimizeButtonSubTexture;
		Ref<SubTexture2D> DownArrowButtonSubTexture;
		Ref<SubTexture2D> RightArrowButtonSubTexture;
		Ref<SubTexture2D> DockSpaceSubTexture;

	

		glm::vec4 DefaultColor = { 1.0f,1.0f,1.0f,1.0f };
		glm::vec4 HooverColor = { 0.4f, 1.8f, 1.7f, 1.0f };
		glm::vec4 SelectColor = { 0.8f,0.0f,0.2f,0.6f };
		glm::vec4 LineColor = { 0.4f,0.2f,0.5f,1.0f };

		static constexpr uint32_t DefaultTextureCount = 3;
		mutable uint32_t NumTexturesInUse = DefaultTextureCount;

		static constexpr uint32_t TextureID = 0;
		static constexpr uint32_t FontTextureID = 1;
		static constexpr uint32_t ColorPickerTextureID = 2;

		friend class InGui;
	};

	struct InGuiNodeWindow;
	struct InGuiDockNode;
	struct InGuiWindow
	{
		InGuiMesh Mesh;
		InGuiLineMesh LineMesh;
		std::string Name;

		glm::vec2 Position = { 0.0f,0.0f };
		glm::vec2 Size = { 0.0f,0.0f };

		float MinimalWidth = 0.0f;
		uint16_t Flags = 0;

		InGuiDockNode* DockNode = nullptr;
		InGuiNodeWindow* NodeWindow = nullptr;
		std::function<void(const glm::vec2&)> OnResizeCallback;
		static constexpr float PanelSize = 25.0f;
	};

	struct InGuiNode
	{
		glm::vec4 Color = { 1,1,1,1 };
		glm::vec2 Position = { 0,0 };
		glm::vec2 Size = { 0,0 };
		uint32_t ID = 0;
		uint8_t Flags = 0;
	};

	struct InGuiNodeWindow
	{
		InGuiNodeWindow();
		~InGuiNodeWindow();
		

		Ref<FrameBuffer> FBO;
		InGuiCamera InCamera;
		InGuiMesh Mesh;
		InGuiLineMesh LineMesh;

		InGuiNode* SelectedNode = nullptr;
		InGuiWindow* RenderWindow = nullptr;

		std::function<void(uint32_t, uint32_t)> OnConnectionCreated;
		std::function<void(uint32_t, uint32_t)> OnConnectionDestroyed;

		std::unordered_map<uint32_t, InGuiNode*> Nodes;

		glm::vec2 RelativeMousePosition = { 0,0 };
		glm::vec2 PopupPosition = { 0,0 };
		bool PopupEnabled = false;
	};

	struct InGuiPerFrameData
	{
		InGuiPerFrameData();
		void ResetWindowData();

		InGuiWindow* EventReceivingWindow;
		InGuiWindow* ModifiedWindow;
		InGuiWindow* CurrentWindow;
		InGuiNodeWindow* CurrentNodeWindow;
		InGuiNode* CurrentNode;

		glm::vec2 WindowSize;
		glm::vec2 ModifiedWindowMouseOffset;
		glm::vec2 WindowSpaceOffset;
		glm::vec2 MenuBarOffset;
		glm::vec2 PopupOffset;
		
		float LeftNodePinOffset;
		float RightNodePinOffset;

		glm::vec2 MousePosition;
		glm::vec2 SelectedPoint;

		float MaxHeightInRow;
		float MenuItemOffset;

		float ScrollOffset;
		int Code;
		int KeyCode;
		int Mode;
		bool CapslockEnabled;

		uint16_t Flags = 0;
		std::vector<TextureRendererIDPair> TexturePairs;
	};

	enum class SplitAxis
	{
		None,
		Vertical,
		Horizontal	
	};
	enum class DockPosition
	{
		None,
		Left,
		Right,
		Bottom,
		Top,
		Middle
	};

	struct InGuiDockNode
	{
		InGuiDockNode(const glm::vec2& pos, const glm::vec2& size, uint32_t id, InGuiDockNode* parent = nullptr)
			:
			Position(pos), Size(size), ID(id), Parent(parent)
		{
			Children[0] = nullptr;
			Children[1] = nullptr;
			VisibleWindow = nullptr;
		}

		glm::vec2 Position;
		glm::vec2 Size;

		InGuiDockNode* Parent;
		InGuiDockNode* Children[2];
		InGuiWindow* VisibleWindow;
		std::vector<InGuiWindow*> Windows;
		uint32_t ID;
		SplitAxis Split = SplitAxis::None;
		DockPosition Dock = DockPosition::None;
	};

	class InGuiDockSpace
	{
		friend class InGuiContext;
	public:
		InGuiDockSpace(InGuiDockNode* root);
		~InGuiDockSpace();

		void InsertWindow(InGuiWindow* window, const glm::vec2& mousePos);
		void RemoveWindow(InGuiWindow* window);

		void Begin();
		void End(const glm::vec2& mousePos, InGuiContext & context);

		bool OnRightMouseButtonPress(const glm::vec2& mousePos);
		bool OnLeftMouseButtonPress();
		bool OnRightMouseButtonRelease(InGuiWindow* window, const glm::vec2& mousePos);
		bool OnWindowResize(const glm::vec2& winSize);

	private:
		void resize(const glm::vec2& mousePos);
		void adjustChildrenProps(InGuiDockNode* node);
		void detectResize(InGuiDockNode* node, const glm::vec2& mousePos);
		void insertWindow(InGuiWindow* window, const glm::vec2& mousePos, InGuiDockNode* node);
		void destroy(InGuiDockNode** node);
		void rescale(const glm::vec2& scale, InGuiDockNode* node);
		void splitNodeProportional(InGuiDockNode* node, SplitAxis axis, const glm::vec2& firstSize);
		void unsplitNode(InGuiDockNode* node);
		void update(InGuiDockNode* node);
		void showNodeWindows(InGuiDockNode* node, const glm::vec2& mousePos,InGuiPerFrameData& frameData, const InGuiRenderConfiguration& renderConfig);
		void showNode(InGuiDockNode* node, const glm::vec2& mousePos, const InGuiRenderConfiguration& renderConfig);
		DockPosition collideWithMarker(InGuiDockNode* node, const glm::vec2& mousePos);

	private:
		InGuiDockNode* m_Root;

		InGuiDockNode* m_ResizedNode = nullptr;

		uint32_t m_NextNodeID = 0;
		std::queue<uint32_t> m_FreeIDs;

		bool m_DockSpaceVisible = false;
		static constexpr glm::vec2 sc_QuadSize = { 50,50 };

		friend class InGuiContext;
		friend class InGui;
	};


	


	using InGuiWindowMap = std::unordered_map<std::string, InGuiWindow*>;
	using InGuiNodeWindowMap = std::unordered_map<std::string, InGuiNodeWindow*>;
	using InGuiEventListeners = std::vector<InGuiWindow*>;

	struct InGuiRenderQueue
	{
		std::vector<InGuiMesh*> InGuiMeshes;
		std::vector<InGuiLineMesh*> InGuiLineMeshes;
	};

	struct InGuiContext
	{
		InGuiRenderConfiguration RenderConfiguration;
		InGuiPerFrameData PerFrameData;
		InGuiWindowMap Windows;
		InGuiNodeWindowMap NodeWindows;
		InGuiRenderQueue RenderQueue;
		InGuiDockSpace* DockSpace = nullptr;
	};
}