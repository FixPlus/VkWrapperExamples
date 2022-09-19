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

using Complex2DTexture = vkw::ImageView<vkw::COLOR, vkw::V2D>;

class FFT {
public:
    explicit FFT(vkw::Device &device, RenderEngine::ShaderLoaderInterface &shaderLoader);

    void ftt(vkw::CommandBuffer &buffer, Complex2DTexture const &input, Complex2DTexture const &output);

private:
    class PermuteRow : public RenderEngine::Compute {
    public:
        PermuteRow(RenderEngine::ComputeLayout &layout, Complex2DTexture const &input, Complex2DTexture const &output);
    };

    class FFTRow : public RenderEngine::Compute {
    public:
        FFTRow(RenderEngine::ComputeLayout &layout, Complex2DTexture const &io);
    };

    class PermuteColumn : public RenderEngine::Compute {
    public:
        PermuteColumn(RenderEngine::ComputeLayout &layout, Complex2DTexture const &io);
    };

    class FFTColumn : public RenderEngine::Compute {
    public:
        FFTColumn(RenderEngine::ComputeLayout &layout, Complex2DTexture const &io);
    };

    RenderEngine::ComputeLayout m_permute_row_layout;
    RenderEngine::ComputeLayout m_fft_row_layout;
    RenderEngine::ComputeLayout m_permute_column_layout;
    RenderEngine::ComputeLayout m_fft_column_layout;

    std::map<std::pair<Complex2DTexture const *, Complex2DTexture const *>, std::tuple<PermuteRow, FFTRow, PermuteColumn, FFTColumn>> m_computes;

};

class WaveSurfaceTexture {
public:
    WaveSurfaceTexture(vkw::Device &device, RenderEngine::ShaderLoaderInterface &shaderLoader, uint32_t baseCascadeSize,
                       uint32_t cascades = 1);

    vkw::BasicImage<vkw::COLOR, vkw::I2D, vkw::ARRAY> &cascade(uint32_t cascadeIndex) {
        return m_cascades.at(cascadeIndex).texture();
    }



    struct GlobalParams {
        uint32_t Size = 256;
        float LengthScale = 100.0f;
        float CutoffHigh = 3000.0f;
        float CutoffLow = 0.1f;
    };

    struct UBO {
        glm::vec4 scale = glm::vec4{100.0f};
    } ubo;

    vkw::UniformBuffer<UBO> const& scalesBuffer() const{
        return m_scales_buffer;
    }

    GlobalParams &cascadeParams(uint32_t cascadeIndex) {
        return m_cascades.at(cascadeIndex).params();
    }

    size_t cascadesCount() const {
        return m_cascades.size();
    }

    void dispatch(vkw::CommandBuffer &buffer);

    void releaseOwnershipTo(vkw::CommandBuffer &buffer,
                            uint32_t computeFamilyIndex,
                            VkImageLayout incomingLayout,
                            VkAccessFlags incomingAccessMask,
                            VkPipelineStageFlags incomingStageMask) const {
        for (auto &cascade: m_cascades) {
            auto &texture = cascade.texture();

            VkImageMemoryBarrier transitLayout1{};
            transitLayout1.image = texture;
            transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            transitLayout1.pNext = nullptr;
            transitLayout1.oldLayout = incomingLayout;
            transitLayout1.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            transitLayout1.srcQueueFamilyIndex = buffer.queueFamily();
            transitLayout1.dstQueueFamilyIndex = computeFamilyIndex;
            transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            transitLayout1.subresourceRange.baseArrayLayer = 0;
            transitLayout1.subresourceRange.baseMipLevel = 0;
            transitLayout1.subresourceRange.layerCount = texture.layers();
            transitLayout1.subresourceRange.levelCount = 1;
            transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            transitLayout1.srcAccessMask = incomingAccessMask;

            buffer.imageMemoryBarrier(incomingStageMask, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, {transitLayout1});
        }
    }

