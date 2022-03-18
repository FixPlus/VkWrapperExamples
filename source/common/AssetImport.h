#ifndef TESTAPP_ASSETIMPORT_H
#define TESTAPP_ASSETIMPORT_H

#include <vector>
#include <string>
#include <vkw/Device.hpp>
#include <vkw/Image.hpp>
#include <vkw/Shader.hpp>

namespace TestApp {


    class AssetImporterBase {
    public:

        explicit AssetImporterBase(std::string rootDirectory) : m_root(std::move(rootDirectory)) {};

        virtual ~AssetImporterBase() = default;

    protected:
        bool try_open(std::string const &filename) const;

        std::vector<char> read_binary(std::string const &filename) const;

    private:
        std::string m_root;
    };

    class TextureLoader : public AssetImporterBase {
    public:
        TextureLoader(vkw::Device &device, std::string const &rootDirectory) :
                AssetImporterBase(rootDirectory), m_device(device) {}

        vkw::ColorImage2D loadTexture(const unsigned char *texture, size_t textureWidth, size_t textureHeight,
                                      VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                      VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_SAMPLED_BIT,
                                      VmaMemoryUsage = VMA_MEMORY_USAGE_GPU_ONLY) const;

        vkw::ColorImage2D
        loadTexture(std::string const &name, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_SAMPLED_BIT,
                    VmaMemoryUsage = VMA_MEMORY_USAGE_GPU_ONLY) const;

    private:
        std::reference_wrapper<vkw::Device> m_device;
    };

    class ShaderLoader : public AssetImporterBase {
    public:
        ShaderLoader(vkw::Device &device, std::string const &rootDirectory) :
                AssetImporterBase(rootDirectory), m_device(device) {}

        vkw::VertexShader loadVertexShader(std::string const &name) const;

        vkw::FragmentShader loadFragmentShader(std::string const &name) const;

    private:
        std::reference_wrapper<vkw::Device> m_device;
    };


}
#endif //TESTAPP_ASSETIMPORT_H
