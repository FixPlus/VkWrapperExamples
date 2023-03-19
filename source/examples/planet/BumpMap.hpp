#ifndef TESTAPP_BUMPMAP_HPP
#define TESTAPP_BUMPMAP_HPP
#include "RenderEngine/Shaders/ShaderLoader.h"
#include <vkw/Image.hpp>

namespace TestApp {

class BumpMap : public vkw::Image<vkw::COLOR, vkw::I2D> {
public:
  BumpMap(vkw::Device &device,
          RenderEngine::ShaderLoaderInterface &shaderLoader,
          vkw::Image<vkw::COLOR, vkw::I2D> const &heightMap);
};

} // namespace TestApp
#endif // TESTAPP_BUMPMAP_HPP
