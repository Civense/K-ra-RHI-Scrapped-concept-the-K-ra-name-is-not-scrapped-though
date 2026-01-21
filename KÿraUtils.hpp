#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#include <Windows.h>
#include <guiddef.h> 
#include <objbase.h>
using namespace Microsoft::WRL;
#endif
#include <vulkan/vulkan.h>
#define UNDEFINED_INTEGER -1
#define VULKAN_MAJOR_VERSION 1
#define VULKAN_MINOR_VERSION 4


#include <iostream>
/*
class KRSwapChain {


#ifdef WIN32


#endif
	VkSurfaceKHR VkSurface;
};
*/
enum KRMode {
	Vulkan,
	DirectX,
	PlatformUndefined
};
class KRFactory;
class KRTap {
	friend class KRFactory;
#ifdef _WIN32
	struct DXTap_ {
		ComPtr<ID3D12Debug4> DebugController4;
		inline bool Debugger1() {
			if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&DebugController4)))) {
				return false;
			}
			DebugController4->EnableDebugLayer();
			DebugController4->SetEnableGPUBasedValidation(TRUE);
			return true;
		}
	} DXTap;
	struct VKTap_ {
		const char* ValidationLayers[1] = { "VK_LAYER_KHRONOS_validation" };
	} VKTap;
#endif
};
struct KR_WINDOW_DESCRIPTION {
	bool WindowedMode;
	const char* WindowTitle;
};
class KRWindow {								//IMPLEMENT CRITICAL: Make sure to finish adding suppourt to get the current monitor output, this should be a ported feature to linux as well(DXGI on windows is done, Vulkan on windows is not done, neither Wayland or X11 are done yet for Linux)
	friend class KRFactory;			//IMPLEMENT CRITICAL: Make sure to implement RTV resizing and RTVs in the first place, as well as the Vulkan equivelent
	bool WindowPositionChanged = false;
	bool FullscreenActive = false;
	bool FullscreenToggle = false;
	bool DisplayChange = false;
	bool SizeChange = false;
	int WindowWidth = 0;
	int WindowHeight = 0;
	int WindowX = 0;
	int WindowY = 0;
#ifdef _WIN32
	ComPtr<IDXGISwapChain4> SwapChain;
	ComPtr<IDXGIOutput> Output;
	ComPtr<IDXGIOutput6> ActiveOutput;
	HMONITOR Monitor;
	HWND hwnd = nullptr;
	MSG WindowMessage = {};
	MONITORINFO Info = {};
	inline DXGI_FORMAT DXGIPollMonitorColorFormat() {
		DXGI_OUTPUT_DESC1 Description1 = {};
		ActiveOutput->GetDesc1(&Description1);
		DXGI_COLOR_SPACE_TYPE ColorSpace = {};
		ColorSpace = Description1.ColorSpace;
		if (ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709) {
			return DXGI_FORMAT_R8G8B8A8_UNORM;
		}
		return DXGI_FORMAT_R10G10B10A2_UNORM;
	}
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		KRWindow* LocalKRWindow = reinterpret_cast<KRWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		switch (msg) {
		case WM_SIZE:
			LocalKRWindow->SizeChange = true;
			break;
		case WM_DISPLAYCHANGE:
			LocalKRWindow->DisplayChange = true;
			break;
		case WM_KEYDOWN:
			if (wParam == VK_F11) {
				LocalKRWindow->FullscreenToggle = true;
			}
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_WINDOWPOSCHANGED:
			LocalKRWindow->WindowPositionChanged = true;
			break;
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
		}
		return 0;
	}
