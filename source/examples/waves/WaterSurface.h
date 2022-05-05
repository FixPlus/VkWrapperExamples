#ifndef TESTAPP_WATERSURFACE_H
#define TESTAPP_WATERSURFACE_H

#include <glm/glm.hpp>
#include <vkw/Device.hpp>
#include <vkw/DescriptorSet.hpp>
#include <vkw/DescriptorPool.hpp>
#include <common/Camera.h>
#include <vkw/Pipeline.hpp>
#include "common/Utils.h"
#include "RenderEngine/AssetImport/AssetImport.h"
#include "GlobalLayout.h"
#include "vkw/Sampler.hpp"
#include "RenderEngine/RecordingState.h"
#include "Grid.h"
#include "Precompute.h"

class WaveSurfaceTexture : public TestApp::PrecomputeImageLayout {
public:
    WaveSurfaceTexture(vkw::Device &device, RenderEngine::ShaderLoaderInterface &shaderLoader, uint32_t baseCascadeSize,
                       uint32_t cascades = 1);

    vkw::ColorImage2DArrayInterface &cascade(uint32_t cascadeIndex) {
        return m_cascades.at(cascadeIndex).texture();
    }

    size_t cascadesCount() const {
        return m_cascades.size();
    }

    void dispatch(vkw::CommandBuffer &buffer) const {
        for (auto &cascade: m_cascades)
            cascade.dispatch(buffer);
    }

    void releaseOwnershipTo(vkw::CommandBuffer &buffer,
                            uint32_t computeFamilyIndex,
                            VkImageLayout incomingLayout,
                            VkAccessFlags incomingAccessMask,
                            VkPipelineStageFlags incomingStageMask) const {
        for (auto &cascade: m_cascades)
            cascade.releaseOwnershipTo(buffer, computeFamilyIndex, incomingLayout, incomingAccessMask,
                                       incomingStageMask);
    }

    void acquireOwnership(vkw::CommandBuffer &buffer,
                          uint32_t incomingFamilyIndex,
                          VkImageLayout incomingLayout,
                          VkAccessFlags incomingAccessMask,
                          VkPipelineStageFlags incomingStageMask) const {
        for (auto &cascade: m_cascades)
            cascade.acquireOwnership(buffer, incomingFamilyIndex, incomingLayout, incomingStageMask, incomingStageMask);
    }


    void releaseOwnership(vkw::CommandBuffer &buffer,
                          uint32_t acquireFamilyIndex,
                          VkImageLayout acquireLayout,
                          VkAccessFlags acquireAccessMask,
                          VkPipelineStageFlags acquireStageMask) const {
        for (auto &cascade: m_cascades)
            cascade.releaseOwnership(buffer, acquireFamilyIndex, acquireLayout, acquireAccessMask, acquireStageMask);
    }

    void acquireOwnershipFrom(vkw::CommandBuffer &buffer,
                              uint32_t computeFamilyIndex,
                              VkImageLayout acquireLayout,
                              VkAccessFlags acquireAccessMask,
                              VkPipelineStageFlags acquireStageMask) const {
        for (auto &cascade: m_cascades)
            cascade.acquireOwnershipFrom(buffer, computeFamilyIndex, acquireLayout, acquireAccessMask,
                                         acquireStageMask);
    }

    void computeSpectrum() {
        for (auto &cascade: m_cascades)
            cascade.computeSpectrum();
    }

    struct SpectrumParameters {
        float scale = 10.0f;
        float angle = 0.0f;
        float spreadBlend = 0.3f;
        float swell = 0.0f;
        float alpha = 100.0f;
        float peakOmega = 1.0f;
        float gamma =1.0f;
        float shortWavesFade = 1.0f;
    } spectrumParameters;


private:
    class WaveSurfaceTextureCascadeImageHandler {
    public:
        WaveSurfaceTextureCascadeImageHandler(vkw::Device &device, uint32_t cascadeSize);

        vkw::ColorImage2DArrayInterface &texture() {
            return m_surfaceTexture;
        }

    private:
        vkw::ColorImage2DArray<2> m_surfaceTexture;
    };

