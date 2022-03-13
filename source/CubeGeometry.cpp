//
// Created by Бушев Дмитрий on 13.03.2022.
//

#include "CubeGeometry.h"
#include "vkw/Device.hpp"
#include "vkw/Queue.hpp"
#include "vkw/CommandPool.hpp"
#include "vkw/CommandBuffer.hpp"

namespace TestApp {

    CubePool::CubePool(vkw::Device &device, uint32_t maxCubes) : m_vertices(device, m_makeCube().size(),
                                                                            {.usage=VMA_MEMORY_USAGE_GPU_ONLY},
                                                                            VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                                                                 m_instances(device, maxCubes,
                                                                             {.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}) {
        VmaAllocationCreateInfo createInfo{};
        createInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        createInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

        vkw::Buffer<PerVertex> stageBuf{device, m_vertices.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, createInfo};


        auto *mapped = stageBuf.map();

        auto vertices = m_makeCube();

        memcpy(mapped, vertices.data(), m_vertices.size() * sizeof(PerVertex));
        stageBuf.flush();
        stageBuf.unmap();

        auto queue = device.getTransferQueue();
        auto commandPool = vkw::CommandPool{device, 0, queue->familyIndex()};
        auto transferCommand = vkw::PrimaryCommandBuffer{commandPool};
        transferCommand.begin(0);
        VkBufferCopy region{};
        region.size = stageBuf.size() * sizeof(PerVertex);
        region.dstOffset = 0;
        region.srcOffset = 0;

        transferCommand.copyBufferToBuffer(stageBuf, m_vertices, {region});

        transferCommand.end();
        queue->submit(transferCommand);
        queue->waitIdle();


        createInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        createInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

        m_instance_mapped = m_instances.map();

    }


    CubePool::CubePool(CubePool
                       &&another) noexcept :
            m_vertices(std::move(another.m_vertices)
            ),
            m_instances(std::move(another.m_instances)
            ),
            m_cubeCount(another
                                .m_cubeCount),
            m_instance_mapped(another
                                      .m_instance_mapped),
            m_cubes(std::move(another.m_cubes)
            ) {
        for (auto &cube: m_cubes) {
            cube.second->m_parent = this;
        }
    }

    void CubePool::bindGeometry(vkw::CommandBuffer &commandBuffer) const{
        commandBuffer.bindVertexBuffer(m_vertices, 0, 0);
        commandBuffer.bindVertexBuffer(m_instances, 1, 0);
    }

    void CubePool::drawGeometry(vkw::CommandBuffer &commandBuffer) const{
        commandBuffer.draw(36, m_cubeCount, 0, 0);
    }

    const vkw::VertexInputStateCreateInfo<vkw::per_vertex<CubePool::PerVertex, 0>, vkw::per_instance<CubePool::PerInstance, 1>> CubePool::m_vsci{};


}
