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
#include <RenderEngine/AssetImport/AssetImport.h>
#include <vkw/Sampler.hpp>
#include <set>
#include <RenderEngine/Window/Window.h>
#include <RenderEngine/RecordingState.h>

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

    class WindowIO : virtual public RenderEngine::Window {
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
        using TextureView = vkw::Image2DView const*;

        GUIBackend(vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass, RenderEngine::TextureLoader textureLoader);

        /** Records draw commands. */
        void draw(RenderEngine::GraphicsRecordingState& recorder);

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
        RenderEngine::TextureLoader m_font_loader;

        RenderEngine::GeometryLayout m_geometryLayout;
        RenderEngine::Geometry m_geometry;

        RenderEngine::ProjectionLayout m_projectionLayout;
        RenderEngine::Projection m_projection;

        RenderEngine::MaterialLayout m_materialLayout;

        struct Material: public RenderEngine::Material{
        public:
            Material(vkw::Device& device, RenderEngine::MaterialLayout& layout, vkw::Image2DView const& texture, vkw::Sampler const& sampler);
        };

        RenderEngine::LightingLayout m_lightingLayout;
        RenderEngine::Lighting m_lighting;

        vkw::Sampler m_sampler;

        static vkw::Sampler m_sampler_init(vkw::Device &device);

        vkw::VertexBuffer<GUIVertex> m_vertices;

        static vkw::VertexBuffer<GUIVertex> m_create_vertex_buffer(vkw::Device &device, uint32_t size);

        vkw::IndexBuffer<VK_INDEX_TYPE_UINT16> m_indices;

        static vkw::IndexBuffer<VK_INDEX_TYPE_UINT16> m_create_index_buffer(vkw::Device &device, uint32_t size);

        ImDrawVert *m_vertices_mapped;
        ImDrawIdx *m_indices_mapped;

        std::map<TextureView, Material> m_materials{};

        std::map<TextureView, vkw::ColorImage2D> m_font_textures{};

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
