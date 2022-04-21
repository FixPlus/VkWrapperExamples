#ifndef TESTAPP_LANDSURFACE_H
#define TESTAPP_LANDSURFACE_H

#include "Grid.h"


namespace TestApp{

    class LandSurface : public TestApp::Grid, public RenderEngine::GeometryLayout {
    public:

        LandSurface(vkw::Device& device);
        struct UBO{
            glm::vec4 params = glm::vec4{20.0f, 100.0f, 0.0f, 0.0f};
            int harmonics = 1;
        } ubo;

        void update(){
            *m_geometry.m_ubo_mapped = ubo;
            m_geometry.m_ubo.flush();
        }
    private:

        struct Geometry : public RenderEngine::Geometry {
            vkw::UniformBuffer<UBO> m_ubo;
            UBO *m_ubo_mapped;

            Geometry(vkw::Device &device, LandSurface &surface);
        } m_geometry;

    protected:
        void preDraw(RenderEngine::GraphicsRecordingState &buffer, const GlobalLayout &globalLayout) override;
        std::pair<float, float> heightBounds() const override { return {-ubo.params.x * 2.0f, ubo.params.x * 2.0f};};
    };


    class LandMaterial : public RenderEngine::MaterialLayout {
    public:
        LandMaterial(vkw::Device &device, bool wireframe = false) : RenderEngine::MaterialLayout(device,
                                                                                                  RenderEngine::MaterialLayout::CreateInfo{.substageDescription=RenderEngine::SubstageDescription{.shaderSubstageName="land", .setBindings={
                                                                                                          vkw::DescriptorSetLayoutBinding{
                                                                                                                  0,
                                                                                                                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}}, .rasterizationState={
                                                                                                          VK_FALSE,
                                                                                                          VK_FALSE,
                                                                                                          wireframe
                                                                                                          ? VK_POLYGON_MODE_LINE
                                                                                                          : VK_POLYGON_MODE_FILL}, .depthTestState=vkw::DepthTestStateCreateInfo{
                                                                                                          VK_COMPARE_OP_LESS,
                                                                                                          true}, .maxMaterials=1}),
                                                                     m_material(device, *this) {

        };

        struct LandDescription {
            glm::vec4 color = glm::vec4(0.7f, 0.29f, 0.3f, 1.0f);
        } description;

        RenderEngine::Material const &get() const {
            return m_material;
        }

        void update() {
            *m_material.m_mapped = description;
            m_material.m_buffer.flush();
        }



    private:
        struct Material : public RenderEngine::Material {
            vkw::UniformBuffer<LandDescription> m_buffer;
            LandDescription *m_mapped;

            Material(vkw::Device &device, LandMaterial &waterMaterial);
        } m_material;


    };

    class LandSettings: public GridSettings{
    public:
        LandSettings(TestApp::GUIFrontEnd& gui, LandSurface& water, std::map<std::string, std::reference_wrapper<LandMaterial>> materials);
        LandMaterial& pickedMaterial() const{
            return m_materials.at(m_materialNames.at(m_pickedMaterial));
        }

    protected:

        void onGui() override;

    private:
        std::reference_wrapper<LandSurface> m_land;
        std::map<std::string, std::reference_wrapper<LandMaterial>> m_materials;
        std::vector<const char*> m_materialNames;
        int m_pickedMaterial = 0;

    };
}
#endif //TESTAPP_LANDSURFACE_H
