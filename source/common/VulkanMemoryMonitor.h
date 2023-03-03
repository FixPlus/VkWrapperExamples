#ifndef TESTAPP_VULKANMEMORYMONITOR_H
#define TESTAPP_VULKANMEMORYMONITOR_H

#include <vkw/HostAllocator.hpp>
#include "GUI.h"

namespace TestApp{

class VulkanMemoryMonitor final: public vkw::HostAllocator{
public:
    VulkanMemoryMonitor():vkw::HostAllocator(true){}

    auto totalHostMemory() const{ return m_totalHostMemory; }
    auto totalAllocations() const{ return m_totalAllocations; }
    auto totalReallocations() const{ return m_totalReallocations; }
    auto totalFrees() const{ return m_totalFrees; }
protected:
    void *allocate(size_t size, size_t alignment,
                           VkSystemAllocationScope scope) noexcept override {
        auto* allocated = vkw::HostAllocator::allocate(size, alignment, scope);
        m_allocations_track.emplace(allocated, size);
        m_totalHostMemory += size;
        m_totalAllocations++;
        return allocated;
    }

    virtual void *reallocate(void *original, size_t size, size_t alignment,
                             VkSystemAllocationScope scope) noexcept override{
        auto* reallocated = vkw::HostAllocator::reallocate(original, size, alignment, scope);
        auto entry = m_allocations_track.extract(original);
        m_totalHostMemory -= entry.mapped();
        m_totalHostMemory += size;
        m_allocations_track.emplace(reallocated, entry.mapped());
        m_totalReallocations++;
        return reallocated;
    }

    virtual void free(void *memory) noexcept override{
        vkw::HostAllocator::free(memory);
        if(!memory)
            return;
        auto entry = m_allocations_track.extract(memory);
        m_totalHostMemory -= entry.mapped();
        m_totalFrees++;
    }
private:
    std::unordered_map<void*, size_t> m_allocations_track;
    size_t m_totalHostMemory = 0;
    size_t m_totalAllocations = 0;
    size_t m_totalReallocations = 0;
    size_t m_totalFrees = 0;
};

class MemoryStatistics: public TestApp::GUIWindow{
public:
    MemoryStatistics(TestApp::GUIFrontEnd& gui, VulkanMemoryMonitor const& monitor);

protected:
    void onGui() override;

    vkw::StrongReference<VulkanMemoryMonitor const> m_monitor;
};
}
#endif //TESTAPP_VULKANMEMORYMONITOR_H