#endif
	friend class KRFactory;
};
struct KR_SWAP_CHAIN_DESCRIPTION {
	int InitialWindowHeight = 0;
	int InitialWindowWidth = 0;
};
struct KR_BUFFER_DESCRIPTION {
	int BufferCount;
};
class KRFactory {
	friend class KRWindow;
	KRTap* LocalKRTap = nullptr;
	KRWindow* LocalKRWindow = nullptr;
	KR_BUFFER_DESCRIPTION LocalBufferDescription;
	bool MasterRenderStateChangeFlag = false;
#ifdef _WIN32
	GUID KRGUID;												//Rendering Tag
	KRMode ActiveAPIMode = KRMode::DirectX;
	wchar_t GPUName[128] = L"";
	DXGI_SWAP_CHAIN_DESC1 SwapChainDescription = {};
	ComPtr<ID3D12Device14> Device;
	ComPtr<IDXGIAdapter4> Adapter;
	ComPtr<IDXGIFactory7> Factory;
	ComPtr<ID3D12CommandQueue> CommandQueue;
	//Move swapchain creation outside of the main factory, and add a KRSwapchain
	const bool LOCKDX12 = false;
	USHORT NodeIndex = 0;
#elif defined(__linux__)
	KRMode ActiveAPIMode = KRMode::Vulkan;
	const bool LOCKDX12 = true;
#else
	KRMode ActiveAPIMode = KRMode::PlatformUndefined;
	const bool LOCKDX12 = true;
#endif
	VkInstance VkInstance;
	VkPhysicalDevice* VkDeviceList;								//Add a Linux path for allocating the device list
	VkDevice VkLogicalDevice = VK_NULL_HANDLE;
	VkPhysicalDevice VkDevice = VK_NULL_HANDLE;
	VkPhysicalDeviceFeatures VkFeatures;
	VkQueue VkGraphicsQueue;
	VkCommandPool CommandPool = VK_NULL_HANDLE;
	VkCommandBuffer GraphicsCommandBuffer;
	VkPhysicalDeviceFeatures DeviceFeatures = {};
	bool SuppourtedAPIVersion = false;



