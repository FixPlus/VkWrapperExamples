#include "GUI.h"
#include "vkw/DescriptorPool.hpp"
#include "vkw/CommandBuffer.hpp"

namespace TestApp {

    const vkw::VertexInputStateCreateInfo<vkw::per_vertex<GUIBackend::GUIVertex, 0>> GUIBackend::m_vertex_state{};

    static VkPipelineColorBlendAttachmentState getUIBlendState(){
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
        return state;
    }
    GUIBackend::GUIBackend(vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass, RenderEngine::TextureLoader textureLoader) :
            m_device(device),
            m_sampler(m_sampler_init(device)),
            m_font_loader(std::move(textureLoader)),
            m_geometryLayout(device,
                             RenderEngine::GeometryLayout::CreateInfo{.vertexInputState=&m_vertex_state, .substageDescription=RenderEngine::SubstageDescription{.shaderSubstageName="ui", .pushConstants={
                                     VkPushConstantRange{.stageFlags=VK_SHADER_STAGE_VERTEX_BIT, .offset=0u, .size=
                                     sizeof(glm::vec2) * 2u}}}, .maxGeometries=1}),
            m_geometry(m_geometryLayout),
            m_projectionLayout(device, RenderEngine::SubstageDescription{.shaderSubstageName="flat"}, 1),
            m_projection(m_projectionLayout),
            m_materialLayout(device, RenderEngine::MaterialLayout::CreateInfo{.substageDescription=RenderEngine::SubstageDescription{.shaderSubstageName="ui", .setBindings={vkw::DescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}}}, .maxMaterials=1000}),
            m_lightingLayout(device, RenderEngine::LightingLayout::CreateInfo{.substageDescription=RenderEngine::SubstageDescription{.shaderSubstageName="flat"},.pass=pass,.subpass=subpass, .blendStates={{getUIBlendState(), 0}}}, 1),
            m_lighting(m_lightingLayout),
    m_vertices(m_create_vertex_buffer(device, 1000)),
    m_indices(m_create_index_buffer(device, 1000)) {
        m_vertices_mapped = m_vertices.map();
        m_indices_mapped = m_indices.map();
    }

    void GUIBackend::m_updateFontTexture(ImFontAtlas *atlas) {
        ImGui::SetCurrentContext(context());

        auto &io = ImGui::GetIO();

        if (!atlas)
            atlas = io.Fonts;

        auto textureID = static_cast<TextureView>(atlas->TexID);

        if(m_font_textures.contains(textureID)){
            m_font_textures.erase(textureID);
        }
        if(m_materials.contains(textureID)){
            m_materials.erase(textureID);
        }

        unsigned char *fontData;
        int texWidth, texHeight;

        atlas->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
        auto fontTexture = m_font_loader.loadTexture(fontData, texWidth,
                                                     texHeight);


        VkComponentMapping mapping;
        mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        TextureView view = &fontTexture.getView<vkw::ColorImageView>(
                m_device,
                fontTexture.format(),
        mapping);

        auto &newTexture = m_font_textures.emplace(view, std::move(fontTexture)).first->second;

        m_materials.emplace(view, Material{m_device, m_materialLayout, *view, m_sampler});
        atlas->TexID = const_cast<vkw::Image2DView*>(view);

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

    void GUIBackend::push() {
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

    void GUIBackend::draw(RenderEngine::GraphicsRecordingState& recorder) {
        ImGui::SetCurrentContext(context());
        auto *imDrawData = ImGui::GetDrawData();
        int32_t vertexOffset = 0;
        int32_t indexOffset = 0;

        if ((!imDrawData) || (imDrawData->CmdListsCount == 0)) {
            return;
        }

        ImGuiIO &io = ImGui::GetIO();

        auto cachedID = static_cast<TextureView>(io.Fonts->TexID);

        recorder.setGeometry(m_geometry);
        recorder.setProjection(m_projection);
        recorder.setLighting(m_lighting);
        recorder.setMaterial(m_materials.at(cachedID));
        recorder.bindPipeline();

        struct PushConstBlock {
            glm::vec2 scale;
            glm::vec2 translate;
        } pushConstBlock;

        pushConstBlock.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
        pushConstBlock.translate = glm::vec2(-1.0f);

        recorder.pushConstants(pushConstBlock, VK_SHADER_STAGE_VERTEX_BIT, 0);

        recorder.commands().bindVertexBuffer(m_vertices, 0, 0);
        recorder.commands().bindIndexBuffer(m_indices, 0);


        for (int32_t i = 0; i < imDrawData->CmdListsCount; i++) {
            const ImDrawList *cmd_list = imDrawData->CmdLists[i];
            for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++) {
                const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[j];
                auto currentTextureID = static_cast<TextureView>(pcmd->TextureId);

                if (currentTextureID != cachedID) {
                    recorder.setMaterial(m_materials.at(currentTextureID));
                    cachedID = currentTextureID;
                }

                VkRect2D scissorRect;
                scissorRect.offset.x = std::max((int32_t) (pcmd->ClipRect.x), 0);
                scissorRect.offset.y = std::max((int32_t) (pcmd->ClipRect.y), 0);
                scissorRect.extent.width = (uint32_t) (pcmd->ClipRect.z - pcmd->ClipRect.x);
                scissorRect.extent.height = (uint32_t) (pcmd->ClipRect.w - pcmd->ClipRect.y);
                recorder.commands().setScissors({scissorRect}, 0);
                recorder.commands().drawIndexed(pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
                indexOffset += pcmd->ElemCount;
            }
            vertexOffset += cmd_list->VtxBuffer.Size;
        }
    }

    static int glfwToImGuiKeyMap(int key) {
        switch (key) {
            case GLFW_KEY_TAB:
                return ImGuiKey_Tab;
            case GLFW_KEY_LEFT:
                return ImGuiKey_LeftArrow;
            case GLFW_KEY_RIGHT:
                return ImGuiKey_RightArrow;
            case GLFW_KEY_UP:
                return ImGuiKey_UpArrow;
            case GLFW_KEY_DOWN:
                return ImGuiKey_DownArrow;
            case GLFW_KEY_PAGE_UP:
                return ImGuiKey_PageUp;
            case GLFW_KEY_PAGE_DOWN:
                return ImGuiKey_PageDown;
            case GLFW_KEY_HOME:
                return ImGuiKey_Home;
            case GLFW_KEY_END:
                return ImGuiKey_End;
            case GLFW_KEY_INSERT:
                return ImGuiKey_Insert;
            case GLFW_KEY_DELETE:
                return ImGuiKey_Delete;
            case GLFW_KEY_BACKSPACE:
                return ImGuiKey_Backspace;
            case GLFW_KEY_SPACE:
                return ImGuiKey_Space;
            case GLFW_KEY_ESCAPE:
                return ImGuiKey_Escape;
            case GLFW_KEY_ENTER:
                return ImGuiKey_Enter;
            case GLFW_KEY_LEFT_SHIFT:
                return ImGuiKey_LeftShift;
            case GLFW_KEY_LEFT_CONTROL:
                return ImGuiKey_LeftCtrl;
            case GLFW_KEY_LEFT_ALT:
                return ImGuiKey_LeftAlt;
            case GLFW_KEY_LEFT_SUPER:
                return ImGuiKey_LeftSuper;
            case GLFW_KEY_RIGHT_SHIFT:
                return ImGuiKey_RightShift;
            case GLFW_KEY_RIGHT_CONTROL:
                return ImGuiKey_RightCtrl;
            case GLFW_KEY_RIGHT_ALT:
                return ImGuiKey_RightAlt;
            case GLFW_KEY_RIGHT_SUPER:
                return ImGuiKey_RightSuper;
            case GLFW_KEY_MENU:
                return ImGuiKey_Menu;
#define KEY_ENTRY(X) case GLFW_KEY_ ##X: return ImGuiKey_ ##X;
            KEY_ENTRY(0)
            KEY_ENTRY(1)
            KEY_ENTRY(2)
            KEY_ENTRY(3)
            KEY_ENTRY(4)
            KEY_ENTRY(5)
            KEY_ENTRY(6)
            KEY_ENTRY(7)
            KEY_ENTRY(8)
            KEY_ENTRY(9)
            KEY_ENTRY(A)
            KEY_ENTRY(B)
            KEY_ENTRY(C)
            KEY_ENTRY(D)
            KEY_ENTRY(E)
            KEY_ENTRY(F)
            KEY_ENTRY(G)
            KEY_ENTRY(H)
            KEY_ENTRY(I)
            KEY_ENTRY(J)
            KEY_ENTRY(K)
            KEY_ENTRY(L)
            KEY_ENTRY(M)
            KEY_ENTRY(N)
            KEY_ENTRY(O)
            KEY_ENTRY(P)
            KEY_ENTRY(Q)
            KEY_ENTRY(R)
            KEY_ENTRY(S)
            KEY_ENTRY(T)
            KEY_ENTRY(U)
            KEY_ENTRY(V)
            KEY_ENTRY(W)
            KEY_ENTRY(X)
            KEY_ENTRY(Y)
            KEY_ENTRY(Z)
            KEY_ENTRY(F1)
            KEY_ENTRY(F2)
            KEY_ENTRY(F3)
            KEY_ENTRY(F4)
            KEY_ENTRY(F5)
            KEY_ENTRY(F6)
            KEY_ENTRY(F7)
            KEY_ENTRY(F8)
            KEY_ENTRY(F9)
            KEY_ENTRY(F10)
            KEY_ENTRY(F11)
            KEY_ENTRY(F12)
#undef KEY_ENTRY
            case GLFW_KEY_APOSTROPHE:
                return ImGuiKey_Apostrophe;
            case GLFW_KEY_COMMA:
                return ImGuiKey_Comma;
            case GLFW_KEY_MINUS:
                return ImGuiKey_Minus;
            case GLFW_KEY_PERIOD:
                return ImGuiKey_Period;
            case GLFW_KEY_SLASH:
                return ImGuiKey_Slash;
            case GLFW_KEY_BACKSLASH:
                return ImGuiKey_Backslash;
            case GLFW_KEY_EQUAL:
                return ImGuiKey_Equal;
            case GLFW_KEY_LEFT_BRACKET:
                return ImGuiKey_LeftBracket;
            case GLFW_KEY_RIGHT_BRACKET:
                return ImGuiKey_RightBracket;
            case GLFW_KEY_GRAVE_ACCENT:
                return ImGuiKey_GraveAccent;
            case GLFW_KEY_CAPS_LOCK:
                return ImGuiKey_CapsLock;
            case GLFW_KEY_NUM_LOCK:
                return ImGuiKey_NumLock;
            case GLFW_KEY_SCROLL_LOCK:
                return ImGuiKey_ScrollLock;
            case GLFW_KEY_PRINT_SCREEN:
                return ImGuiKey_PrintScreen;
            case GLFW_KEY_PAUSE:
                return ImGuiKey_Pause;
#define KEY_ENTRY(X) case GLFW_KEY_KP_ ##X: return ImGuiKey_Keypad ##X;
            KEY_ENTRY(0)
            KEY_ENTRY(1)
            KEY_ENTRY(2)
            KEY_ENTRY(3)
            KEY_ENTRY(4)
            KEY_ENTRY(5)
            KEY_ENTRY(6)
            KEY_ENTRY(7)
            KEY_ENTRY(8)
            KEY_ENTRY(9)
#undef KEY_ENTRY
            case GLFW_KEY_KP_DECIMAL:
                return ImGuiKey_KeypadDecimal;
            case GLFW_KEY_KP_ADD:
                return ImGuiKey_KeypadAdd;
            case GLFW_KEY_KP_EQUAL:
                return ImGuiKey_KeypadEqual;
            case GLFW_KEY_KP_SUBTRACT:
                return ImGuiKey_KeypadSubtract;
            case GLFW_KEY_KP_DIVIDE:
                return ImGuiKey_KeypadDivide;
            case GLFW_KEY_KP_MULTIPLY:
                return ImGuiKey_KeypadMultiply;
            case GLFW_KEY_KP_ENTER:
                return ImGuiKey_KeypadEnter;
            default:
                return ImGuiKey_None;
        }
    }

    void WindowIO::keyInput(int key, int scancode, int action, int mods) {
        ImGui::SetCurrentContext(m_context);
        auto &io = ImGui::GetIO();

        auto imGuiKey = glfwToImGuiKeyMap(key);

        auto down = action == GLFW_PRESS;

        if (imGuiKey != ImGuiKey_None)
            io.AddKeyEvent(imGuiKey, down);

        if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)
            io.AddKeyEvent(ImGuiKey_ModShift, down);

        if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL)
            io.AddKeyEvent(ImGuiKey_ModCtrl, down);

        if (key == GLFW_KEY_LEFT_ALT || key == GLFW_KEY_RIGHT_ALT)
            io.AddKeyEvent(ImGuiKey_ModAlt, down);

        if (key == GLFW_KEY_LEFT_SUPER || key == GLFW_KEY_RIGHT_SUPER)
            io.AddKeyEvent(ImGuiKey_ModSuper, down);

    }

    void WindowIO::mouseMove(double xpos, double ypos, double xdelta, double ydelta) {
        ImGui::SetCurrentContext(m_context);
        auto &io = ImGui::GetIO();
        if (cursorDisabled()) {
            io.AddMousePosEvent(FLT_MAX, FLT_MAX);
        } else {
            io.AddMousePosEvent(xpos, ypos);
        }
    }

    void WindowIO::mouseInput(int button, int action, int mods) {
        ImGui::SetCurrentContext(m_context);
        auto &io = ImGui::GetIO();
        if (!cursorDisabled())
            io.AddMouseButtonEvent(button, action == GLFW_PRESS);
    }

    void WindowIO::onPollEvents() {
        ImGui::SetCurrentContext(m_context);
        auto &io = ImGui::GetIO();
        auto size = framebufferExtents();
        io.DisplaySize = ImVec2{(float) size.first, (float) size.second};
        io.DeltaTime = clock().frameTime();
    }

    void WindowIO::charInput(unsigned int codepoint) {
        ImGui::SetCurrentContext(m_context);
        auto &io = ImGui::GetIO();

        io.AddInputCharacter(codepoint);

    }

    bool WindowIO::guiWantCaptureKeyboard() const {
        ImGui::SetCurrentContext(m_context);
        auto &io = ImGui::GetIO();
        return io.WantCaptureKeyboard;
    }

    bool WindowIO::guiWantCaptureMouse() const {
        ImGui::SetCurrentContext(m_context);
        auto &io = ImGui::GetIO();
        return io.WantCaptureMouse;
    }

    GUIBackend::Material::Material(vkw::Device &device, RenderEngine::MaterialLayout &layout,
                                   const vkw::Image2DView &texture, const vkw::Sampler &sampler): RenderEngine::Material(layout) {
        auto* pTexture = &texture;
        if(auto colorTexture = dynamic_cast<const vkw::ColorImageView*>(pTexture)){
            set().write(0, *colorTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler);
        } else if(auto depthTexture = dynamic_cast<const vkw::DepthImageView*>(pTexture)){
            set().write(0, *depthTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler);
        } else if(auto stencilTexture = dynamic_cast<const vkw::StencilImageView*>(pTexture)){
            set().write(0, *stencilTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler);
        } else{
            throw std::runtime_error("GUI: failed to create Combined Image sampler: broken image view");
        }
    }

    GUIWindow::GUIWindow(GUIFrontEnd& parent, WindowSettings settings): m_parent(parent), m_settings(std::move(settings)){
        m_parent.get().m_windows.emplace(this);
    }

    GUIWindow::~GUIWindow(){
        m_parent.get().m_windows.erase(this);
    }

    GUIWindow::GUIWindow(GUIWindow &&another) noexcept: GUIWindow(another.m_parent, another.m_settings){
        m_opened = another.m_opened;
    }

    void GUIWindow::m_compileFlags(){
        m_flags = 0;
        if(!m_settings.movable){
            m_flags |= ImGuiWindowFlags_NoMove;
        }
        if(!m_settings.resizable){
            m_flags |= ImGuiWindowFlags_NoResize;
        }
        if(m_settings.autoSize){
            m_flags |= ImGuiWindowFlags_AlwaysAutoResize;
        }
    }

    GUIWindow &GUIWindow::operator=(GUIWindow &&another) noexcept {
        if(&m_parent.get() != &another.m_parent.get()) {
            m_parent.get().m_windows.erase(this);
            m_parent = another.m_parent;
            m_parent.get().m_windows.emplace(this);
        }

        m_settings = std::move(another.m_settings);
        m_opened = another.m_opened;

        return *this;
    }

    void GUIWindow::drawWindow() {
        if(m_opened) {
            if(m_settings.size.x != 0 && m_settings.size.y != 0){
                ImGui::SetNextWindowSize(m_settings.size);
            }
            if(m_settings.pos.x != -1 && m_settings.pos.y != -1)
                ImGui::SetNextWindowPos(m_settings.pos, ImGuiCond_Once);

            if(ImGui::Begin(m_settings.title.c_str(), &m_opened, m_flags))
                onGui();

            ImGui::End();
        }
    }

    void GUIFrontEnd::frame() {
        ImGui::SetCurrentContext(context());
        ImGui::NewFrame();

        for(auto* window: m_windows){
            window->drawWindow();
        }

        gui();

        ImGui::EndFrame();
        ImGui::Render();

    }

    GUIFrontEnd::GUIFrontEnd(GUIFrontEnd &&another) noexcept: m_windows(std::move(another.m_windows)) {
        for(auto* window: another.m_windows){
            window->m_parent = *this;
        }
    }
}