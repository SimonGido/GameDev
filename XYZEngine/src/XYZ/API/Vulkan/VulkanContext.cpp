#include "stdafx.h"
#include "VulkanContext.h"

#include "VulkanValidation.h"
#include "VulkanAllocator.h"

#include "XYZ/Core/Application.h"

namespace XYZ {
	#ifdef XYZ_DEBUG
	static bool s_Validation = true;
	#else
	static bool s_Validation = false; // Let's leave this on for now...
	#endif


	static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
	{
		(void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix; // Unused arguments
		XYZ_CORE_WARN("VulkanDebugCallback:\n  Object Type: {0}\n  Message: {1}", objectType, pMessage);

		if (strstr(pMessage, "CoreValidation-DrawState-InvalidImageLayout"))
			XYZ_ASSERT(false, "");

		return VK_FALSE;
	}


	VulkanContext::VulkanContext()
		:
		m_DebugReportCallback(VK_NULL_HANDLE),
		m_PipelineCache(VK_NULL_HANDLE),
		m_WindowHandle(nullptr)
	{
	}
	VulkanContext::~VulkanContext()
	{
		
	}
	void VulkanContext::Init(GLFWwindow* window)
	{
		m_WindowHandle = window;
		// Application info
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pApplicationName = "XYZEngine";
		appInfo.pEngineName = "XYZEngine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_3;
	

		// Extensions and validation layers
		#define VK_KHR_WIN32_SURFACE_EXTENSION_NAME "VK_KHR_win32_surface"

		std::vector<const char*> instanceExtensions;
		VulkanValidation::AddExtension(VK_KHR_SURFACE_EXTENSION_NAME, instanceExtensions);
		VulkanValidation::AddExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME, instanceExtensions);
		VulkanValidation::AddExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, instanceExtensions);
	
		if (s_Validation)
		{
			VulkanValidation::AddExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, instanceExtensions);
			VulkanValidation::AddExtension(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, instanceExtensions);
		}

		const VkValidationFeatureEnableEXT enables[] = { VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT };
		VkValidationFeaturesEXT features = {};
		features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
		features.enabledValidationFeatureCount = 1;
		features.pEnabledValidationFeatures = enables;

		VkInstanceCreateInfo instanceCreateInfo = {};
		instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceCreateInfo.pNext = nullptr;//&features;
		instanceCreateInfo.pApplicationInfo = &appInfo;
		instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
		instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
		
		std::vector<const char*> validationLayers;
		if (s_Validation)
		{
			if (VulkanValidation::AddValidationLayer("VK_LAYER_KHRONOS_validation", validationLayers))
			{
				instanceCreateInfo.ppEnabledLayerNames = validationLayers.data();
				instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			}
			else
			{
				XYZ_CORE_ERROR("Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled");
			}
		}

		// Instance creation
		VK_CHECK_RESULT(vkCreateInstance(&instanceCreateInfo, nullptr, &s_VulkanInstance));
		setupDebugCallback();
		setupDevice();
		m_SwapChain.Init(s_VulkanInstance, m_Device);
		m_SwapChain.InitSurface(m_WindowHandle);
		VulkanAllocator::Init(m_Device);
	}

	void VulkanContext::Shutdown()
	{
		VK_CHECK_RESULT(vkDeviceWaitIdle(m_Device->GetVulkanDevice()));
		VulkanAllocator::Shutdown();
		m_SwapChain.Destroy();
		m_Device->Destroy();
		if (s_Validation)
		{
			const auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(s_VulkanInstance, "vkDestroyDebugReportCallbackEXT");
			vkDestroyDebugReportCallbackEXT(s_VulkanInstance, m_DebugReportCallback, nullptr);
		}
		vkDestroyInstance(s_VulkanInstance, nullptr);
		s_VulkanInstance = nullptr;
	}
	
	void VulkanContext::CreateSwapChain(uint32_t* width, uint32_t* height, bool vSync)
	{
		m_SwapChain.Create(width, height, vSync);
	}
	void VulkanContext::SwapBuffers()
	{
		m_SwapChain.Present();		
	}
	void VulkanContext::BeginFrame()
	{
		m_SwapChain.BeginFrame();
	}
	void VulkanContext::OnResize(uint32_t width, uint32_t height)
	{
		m_SwapChain.OnResize(width, height);
	}
	Ref<RenderCommandBuffer> VulkanContext::GetRenderCommandBuffer()
	{
		return m_SwapChain.GetRenderCommandBuffer();
	}
	
	Ref<VulkanContext> VulkanContext::Get()
	{
		return Ref<VulkanContext>(Renderer::GetAPIContext());
	}
	void VulkanContext::setupDebugCallback()
	{
		if (s_Validation)
		{
			const auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(s_VulkanInstance, "vkCreateDebugReportCallbackEXT");
			XYZ_ASSERT(vkCreateDebugReportCallbackEXT != NULL, "");
			VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
			debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
			debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
			debug_report_ci.pfnCallback = VulkanDebugReportCallback;
			debug_report_ci.pUserData = NULL;
			VK_CHECK_RESULT(vkCreateDebugReportCallbackEXT(s_VulkanInstance, &debug_report_ci, nullptr, &m_DebugReportCallback));
		}
	}
	void VulkanContext::setupDevice()
	{
		VkPhysicalDeviceFeatures2 enabledFeatures;
		memset(&enabledFeatures, 0, sizeof(VkPhysicalDeviceFeatures2));
		enabledFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		enabledFeatures.features.samplerAnisotropy = true;
		enabledFeatures.features.wideLines = true;
		enabledFeatures.features.fillModeNonSolid = true;
		enabledFeatures.features.pipelineStatisticsQuery = true;
		enabledFeatures.features.inheritedQueries = true;
		enabledFeatures.features.shaderStorageImageMultisample = true; // Required for VR
		enabledFeatures.features.fragmentStoresAndAtomics = true;


		m_Device = Ref<VulkanDevice>::Create(enabledFeatures);
	}
}