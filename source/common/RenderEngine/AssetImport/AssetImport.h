#ifndef TESTAPP_ASSETIMPORT_H
#define TESTAPP_ASSETIMPORT_H

#include <vector>
#include <string>
#include <vkw/Device.hpp>
#include <vkw/Image.hpp>
#include <vkw/Shader.hpp>
#include <fstream>
#include "spirv-tools/linker.hpp"

namespace RenderEngine {


    class AssetImporterBase {
    public:

        explicit AssetImporterBase(std::string rootDirectory) : m_root(std::move(rootDirectory)) {};

        virtual ~AssetImporterBase() = default;

    protected:
        bool try_open(std::string const &filename) const;

        template<typename T = char>
        std::vector<T> read_binary(std::string const &filename) const{
            std::ifstream is(m_root + filename, std::ios::binary | std::ios::in | std::ios::ate);

            if (is.is_open()) {
                size_t size = is.tellg();
                is.seekg(0, std::ios::beg);

                std::vector<T> ret{};

                ret.resize(size / sizeof(T));
                is.read(reinterpret_cast<char*>(ret.data()), size);
                is.close();

                if (size == 0)
                    throw std::runtime_error(
                            "Asset import failed: could not read " + filename);

                return ret;
            }
            throw std::runtime_error("Asset import failed: cannot open file '" + filename + "'");
        }

    private:
        std::string m_root;
    };

    class TextureLoader : public AssetImporterBase {
    public:
        TextureLoader(vkw::Device &device, std::string const &rootDirectory) :
                AssetImporterBase(rootDirectory), m_device(device) {}

        vkw::Image<vkw::COLOR, vkw::I2D, vkw::SINGLE> loadTexture(const unsigned char *texture, size_t textureWidth, size_t textureHeight, int mipLevels = 1,
                                      VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                      VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_SAMPLED_BIT,
                                      VmaMemoryUsage = VMA_MEMORY_USAGE_GPU_ONLY) const;

        vkw::Image<vkw::COLOR, vkw::I2D, vkw::SINGLE>
        loadTexture(std::string const &name, int mipLevels = 1, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_SAMPLED_BIT,
                    VmaMemoryUsage = VMA_MEMORY_USAGE_GPU_ONLY) const;

    private:
        std::reference_wrapper<vkw::Device> m_device;
    };

    class ShaderImporter : public AssetImporterBase {
    public:
        ShaderImporter(vkw::Device &device, std::string const &rootDirectory);

        vkw::VertexShader loadVertexShader(std::string const &name) const;

        vkw::VertexShader loadVertexShader(std::string const &geometry, std::string const &projection) const;

        vkw::FragmentShader loadFragmentShader(std::string const &name) const;

        vkw::FragmentShader loadFragmentShader(std::string const &material, std::string const &lighting) const;

        vkw::ComputeShader loadComputeShader(std::string const& name) const;

    private:
        std::vector<uint32_t> m_link(std::vector<std::vector<uint32_t> const*> const& binaries) const;
        std::reference_wrapper<vkw::Device> m_device;
        spvtools::MessageConsumer m_message_consumer;
        spvtools::Context m_context;
        std::vector<uint32_t> m_general_vert{};
        std::vector<uint32_t> m_general_frag{};

    };


}
#endif //TESTAPP_ASSETIMPORT_H
