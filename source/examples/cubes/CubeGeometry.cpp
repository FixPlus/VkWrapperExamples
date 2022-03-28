#include "CubeGeometry.h"
#include "vkw/Device.hpp"
#include "vkw/Queue.hpp"
#include "vkw/CommandPool.hpp"
#include "vkw/CommandBuffer.hpp"
#include <cstring>
#include <RenderEngine/RecordingState.h>

namespace TestApp {

    CubePool::CubePool(vkw::Device &device, uint32_t maxCubes) : m_geometry_layout(device,
                                                                                   RenderEngine::GeometryLayout::CreateInfo{.vertexInputState=&m_vsci, .substageDescription=RenderEngine::SubstageDescription{.shaderSubstageName="cube"},
                                                                                           .maxGeometries=1}),
                                                                 m_geometry(device, m_geometry_layout, maxCubes) {
    }


    CubePool::CubePool(CubePool
                       &&another) noexcept:
            m_geometry(std::move(another.m_geometry)
            ),
            m_geometry_layout(std::move(another.m_geometry_layout)),
            m_cubeCount(another
                                .m_cubeCount),
            m_cubes(std::move(another.m_cubes)
            ) {
        for (auto &cube: m_cubes) {
            cube.second->m_parent = this;
        }
    }

    void CubePool::draw(RenderEngine::GraphicsRecordingState &state) const {
        bind(state);

        state.commands().draw(36, m_cubeCount, 0, 0);
    }

    const vkw::VertexInputStateCreateInfo<vkw::per_vertex<CubePool::PerVertex, 0>, vkw::per_instance<CubePool::PerInstance, 1>> CubePool::m_vsci{};

    void CubePool::bind(RenderEngine::GraphicsRecordingState &state) const {
        m_geometry.bind(state);
    }


    CubePool::M_Cube_geometry::M_Cube_geometry(vkw::Device &device, RenderEngine::GeometryLayout &layout,
                                               uint32_t maxCubes) :
            RenderEngine::Geometry(layout), m_vertices(device, m_makeCube().size(),
                                                       {.usage=VMA_MEMORY_USAGE_GPU_ONLY},
                                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT),
            m_instances(device, maxCubes,
                        {.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
            m_instance_mapped(m_instances.map()) {
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

    }


    std::vector<CubePool::PerVertex> CubePool::M_Cube_geometry::m_makeCube() {
        std::vector<PerVertex> ret{};
        // top
        ret.push_back({.pos = {0.5f, 0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {0.0f, 0.0f}});
        ret.push_back({.pos = {-0.5f, -0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {1.0f, 1.0f}});
        ret.push_back({.pos = {0.5f, -0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {1.0f, 0.0f}});


        ret.push_back({.pos = {0.5f, 0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {0.0f, 0.0f}});
        ret.push_back({.pos = {-0.5f, 0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {0.0f, 1.0f}});
        ret.push_back({.pos = {-0.5f, -0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {1.0f, 1.0f}});


        // bottom

        ret.push_back({.pos = {0.5f, 0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv = {0.0f, 0.0f}});
        ret.push_back({.pos = {0.5f, -0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv = {1.0f, 0.0f}});
        ret.push_back({.pos = {-0.5f, -0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv = {1.0f, 1.0f}});


        ret.push_back({.pos = {0.5f, 0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv = {0.0f, 0.0f}});
        ret.push_back({.pos = {-0.5f, -0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv = {1.0f, 1.0f}});
        ret.push_back({.pos = {-0.5f, 0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv = {0.0f, 1.0f}});



        // east

        ret.push_back({.pos = {0.5f, 0.5f, 0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv = {0.0f, 0.0f}});
        ret.push_back({.pos = {0.5f, 0.5f, -0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv = {1.0f, 0.0f}});
        ret.push_back({.pos = {-0.5f, 0.5f, -0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv = {1.0f, 1.0f}});

        ret.push_back({.pos = {0.5f, 0.5f, 0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv = {0.0f, 0.0f}});
        ret.push_back({.pos = {-0.5f, 0.5f, -0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv = {1.0f, 1.0f}});
        ret.push_back({.pos = {-0.5f, 0.5f, 0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv = {0.0f, 1.0f}});


        // west

        ret.push_back({.pos = {0.5f, -0.5f, 0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv = {0.0f, 0.0f}});
        ret.push_back({.pos = {-0.5f, -0.5f, -0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv = {1.0f, 1.0f}});
        ret.push_back({.pos = {0.5f, -0.5f, -0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv = {1.0f, 0.0f}});


        ret.push_back({.pos = {0.5f, -0.5f, 0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv = {0.0f, 0.0f}});
        ret.push_back({.pos = {-0.5f, -0.5f, 0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv = {0.0f, 1.0f}});
        ret.push_back({.pos = {-0.5f, -0.5f, -0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv = {1.0f, 1.0f}});



        // north

        ret.push_back({.pos = {0.5f, 0.5f, 0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv = {0.0f, 0.0f}});
        ret.push_back({.pos = {0.5f, -0.5f, 0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv = {1.0f, 0.0f}});
        ret.push_back({.pos = {0.5f, -0.5f, -0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv = {1.0f, 1.0f}});


        ret.push_back({.pos = {0.5f, 0.5f, 0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv = {0.0f, 0.0f}});
        ret.push_back({.pos = {0.5f, -0.5f, -0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv = {1.0f, 1.0f}});
        ret.push_back({.pos = {0.5f, 0.5f, -0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv = {0.0f, 1.0f}});



        // south

        ret.push_back({.pos = {-0.5f, 0.5f, 0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv = {0.0f, 0.0f}});
        ret.push_back({.pos = {-0.5f, -0.5f, -0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv = {1.0f, 1.0f}});
        ret.push_back({.pos = {-0.5f, -0.5f, 0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv = {1.0f, 0.0f}});


        ret.push_back({.pos = {-0.5f, 0.5f, 0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv = {0.0f, 0.0f}});
        ret.push_back({.pos = {-0.5f, 0.5f, -0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv = {0.0f, 1.0f}});
        ret.push_back({.pos = {-0.5f, -0.5f, -0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv = {1.0f, 1.0f}});


        return ret;
    }

    void CubePool::M_Cube_geometry::bind(RenderEngine::GraphicsRecordingState &state) const {
        RenderEngine::Geometry::bind(state);
        state.commands().bindVertexBuffer(m_vertices, 0, 0);
        state.commands().bindVertexBuffer(m_instances, 1, 0);
    }

}
