#include "VulkanMemoryMonitor.h"


namespace TestApp{

    MemoryStatistics::MemoryStatistics(TestApp::GUIFrontEnd& gui, VulkanMemoryMonitor const& monitor):
    TestApp::GUIWindow(gui, WindowSettings{.title="Memory monitor"}), m_monitor(monitor){

    }

    void MemoryStatistics::onGui() {
        ImGui::Text("Vulkan Library Memory: %d", m_monitor.get().totalHostMemory());
    }
}