	inline bool ProcessVkResultRequest(VkResult _Result) {
		if (_Result == VK_SUCCESS) {
			return true;
		}
		else {
#ifdef _DEBUG
			printf("ERROR_CODE:0x%X\n", _Result);
#endif
		}
		return false;
	}
	void CleanVKInstance() {
#ifdef WIN32
		if (VkDeviceList) VirtualFree(VkDeviceList, 0, MEM_RELEASE), VkDeviceList = nullptr;
#endif
		vkDeviceWaitIdle(VkLogicalDevice);
		//Pre-Instance Destruction
			//Pre-Device Destruction
		if (CommandPool != VK_NULL_HANDLE) vkDestroyCommandPool(VkLogicalDevice, CommandPool, nullptr);
		//Device Destruction
		if (CommandPool != VK_NULL_HANDLE && VkLogicalDevice != VK_NULL_HANDLE) vkDestroyDevice(VkLogicalDevice, nullptr);
		//Instance Destruction
		if (VkInstance) vkDestroyInstance(VkInstance, nullptr);
	}
	inline bool AllocateVkDeviceEnumerationContext(uint32_t* DeviceCount) {
		if (!ProcessVkResultRequest(vkEnumeratePhysicalDevices(VkInstance, DeviceCount, nullptr))) {
			CleanVKInstance();
			return false;
		}
		PROCESSOR_NUMBER ProcessorNumerator;
		GetCurrentProcessorNumberEx(&ProcessorNumerator);
		GetNumaProcessorNodeEx(&ProcessorNumerator, &NodeIndex);
#ifdef WIN32
		VkDeviceList = static_cast<VkPhysicalDevice*>(VirtualAllocExNuma(GetCurrentProcess(), nullptr, *DeviceCount * sizeof(VkPhysicalDevice), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, NodeIndex));						//Add VK Device list allocation path
		if (!VkDeviceList || *DeviceCount == 0) {
			CleanVKInstance();
			return false;
		}
#endif
		if (!ProcessVkResultRequest(vkEnumeratePhysicalDevices(VkInstance, DeviceCount, VkDeviceList))) {
			CleanVKInstance();
			return false;
		}
		return true;
	}
	inline void SetVKDeviceName(const char* DeviceName) {
		ZeroMemory(GPUName, sizeof(GPUName));
		MultiByteToWideChar(CP_ACP, 0, DeviceName, -1, GPUName, 128);
	}
	inline bool CreateVkDeviceInstance() {
		VkPhysicalDeviceProperties VkPhysicalDeviceProperties;
		uint32_t DeviceCount;
		if (!AllocateVkDeviceEnumerationContext(&DeviceCount)) {
			CleanVKInstance();
			return false;
		}
		for (uint32_t Index = 0; Index < DeviceCount; ++Index) {
			vkGetPhysicalDeviceProperties(VkDeviceList[Index], &VkPhysicalDeviceProperties);
			if (VkPhysicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
				VkDevice = VkDeviceList[Index];
				break;
			}
		}
		if (VkDevice == VK_NULL_HANDLE) {
			for (uint32_t Index = 0; Index < DeviceCount; ++Index) {
				vkGetPhysicalDeviceProperties(VkDeviceList[Index], &VkPhysicalDeviceProperties);
				if (VkPhysicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
					VkDevice = VkDeviceList[Index];
					break;
				}
			}
		}
		SetVKDeviceName(VkPhysicalDeviceProperties.deviceName);
		vkGetPhysicalDeviceFeatures(VkDevice, &VkFeatures);
		VirtualFree(VkDeviceList, 0, MEM_RELEASE);
		VkDeviceList = nullptr;
		return true;
	}
	inline bool CreateVkQueueForActiveDevice() {
		VkQueueFamilyProperties* QueueFamilyProperties;
		uint32_t QueueFamilyNumerator = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(VkDevice, &QueueFamilyNumerator, nullptr);
#ifdef WIN32
		QueueFamilyProperties = static_cast<VkQueueFamilyProperties*>(VirtualAllocExNuma(GetCurrentProcess(), nullptr, QueueFamilyNumerator * sizeof(VkQueueFamilyProperties), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, NodeIndex));
		if (!QueueFamilyProperties) {
			return false;
		}
#endif
		vkGetPhysicalDeviceQueueFamilyProperties(VkDevice, &QueueFamilyNumerator, QueueFamilyProperties);
		for (uint32_t Index = 0; Index < QueueFamilyNumerator; ++Index) {
			uint32_t PlaceholderNumerator = 1;
			if (QueueFamilyProperties[Index].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				QueueFamilyNumerator = Index;
				break;
			}
		}
		VirtualFree(QueueFamilyProperties, 0, MEM_RELEASE);
		vkGetPhysicalDeviceFeatures(VkDevice, &VkFeatures);
		float QueuePriority = 1.0f;
		VkDeviceQueueCreateInfo QueueCreateInfo = {};
		QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		QueueCreateInfo.queueFamilyIndex = QueueFamilyNumerator;
		QueueCreateInfo.queueCount = 1;
		QueueCreateInfo.pQueuePriorities = &QueuePriority;
		VkDeviceCreateInfo DeviceCreateInfo = {};
		DeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		DeviceCreateInfo.queueCreateInfoCount = 1;
		DeviceCreateInfo.pQueueCreateInfos = &QueueCreateInfo;
		DeviceCreateInfo.pEnabledFeatures = &DeviceFeatures;
		if (!ProcessVkResultRequest(vkCreateDevice(VkDevice, &DeviceCreateInfo, nullptr, &VkLogicalDevice))) {
			CleanVKInstance();
			return false;
		}
		vkGetDeviceQueue(VkLogicalDevice, QueueFamilyNumerator, 0, &VkGraphicsQueue);
		VkCommandPoolCreateInfo PoolInfo{};
		PoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		PoolInfo.queueFamilyIndex = QueueFamilyNumerator;
		PoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		if (!ProcessVkResultRequest(vkCreateCommandPool(VkLogicalDevice, &PoolInfo, nullptr, &CommandPool))) {
			CleanVKInstance();
			return false;
		}
		VkCommandBufferAllocateInfo CommandBufferInfo = {};
		CommandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		CommandBufferInfo.commandPool = CommandPool;
		CommandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		CommandBufferInfo.commandBufferCount = LocalBufferDescription.BufferCount;
		if (!ProcessVkResultRequest(vkAllocateCommandBuffers(VkLogicalDevice, &CommandBufferInfo, &GraphicsCommandBuffer))) {
			CleanVKInstance();
			return false;
		}
		return true;
	}
public:
	inline bool Initialize(KRTap* _KRTap, KRMode Mode, KR_BUFFER_DESCRIPTION BUFF_DESC) {
		if ((Mode == KRMode::DirectX && LOCKDX12 == true) || (ActiveAPIMode == KRMode::PlatformUndefined)) {
			return false;
		}
		else {
			ActiveAPIMode = Mode;
		}
		LocalBufferDescription = BUFF_DESC;
#ifdef WIN32
		if (ActiveAPIMode == KRMode::DirectX) {
			if (FAILED(CoCreateGuid(&KRGUID))) {
				return false;
			}
			UINT Flags = 0;
			if (LocalKRTap) {
				bool result = _KRTap->DXTap.Debugger1();
				if (!result) {
					return false;
				}
				else {
					Flags = DXGI_CREATE_FACTORY_DEBUG;
				}
			}
			if (FAILED(CreateDXGIFactory2(Flags, IID_PPV_ARGS(&Factory)))) {
				return false;
			}
			UINT Index = 0;
			if (FAILED(Factory->EnumAdapterByGpuPreference(Index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&Adapter)))) {
				return false;
			}
			else {
				DXGI_ADAPTER_DESC3 AdapterDescription;
				Adapter->GetDesc3(&AdapterDescription);
				wcscpy_s(GPUName, AdapterDescription.Description);
			}
			if (FAILED(D3D12CreateDevice(Adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&Device)))) {
				return false;
			}
			D3D12_COMMAND_QUEUE_DESC CommandQueueDescription = {};
			CommandQueueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			CommandQueueDescription.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
			CommandQueueDescription.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			CommandQueueDescription.NodeMask = 0;
			if (FAILED(Device->CreateCommandQueue1(&CommandQueueDescription, KRGUID, IID_PPV_ARGS(&CommandQueue)))) {
				return false;
			}
		}
