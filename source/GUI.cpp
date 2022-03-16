#include "GUI.h"
#include "vkw/DescriptorPool.hpp"
#include "vkw/CommandBuffer.hpp"

namespace TestApp {

    const vkw::VertexInputStateCreateInfo<vkw::per_vertex<GUIBackend::GUIVertex, 0>> GUIBackend::m_vertex_state{};

    GUIBackend::GUIBackend(vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass,
                           ShaderLoader const &shaderLoader, TextureLoader textureLoader) :
            m_device(device),
            m_sampler(m_sampler_init(device)),
            m_font_loader(std::move(textureLoader)),
            m_vertex_shader(shaderLoader.loadVertexShader("ui")),
            m_fragment_shader(shaderLoader.loadFragmentShader("ui")),
            m_descriptorLayout(m_descriptorLayout_init(device)),
            m_layout(m_layout_init(device)),
            m_pipeline(m_pipeline_init(device, pass, subpass)),
            m_vertices(m_create_vertex_buffer(device, 1000)),
            m_indices(m_create_index_buffer(device, 1000)) {
        m_vertices_mapped = m_vertices.map();
        m_indices_mapped = m_indices.map();
    }

    vkw::DescriptorSetLayout GUIBackend::m_descriptorLayout_init(vkw::Device &device) {
        vkw::DescriptorSetLayoutBinding textureBinding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                       VK_SHADER_STAGE_FRAGMENT_BIT};
        return {device, textureBinding};
    }

    vkw::PipelineLayout GUIBackend::m_layout_init(vkw::Device &device) {
        VkPushConstantRange constants{};
        constants.size = sizeof(glm::vec2) * 2u;
        constants.offset = 0u;
        constants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        return {device, m_descriptorLayout, {constants}};
    }

    vkw::GraphicsPipeline GUIBackend::m_pipeline_init(vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass) {
        vkw::GraphicsPipelineCreateInfo createInfo{pass, subpass, m_layout};
        createInfo.addVertexInputState(m_vertex_state);
        createInfo.addVertexShader(m_vertex_shader);
        createInfo.addFragmentShader(m_fragment_shader);

        // TODO: finish pipeline layout definition

        VkPipelineColorBlendAttachmentState state{};
        state.blendEnable = VK_TRUE;
        state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                               VK_COLOR_COMPONENT_A_BIT;
        state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        state.colorBlendOp = VK_BLEND_OP_ADD;
        state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        state.alphaBlendOp = VK_BLEND_OP_ADD;

        createInfo.addBlendState(state, 0);

        return {device, createInfo};
    }

    void GUIBackend::m_updateFontTexture(ImFontAtlas *atlas) {
        ImGui::SetCurrentContext(context());

        auto &io = ImGui::GetIO();

        if (!atlas)
            atlas = io.Fonts;

        auto textureID = atlas->TexID;

        m_used_ids.emplace(textureID);

        unsigned char *fontData;
        int texWidth, texHeight;

        atlas->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);

        m_font_textures.erase(textureID);
        auto &newTexture = m_font_textures.emplace(textureID, m_font_loader.loadTexture(fontData, texWidth,
                                                                                        texHeight)).first->second;
        VkComponentMapping mapping;
        mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        m_texture_views.erase(textureID);
        auto *texView = static_cast<vkw::ColorImage2DView const *>(m_texture_views.emplace(textureID,
                                                                                           &newTexture.getView<vkw::ColorImageView>(
                                                                                                   m_device,
                                                                                                   newTexture.format(),
                                                                                                   mapping)).first->second);

        if (!m_sets.contains(textureID))
            m_sets.emplace(textureID, m_emplace_set());

        m_sets.at(textureID).write(0, *texView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_sampler);

    }

    vkw::DescriptorSet GUIBackend::m_emplace_set() {
        for (auto &pool: m_pools) {
            if (pool.currentSetsCount() != pool.maxSets())
                return vkw::DescriptorSet{pool, m_descriptorLayout};
        }

        std::vector<VkDescriptorPoolSize> m_sizes{};

        constexpr const uint32_t maxSets = 10u;
        m_pools.push_back(vkw::DescriptorPool{m_device.get(), maxSets,
                                              {VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = maxSets}}});

        return vkw::DescriptorSet{m_pools.back(), m_descriptorLayout};
    }

    vkw::Sampler GUIBackend::m_sampler_init(vkw::Device &device) {
        VkSamplerCreateInfo createInfo{};

        createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.minFilter = VK_FILTER_LINEAR;
        createInfo.magFilter = VK_FILTER_LINEAR;
        createInfo.minLod = 0.0f;
        createInfo.maxLod = 1.0f;
        createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        createInfo.anisotropyEnable = false;

        return {device, createInfo};
    }

    void GUIBackend::m_update() {
        ImGui::SetCurrentContext(context());

        auto *drawData = ImGui::GetDrawData();

        if (!drawData)
            return;

        auto vertices = drawData->TotalVtxCount;
        auto indices = drawData->TotalIdxCount;

        // Reallocate buffers if not enough space

        if (vertices > m_vertices.size()) {
            m_vertices = m_create_vertex_buffer(m_device, vertices);
            m_vertices_mapped = m_vertices.map();
        }

        if (indices > m_indices.size()) {
            m_indices = m_create_index_buffer(m_device, indices);
            m_indices_mapped = m_indices.map();
        }

        // Upload data
        ImDrawVert *vtxDst = m_vertices_mapped;
        ImDrawIdx *idxDst = m_indices_mapped;

        for (int n = 0; n < drawData->CmdListsCount; n++) {
            const ImDrawList *cmd_list = drawData->CmdLists[n];
            memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtxDst += cmd_list->VtxBuffer.Size;
            idxDst += cmd_list->IdxBuffer.Size;
        }

        m_vertices.flush();
        m_indices.flush();
    }

    vkw::VertexBuffer<GUIBackend::GUIVertex> GUIBackend::m_create_vertex_buffer(vkw::Device &device, uint32_t size) {
        VmaAllocationCreateInfo createInfo{};
        createInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        createInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

        return {device, size, createInfo};
    }

    vkw::IndexBuffer<VK_INDEX_TYPE_UINT16> GUIBackend::m_create_index_buffer(vkw::Device &device, uint32_t size) {
        VmaAllocationCreateInfo createInfo{};
        createInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        createInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

        return {device, size, createInfo};
    }

    void GUIBackend::draw(vkw::CommandBuffer &commandBuffer) {
        ImGui::SetCurrentContext(context());
        auto *imDrawData = ImGui::GetDrawData();
        int32_t vertexOffset = 0;
        int32_t indexOffset = 0;

        if ((!imDrawData) || (imDrawData->CmdListsCount == 0)) {
            return;
        }

        ImGuiIO &io = ImGui::GetIO();

        commandBuffer.bindGraphicsPipeline(m_pipeline);

        struct PushConstBlock {
            glm::vec2 scale;
            glm::vec2 translate;
        } pushConstBlock;

        pushConstBlock.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
        pushConstBlock.translate = glm::vec2(-1.0f);

        commandBuffer.pushConstants(m_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, pushConstBlock);

        commandBuffer.bindVertexBuffer(m_vertices, 0, 0);
        commandBuffer.bindIndexBuffer(m_indices, 0);

        ImTextureID cachedID = m_sets.begin()->first;
        auto *descriptor = &m_sets.begin()->second;

        commandBuffer.bindDescriptorSets(m_layout, VK_PIPELINE_BIND_POINT_GRAPHICS, *descriptor, 0);

        for (int32_t i = 0; i < imDrawData->CmdListsCount; i++) {
            const ImDrawList *cmd_list = imDrawData->CmdLists[i];
            for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++) {
                const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[j];
                auto currentTextureID = pcmd->TextureId;

                if (currentTextureID != cachedID) {
                    descriptor = &m_sets.at(currentTextureID);
                    cachedID = currentTextureID;
                    commandBuffer.bindDescriptorSets(m_layout, VK_PIPELINE_BIND_POINT_GRAPHICS, *descriptor, 0);
                }

                VkRect2D scissorRect;
                scissorRect.offset.x = std::max((int32_t) (pcmd->ClipRect.x), 0);
                scissorRect.offset.y = std::max((int32_t) (pcmd->ClipRect.y), 0);
                scissorRect.extent.width = (uint32_t) (pcmd->ClipRect.z - pcmd->ClipRect.x);
                scissorRect.extent.height = (uint32_t) (pcmd->ClipRect.w - pcmd->ClipRect.y);
                commandBuffer.setScissors({scissorRect}, 0);
                commandBuffer.drawIndexed(pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
                indexOffset += pcmd->ElemCount;
            }
            vertexOffset += cmd_list->VtxBuffer.Size;
        }
    }

    void WindowIO::keyInput(int key, int scancode, int action, int mods) {
        ImGui::SetCurrentContext(m_context);
        auto &io = ImGui::GetIO();

        if (action == GLFW_PRESS) {
            io.AddKeyEvent(key, true);
        } else {
            io.AddKeyEvent(key, false);
        }

    }

    void WindowIO::mouseMove(double xpos, double ypos, double xdelta, double ydelta) {
        ImGui::SetCurrentContext(m_context);
        auto &io = ImGui::GetIO();

        io.AddMousePosEvent(xpos, ypos);
    }

    void WindowIO::mouseInput(int button, int action, int mods) {
        ImGui::SetCurrentContext(m_context);
        auto &io = ImGui::GetIO();

        if (action == GLFW_PRESS) {
            io.AddMouseButtonEvent(button, true);
        } else {
            io.AddMouseButtonEvent(button, false);
        }
    }

    void WindowIO::onPollEvents() {
        ImGui::SetCurrentContext(m_context);
        auto &io = ImGui::GetIO();
        auto size = framebufferExtents();
        io.DisplaySize = ImVec2{(float) size.first, (float) size.second};
    }
}