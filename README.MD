# Application examples of VkWrapper

## About
Collection of applications using https://github.com/FixPlus/VkWrapper
## Prerequisites
* VkWrapper and it's dependencies: https://github.com/FixPlus/VkWrapper
* GLFW 3.3 or higher
## Download & Build
```
git clone https://github.com/FixPlus/VkWrapperExamples --recursive
cd VkWrapperExamples
mkdir build && cd build
cmake .. -DRENDERER_INSTALL_DIR=/path/to/VkWrapper/install
make install -j <num workers>
```
## Assets
Some examples might need additional assets like textures and/or 3d-models. Right now build configuration does not provide them automatically, so you might want to use your own. 