#endif
		if (ActiveAPIMode == KRMode::Vulkan) {
			VkApplicationInfo ApplicationInfo{};
			ApplicationInfo.pApplicationName = "KRVK_APP";
			ApplicationInfo.applicationVersion = VK_MAKE_VERSION(UNDEFINED_INTEGER, UNDEFINED_INTEGER, UNDEFINED_INTEGER);
			ApplicationInfo.pEngineName = "KR_RHI";
			ApplicationInfo.engineVersion = VK_MAKE_VERSION(UNDEFINED_INTEGER, UNDEFINED_INTEGER, UNDEFINED_INTEGER);
			ApplicationInfo.apiVersion = VK_API_VERSION_1_4;
#ifdef WIN32
			const char* WindowingExtensions[] = {
				VK_KHR_SURFACE_EXTENSION_NAME,
				VK_KHR_WIN32_SURFACE_EXTENSION_NAME
			};
#elif defined(__linux__)			//Add Wayland/X11 selection

#endif
			VkInstanceCreateInfo InstanceInfo = {};
			InstanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			InstanceInfo.pNext = NULL;
			InstanceInfo.pApplicationInfo = &ApplicationInfo;
			InstanceInfo.enabledLayerCount = (LocalKRTap) ? (uint32_t)(sizeof(LocalKRTap->VKTap.ValidationLayers) / sizeof(LocalKRTap->VKTap.ValidationLayers[0])) : 0;
			InstanceInfo.ppEnabledLayerNames = (LocalKRTap) ? LocalKRTap->VKTap.ValidationLayers : NULL;
			InstanceInfo.enabledExtensionCount = (uint32_t)(sizeof(WindowingExtensions) / sizeof(WindowingExtensions[0]));
			InstanceInfo.ppEnabledExtensionNames = WindowingExtensions;
			if (!ProcessVkResultRequest(vkCreateInstance(&InstanceInfo, nullptr, &VkInstance))) {
#ifdef _DEBUG
				throw UNDEFINED_INTEGER;
#endif
				return false;
			}
			if (!CreateVkDeviceInstance()) {
				return false;
			}
			else if (!CreateVkQueueForActiveDevice()) {
				return false;
			}
			VkPhysicalDeviceProperties Properties;
			vkGetPhysicalDeviceProperties(VkDevice, &Properties);
			uint32_t MajorVersion = VK_VERSION_MAJOR(Properties.apiVersion);
			uint32_t MinorVersion = VK_VERSION_MINOR(Properties.apiVersion);
			if (MajorVersion >= VULKAN_MAJOR_VERSION) {
				if ((MajorVersion > VULKAN_MAJOR_VERSION) ? true : MinorVersion >= VULKAN_MINOR_VERSION) {
					return true;
				}
				return false;
			}
			else {
				return false;
			}
		}
		else if (ActiveAPIMode == KRMode::PlatformUndefined) {
			return false;
		}
		return true;
	}
	inline const wchar_t* GetGPUIdentifierTag() {
		return GPUName;
	}
	inline bool GenerateRegisteredWindow(KRWindow* Window, KR_WINDOW_DESCRIPTION* KR_WINDOW_DESCRIPTION_PTR) {
		if (!Window || !KR_WINDOW_DESCRIPTION_PTR) {
			return false;
		}
#ifdef WIN32
		//Window Creation
		LocalKRWindow = Window;
		const char* Title = KR_WINDOW_DESCRIPTION_PTR->WindowTitle;
		WNDCLASS WindowClass = { };
		HINSTANCE HInstance = GetModuleHandle(nullptr);
		WindowClass.lpfnWndProc = KRWindow::WndProc;
		WindowClass.hInstance = HInstance;
		WindowClass.lpszClassName = Title;
		RegisterClass(&WindowClass);
		Window->hwnd = CreateWindowEx(0, Title, Title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, HInstance, NULL);
		SetWindowLongPtr(Window->hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(LocalKRWindow));
		ShowWindow(Window->hwnd, SW_SHOWNORMAL);
		UpdateWindow(Window->hwnd);
		return true;
#endif
	}
	inline bool CreateSwapChain(KR_SWAP_CHAIN_DESCRIPTION* KR_SWAP_CHAIN_DESCRIPTION_PTR) {
		if (!KR_SWAP_CHAIN_DESCRIPTION_PTR || !LocalKRWindow) {
			return false;
		}
#ifdef WIN32
		if (ActiveAPIMode == KRMode::DirectX) {
			if (!SetOutputDevice()) {
				return false;
			}
			SwapChainDescription.BufferCount = LocalBufferDescription.BufferCount;
			SwapChainDescription.Width = LocalKRWindow->WindowWidth;
			SwapChainDescription.Height = LocalKRWindow->WindowHeight;
			SwapChainDescription.Format = LocalKRWindow->DXGIPollMonitorColorFormat();
			SwapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			SwapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			SwapChainDescription.SampleDesc.Count = 1;
			ComPtr<IDXGISwapChain1> SwapChain1;
			Factory->CreateSwapChainForHwnd(CommandQueue.Get(), LocalKRWindow->hwnd, &SwapChainDescription, nullptr, nullptr, &SwapChain1);
			SwapChain1.As(&LocalKRWindow->SwapChain);
		}
#endif
		if (ActiveAPIMode == KRMode::Vulkan) {

		}
		else if (ActiveAPIMode == KRMode::PlatformUndefined) {
			return false;
		}
		return true;
	}
	inline bool WinMessagePing() {
#ifdef WIN32
		while (PeekMessage(&LocalKRWindow->WindowMessage, LocalKRWindow->hwnd, 0, 0, PM_REMOVE)) {
			TranslateMessage(&LocalKRWindow->WindowMessage);
			DispatchMessage(&LocalKRWindow->WindowMessage);
			if (LocalKRWindow->DisplayChange) {
				if (!SetOutputDevice()) {
					return false;
				}
				MasterRenderStateChangeFlag = true;
				LocalKRWindow->DisplayChange = false;
			}
			else if (LocalKRWindow->SizeChange && !(LocalKRWindow->FullscreenToggle || LocalKRWindow->FullscreenActive)) {
				RECT WindowRect;
				if (GetClientRect(LocalKRWindow->hwnd, &WindowRect)) {
					LocalKRWindow->WindowWidth = WindowRect.right - WindowRect.left;
					LocalKRWindow->WindowHeight = WindowRect.bottom - WindowRect.top;
				}
				MasterRenderStateChangeFlag = true;
				LocalKRWindow->SizeChange = false;
			}
			else if (LocalKRWindow->WindowPositionChanged == true && !(LocalKRWindow->FullscreenToggle || LocalKRWindow->FullscreenActive)) {
				RECT Rect;
				GetWindowRect(LocalKRWindow->hwnd, &Rect);
				LocalKRWindow->WindowX = Rect.left;
				LocalKRWindow->WindowY = Rect.top;
				LocalKRWindow->WindowPositionChanged = false;
			}
			else if (LocalKRWindow->FullscreenToggle == true) {
				if (LocalKRWindow->FullscreenActive == false) {
					SetWindowLong(LocalKRWindow->hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
					RECT Rect = LocalKRWindow->Info.rcMonitor;
					SetWindowPos(LocalKRWindow->hwnd, HWND_TOP, Rect.left, Rect.top, Rect.right - Rect.left, Rect.bottom - Rect.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
					LocalKRWindow->FullscreenActive = true;
				}
				else if (LocalKRWindow->FullscreenActive == true) {
					SetWindowLong(LocalKRWindow->hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
					SetWindowPos(LocalKRWindow->hwnd, HWND_TOP, LocalKRWindow->WindowX, LocalKRWindow->WindowY, LocalKRWindow->WindowWidth, LocalKRWindow->WindowHeight, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
					LocalKRWindow->FullscreenActive = false;
				}
				MasterRenderStateChangeFlag = true;
				LocalKRWindow->FullscreenToggle = false;
			}
		}
		return true;
#endif
	}
private:
	inline bool SetOutputDevice() {
#ifdef WIN32
		if (ActiveAPIMode == KRMode::DirectX) {
			if (LocalKRWindow) {
				LocalKRWindow->Monitor = MonitorFromWindow(LocalKRWindow->hwnd, MONITOR_DEFAULTTONEAREST);
				LocalKRWindow->Info.cbSize = sizeof(MONITORINFO);
				GetMonitorInfo(LocalKRWindow->Monitor, &LocalKRWindow->Info);
				UINT OutputCount = 0;
				for (UINT Index = 0; ; ++Index) {
					DXGI_OUTPUT_DESC OutputDescription{};
					if (Adapter->EnumOutputs(OutputCount, &LocalKRWindow->Output) == DXGI_ERROR_NOT_FOUND) {
						return false;
					}
					LocalKRWindow->Output->GetDesc(&OutputDescription);
					if (OutputDescription.Monitor == LocalKRWindow->Monitor) {
						if (FAILED(LocalKRWindow->Output.As(&LocalKRWindow->ActiveOutput))) {
							return false;
						}
						break;
					}
				}
				return true;
			}
			return false;
		}
#endif
		if (ActiveAPIMode == KRMode::Vulkan) {

		}
		return false;
	}
public:
	~KRFactory() {
		if (ActiveAPIMode == KRMode::Vulkan) {
			CleanVKInstance();
		}
	}
};