    void acquireOwnership(vkw::CommandBuffer &buffer,
                          uint32_t incomingFamilyIndex,
                          VkImageLayout incomingLayout,
                          VkAccessFlags incomingAccessMask,
                          VkPipelineStageFlags incomingStageMask) const {
        for (auto &cascade: m_cascades) {
            auto &texture = cascade.texture();

            VkImageMemoryBarrier transitLayout1{};
            transitLayout1.image = texture;
            transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            transitLayout1.pNext = nullptr;
            transitLayout1.oldLayout = incomingLayout;
            transitLayout1.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            transitLayout1.srcQueueFamilyIndex = incomingFamilyIndex;
            transitLayout1.dstQueueFamilyIndex = buffer.queueFamily();
            transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            transitLayout1.subresourceRange.baseArrayLayer = 0;
            transitLayout1.subresourceRange.baseMipLevel = 0;
            transitLayout1.subresourceRange.layerCount = texture.layers();
            transitLayout1.subresourceRange.levelCount = 1;
            transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            transitLayout1.srcAccessMask = incomingAccessMask;

            buffer.imageMemoryBarrier(incomingStageMask, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, {transitLayout1});
        }
    }


    void releaseOwnership(vkw::CommandBuffer &buffer,
                          uint32_t acquireFamilyIndex,
                          VkImageLayout acquireLayout,
                          VkAccessFlags acquireAccessMask,
                          VkPipelineStageFlags acquireStageMask) const {
        for (auto &cascade: m_cascades) {
            auto &texture = cascade.texture();

            VkImageMemoryBarrier transitLayout1{};
            transitLayout1.image = texture;
            transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            transitLayout1.pNext = nullptr;
            transitLayout1.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            transitLayout1.newLayout = acquireLayout;
            transitLayout1.srcQueueFamilyIndex = buffer.queueFamily();
            transitLayout1.dstQueueFamilyIndex = acquireFamilyIndex;
            transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            transitLayout1.subresourceRange.baseArrayLayer = 0;
            transitLayout1.subresourceRange.baseMipLevel = 0;
            transitLayout1.subresourceRange.layerCount = texture.layers();
            transitLayout1.subresourceRange.levelCount = 1;
            transitLayout1.dstAccessMask = acquireAccessMask;
            transitLayout1.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;

            buffer.imageMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, acquireStageMask, {transitLayout1});
        }
    }

    void acquireOwnershipFrom(vkw::CommandBuffer &buffer,
                              uint32_t computeFamilyIndex,
                              VkImageLayout acquireLayout,
                              VkAccessFlags acquireAccessMask,
                              VkPipelineStageFlags acquireStageMask) const {
        for (auto &cascade: m_cascades) {
            auto &texture = cascade.texture();

            VkImageMemoryBarrier transitLayout1{};
            transitLayout1.image = texture;
            transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            transitLayout1.pNext = nullptr;
            transitLayout1.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            transitLayout1.newLayout = acquireLayout;
            transitLayout1.srcQueueFamilyIndex = computeFamilyIndex;
            transitLayout1.dstQueueFamilyIndex = buffer.queueFamily();
            transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            transitLayout1.subresourceRange.baseArrayLayer = 0;
            transitLayout1.subresourceRange.baseMipLevel = 0;
            transitLayout1.subresourceRange.layerCount = texture.layers();
            transitLayout1.subresourceRange.levelCount = 1;
            transitLayout1.dstAccessMask = acquireAccessMask;
            transitLayout1.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;

            buffer.imageMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, acquireStageMask, {transitLayout1});
        }
    }

    void computeSpectrum();

    struct SpectrumParameters {
        float scale = 0.1f;
        float angle = 0.0f;
        float spreadBlend = 0.3f;
        float swell = 0.0f;
        float alpha = 1.0f;
        float peakOmega = 1.0f;
        float gamma = 0.1f;
        float shortWavesFade = 0.05f;
        float GravityAcceleration = 9.8f;
        float Depth = 200.0f;
    } spectrumParameters;

    struct DynamicSpectrumParams {
        float time;
        float stretch;
    } dynamicSpectrumParams;

    void update(float deltaTime) {
        dynamicSpectrumParams.time += deltaTime;
        *m_dyn_params_mapped = dynamicSpectrumParams;
        m_dyn_params.flush();
        *m_scales_buffer_mapped = ubo;
        m_scales_buffer.flush();
    }


