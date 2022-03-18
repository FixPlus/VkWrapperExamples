#ifndef TESTAPP_GUI_H
#define TESTAPP_GUI_H

#include <imgui/imgui.h>
#include <vkw/VertexBuffer.hpp>
#include <vkw/RenderPass.hpp>
#include <vkw/Pipeline.hpp>
#include <glm/glm.hpp>
#include <DescriptorSet.hpp>
#include <DescriptorPool.hpp>
#include <map>
#include <Image.hpp>
#include <AssetImport.h>
#include <vkw/Sampler.hpp>
#include <set>
#include <Window.h>

namespace TestApp {

    class GUIBase {
    public:
        GUIBase() : m_context(ImGui::CreateContext()) {

        }

        virtual ~GUIBase() { ImGui::DestroyContext(m_context); }

    protected:
        ImGuiContext *context() const {
            return m_context;
        };
    private:

        friend class WindowIO;

        ImGuiContext *m_context;
    };

    class WindowIO : virtual public Window {
    public:

        WindowIO() : Window(0, 0, "") {}

        void setContext(GUIBase const &gui) {
            m_context = gui.m_context;
        }

    protected:
        bool guiWantCaptureMouse() const;
        bool guiWantCaptureKeyboard() const;

        void keyInput(int key, int scancode, int action, int mods) override;

        void mouseMove(double xpos, double ypos, double xdelta, double ydelta) override;

        void mouseInput(int button, int action, int mods) override;

        void charInput(unsigned int codepoint) override;

        void onPollEvents() override;

    private:
        ImGuiContext *m_context = nullptr;
    };

    class GUIBackend : virtual public GUIBase {
    public:

        GUIBackend(vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass, ShaderLoader const &shaderLoader,
                   TextureLoader textureLoader);

        /** Records draw commands. */
        void draw(vkw::CommandBuffer &commandBuffer);

        /** Fills buffers with content of given context draw data. */
        void push();

    protected:


        void m_updateFontTexture(
                ImFontAtlas *atlas = nullptr /* if null, default font atlas of current context will be used*/);

    private:
        struct GUIVertex : public vkw::AttributeBase<vkw::VertexAttributeType::VEC2F,
                vkw::VertexAttributeType::VEC2F, vkw::VertexAttributeType::RGBA8_UNORM>, public ImDrawVert {
        };
        static const vkw::VertexInputStateCreateInfo<vkw::per_vertex<GUIVertex, 0>> m_vertex_state;

        // keep this in that initialization order

        std::reference_wrapper<vkw::Device> m_device;
        TextureLoader m_font_loader;
        vkw::VertexShader m_vertex_shader;
        vkw::FragmentShader m_fragment_shader;

        vkw::DescriptorSetLayout m_descriptorLayout;

        static vkw::DescriptorSetLayout m_descriptorLayout_init(vkw::Device &device);

        vkw::PipelineLayout m_layout;

        vkw::PipelineLayout m_layout_init(vkw::Device &device);

        vkw::GraphicsPipeline m_pipeline;

        vkw::GraphicsPipeline m_pipeline_init(vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass);

        vkw::Sampler m_sampler;

        static vkw::Sampler m_sampler_init(vkw::Device &device);

        vkw::VertexBuffer<GUIVertex> m_vertices;

        static vkw::VertexBuffer<GUIVertex> m_create_vertex_buffer(vkw::Device &device, uint32_t size);

        vkw::IndexBuffer<VK_INDEX_TYPE_UINT16> m_indices;

        static vkw::IndexBuffer<VK_INDEX_TYPE_UINT16> m_create_index_buffer(vkw::Device &device, uint32_t size);

        ImDrawVert *m_vertices_mapped;
        ImDrawIdx *m_indices_mapped;

        vkw::DescriptorSet m_emplace_set();

        std::vector<vkw::DescriptorPool> m_pools{};
        std::set<ImTextureID> m_used_ids{};
        std::map<ImTextureID, vkw::DescriptorSet> m_sets{};
        std::map<ImTextureID, vkw::Image2DView const *> m_texture_views{};

        std::map<ImTextureID, vkw::ColorImage2D> m_font_textures{};

    };

    class GUIFrontEnd : virtual public GUIBase {
    public:

        void frame(){
            ImGui::SetCurrentContext(context());
            ImGui::NewFrame();

            gui();

            ImGui::EndFrame();
            ImGui::Render();
        }

    protected:

        virtual void gui() const = 0;

    };



}
#endif //TESTAPP_GUI_H