    class WaveSurfaceTextureCascade : public WaveSurfaceTextureCascadeImageHandler, public TestApp::PrecomputeImage {
    public:
        WaveSurfaceTextureCascade(WaveSurfaceTexture &parent, TestApp::PrecomputeImageLayout &spectrumPrecomputeLayout,
                                  vkw::Device &device, vkw::UniformBuffer<SpectrumParameters> const& spectrumParams, uint32_t cascadeSize);

        void computeSpectrum();

    private:

        class SpectrumTextures : public vkw::ColorImage2DArray<2>, public TestApp::PrecomputeImage {
        public:
            SpectrumTextures(TestApp::PrecomputeImageLayout &spectrumPrecomputeLayout, vkw::UniformBuffer<SpectrumParameters> const& spectrumParams, vkw::Device &device,
                             uint32_t cascadeSize);

        private:

            struct GlobalParams {
                uint32_t Size = 256;
                float LengthScale = 1.0f;
                float CutoffHigh = 3000.0f;
                float CutoffLow = 3.0f;
                float GravityAcceleration = 9.8f;
                float Depth = 200.0f;
            } globalParameters;

            class GaussTexture : public vkw::ColorImage2D {
            public:
                GaussTexture(vkw::Device &device, uint32_t size);
            } m_gauss_texture;

            vkw::UniformBuffer<GlobalParams> m_global_params;
            GlobalParams* m_globals_mapped;
        } m_spectrum_textures;



        std::reference_wrapper<vkw::Device> m_device;
    };

    TestApp::PrecomputeImageLayout m_spectrum_precompute_layout;

    vkw::UniformBuffer<SpectrumParameters> m_spectrum_params;
    SpectrumParameters* m_params_mapped;

    std::vector<WaveSurfaceTextureCascade> m_cascades;
};

class WaterSurface : public TestApp::Grid, public RenderEngine::GeometryLayout {
public:

    struct UBO {
        glm::vec4 waves[4];
        glm::vec4 params = glm::vec4{9.8f, 0.0f, 0.0f, 0.0f};
        float time = 0.0f;
    } ubo;

    bool wireframe = false;

    void update(float deltaTime) {
        ubo.time += deltaTime;
        *m_geometry.m_ubo_mapped = ubo;
        m_geometry.m_ubo.flush();
    }

    WaterSurface(vkw::Device &device);


private:

    struct Geometry : public RenderEngine::Geometry {
        vkw::UniformBuffer<UBO> m_ubo;
        UBO *m_ubo_mapped;

        Geometry(vkw::Device &device, WaterSurface &surface);
    } m_geometry;

protected:
    void preDraw(RenderEngine::GraphicsRecordingState &buffer) override;

    std::pair<float, float> heightBounds() const override { return {-5.0f, 5.0f}; };
};


class WaterMaterial : public RenderEngine::MaterialLayout {
public:
    WaterMaterial(vkw::Device &device, bool wireframe = false) : RenderEngine::MaterialLayout(device,
                                                                                              RenderEngine::MaterialLayout::CreateInfo{.substageDescription=RenderEngine::SubstageDescription{.shaderSubstageName="water", .setBindings={
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

    struct WaterDescription {
        glm::vec4 deepWaterColor = glm::vec4(0.0f, 0.01f, 0.3f, 1.0f);
        float metallic = 1.0f;
        float roughness = 0.0f;
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
        vkw::UniformBuffer<WaterDescription> m_buffer;
        WaterDescription *m_mapped;

        Material(vkw::Device &device, WaterMaterial &waterMaterial);
    } m_material;

};

class WaveSettings : public TestApp::GridSettings {
public:

    WaveSettings(TestApp::GUIFrontEnd &gui, WaterSurface &water,
                 std::map<std::string, std::reference_wrapper<WaterMaterial>> materials);

    WaterMaterial &pickedMaterial() const {
        return m_materials.at(m_materialNames.at(m_pickedMaterial));
    }

    WaveSettings(WaveSettings &&another) noexcept;

    WaveSettings &operator=(WaveSettings &&another) noexcept;

protected:

    void onGui() override;

private:
    std::reference_wrapper<WaterSurface> m_water;
    std::map<std::string, std::reference_wrapper<WaterMaterial>> m_materials;
    std::vector<const char *> m_materialNames;
    int m_pickedMaterial = 0;
};

#endif //TESTAPP_WATERSURFACE_H