private:
    class WaveSurfaceTextureCascadeImageHandler {
    public:
        WaveSurfaceTextureCascadeImageHandler(vkw::Device &device, uint32_t cascadeSize);

        vkw::BasicImage<vkw::COLOR, vkw::I2D, vkw::ARRAY> &texture() {
            return m_surfaceTexture;
        }

        vkw::BasicImage<vkw::COLOR, vkw::I2D, vkw::ARRAY> const &texture() const {
            return m_surfaceTexture;
        }


        vkw::ImageView<vkw::COLOR, vkw::V2D> const& view(unsigned i) const{
            return m_views.at(i);
        }

    private:
        vkw::Image<vkw::COLOR, vkw::I2D, vkw::ARRAY> m_surfaceTexture;
        std::array<vkw::ImageView<vkw::COLOR, vkw::V2D>, 3> m_views;
    };

    class WaveSurfaceTextureCascade : public WaveSurfaceTextureCascadeImageHandler {
    public:
        WaveSurfaceTextureCascade(WaveSurfaceTexture &parent, TestApp::PrecomputeImageLayout &spectrumPrecomputeLayout,
                                  RenderEngine::ComputeLayout &dynSpectrumCompute,
                                  RenderEngine::ComputeLayout &combinerLayout,
                                  vkw::Device &device, vkw::UniformBuffer<SpectrumParameters> const &spectrumParams,
                                  vkw::UniformBuffer<DynamicSpectrumParams> const &dynParams,
                                  uint32_t cascadeSize, float cascadeScale);

        void precomputeStaticSpectrum(vkw::CommandBuffer &buffer);

        void computeSpectrum(vkw::CommandBuffer &commandBuffer);



        void combineFinalTexture(vkw::CommandBuffer &buffer);

        GlobalParams &params() {
            return m_spectrum_textures.globalParameters;
        }

    private:

        class SpectrumTextures : public vkw::Image<vkw::COLOR, vkw::I2D, vkw::ARRAY>, public TestApp::PrecomputeImage {
        public:
            SpectrumTextures(TestApp::PrecomputeImageLayout &spectrumPrecomputeLayout,
                             vkw::UniformBuffer<SpectrumParameters> const &spectrumParams, vkw::Device &device,
                             uint32_t cascadeSize, float cascadeScale);

            vkw::ImageView<vkw::COLOR, vkw::V2DA> const& view() const{
                return m_view;
            }
            GlobalParams globalParameters{};

            void update();

        private:

            class GaussTexture : public vkw::Image<vkw::COLOR, vkw::I2D> {
            public:
                GaussTexture(vkw::Device &device, uint32_t size);
                vkw::ImageView<vkw::COLOR, vkw::V2D> const& view() const{
                    return m_view;
                }
            private:
                vkw::ImageView<vkw::COLOR, vkw::V2D> m_view;
            } m_gauss_texture;

            vkw::UniformBuffer<GlobalParams> m_global_params;
            GlobalParams *m_globals_mapped;
            vkw::ImageView<vkw::COLOR, vkw::V2DA> m_view;
        } m_spectrum_textures;

        class DynamicSpectrumTextures : public vkw::Image<vkw::COLOR, vkw::I2D, vkw::ARRAY>, public RenderEngine::Compute {
        public:
            DynamicSpectrumTextures(RenderEngine::ComputeLayout &layout, SpectrumTextures &spectrum,
                                    vkw::UniformBuffer<DynamicSpectrumParams> const &params,
                                    vkw::Device &device, uint32_t cascadeSize);

            vkw::ImageView<vkw::COLOR, vkw::V2D> const& displacementView(unsigned i) const{
                return m_displacementViews.at(i);
            }
        private:
            vkw::ImageView<vkw::COLOR, vkw::V2DA> staticSpectrumView;
            vkw::ImageView<vkw::COLOR, vkw::V2D> displacementXY;
            vkw::ImageView<vkw::COLOR, vkw::V2D> displacementZXdx;
            vkw::ImageView<vkw::COLOR, vkw::V2D> displacementYdxZdx;
            vkw::ImageView<vkw::COLOR, vkw::V2D> displacementYdzZdz;

            std::array<vkw::ImageView<vkw::COLOR, vkw::V2D>, 8> m_displacementViews;
        } m_dynamic_spectrum;

        class FinalTextureCombiner : public RenderEngine::Compute {
        public:
            FinalTextureCombiner(RenderEngine::ComputeLayout &layout, DynamicSpectrumTextures &specTex,
                                 WaveSurfaceTextureCascade &final, vkw::Device &device);
        } m_texture_combiner;

        size_t cascade_size;
        std::reference_wrapper<vkw::Device> m_device;
        std::reference_wrapper<TestApp::PrecomputeImageLayout> m_layout;
    public:
        DynamicSpectrumTextures &spectrum() {
            return m_dynamic_spectrum;
        }
    };

    TestApp::PrecomputeImageLayout m_spectrum_precompute_layout;
    RenderEngine::ComputeLayout m_dyn_spectrum_gen_layout;
    RenderEngine::ComputeLayout m_combiner_layout;

    vkw::UniformBuffer<SpectrumParameters> m_spectrum_params;
    SpectrumParameters *m_params_mapped;

    vkw::UniformBuffer<UBO> m_scales_buffer;
    UBO* m_scales_buffer_mapped;

    std::vector<WaveSurfaceTextureCascade> m_cascades;
    std::reference_wrapper<vkw::Device> m_device;
    vkw::UniformBuffer<DynamicSpectrumParams> m_dyn_params;
    DynamicSpectrumParams *m_dyn_params_mapped;
    FFT m_fft;
};

class WaterSurface : public TestApp::Grid, public RenderEngine::GeometryLayout {
public:



    bool wireframe = false;

    void update(float deltaTime) {

    }

    WaterSurface(vkw::Device &device, WaveSurfaceTexture &texture);


private:

    class Geometry : public RenderEngine::Geometry {
    public:
        vkw::Sampler m_sampler;

        Geometry(vkw::Device &device, WaterSurface &surface, WaveSurfaceTexture &texture);

        static vkw::Sampler m_sampler_create(vkw::Device &device);
    private:
        std::vector<vkw::ImageView<vkw::COLOR, vkw::V2D>> m_displacements_view;
    } m_geometry;

protected:
    void preDraw(RenderEngine::GraphicsRecordingState &buffer) override;

    std::pair<float, float> heightBounds() const override { return {-5.0f, 5.0f}; };
};


class WaterMaterial : public RenderEngine::MaterialLayout {
public:
    WaterMaterial(vkw::Device &device, WaveSurfaceTexture &texture, bool wireframe = false);

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
    class Material : public RenderEngine::Material {
    public:
        vkw::UniformBuffer<WaterDescription> m_buffer;
        WaterDescription *m_mapped;

        Material(vkw::Device &device, WaterMaterial &waterMaterial, WaveSurfaceTexture &texture);

        static vkw::Sampler m_sampler_create(vkw::Device &device);

        vkw::Sampler m_sampler;
    private:
        std::vector<std::pair<vkw::ImageView<vkw::COLOR, vkw::V2D>, vkw::ImageView<vkw::COLOR, vkw::V2D>>> m_derivTurbViews;

    } m_material;

};

class WaveSettings : public TestApp::GridSettings {
public:

    WaveSettings(TestApp::GUIFrontEnd &gui, WaterSurface &water, WaveSurfaceTexture &texture,
                 std::map<std::string, std::reference_wrapper<WaterMaterial>> materials);

    WaterMaterial &pickedMaterial() const {
        return m_materials.at(m_materialNames.at(m_pickedMaterial));
    }

    bool needUpdateStaticSpectrum() const {
        return m_need_update_static_spectrum;
    }

    void resetUpdateSpectrum() {
        m_need_update_static_spectrum = false;
    }

    WaveSettings(WaveSettings &&another) noexcept;

    WaveSettings &operator=(WaveSettings &&another) noexcept;

protected:

    void onGui() override;

private:
    float m_gravity = 9.8f;
    float m_wind_speed = 100.0f;
    float m_fetch = 100000.0f;
    bool m_need_update_static_spectrum = true;
    std::reference_wrapper<WaterSurface> m_water;
    std::reference_wrapper<WaveSurfaceTexture> m_texture;
    std::map<std::string, std::reference_wrapper<WaterMaterial>> m_materials;
    std::vector<const char *> m_materialNames;
    int m_pickedMaterial = 0;

    void m_calculate_alpha();

    void m_calculate_peak_frequency();
};

#endif //TESTAPP_WATERSURFACE_H
