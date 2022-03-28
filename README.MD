# Application examples of VkWrapper

## About
Collection of applications using https://github.com/FixPlus/VkWrapper
## Prerequisites
* VkWrapper and it's dependencies: https://github.com/FixPlus/VkWrapper
* GLFW 3.3 or higher
* SPIRV-Tools: https://github.com/FixPlus/SPIRV-Tools (master branch)
* glslang: https://github.com/FixPlus/glslang/tree/linkage (linkage branch)

Note: you should use those forks, because some needed functionality hasn't got into Khronos yet
## Build
### 1. Install dependencies
First, you should download and build SPIRV-Tools and glslang (links are above). Replace glslangValidator inside Vulkan SDK on custom built one. Install SPIRV-Tools somewhere and provide path to it via cmake variable **SPIRV_TOOLS_INTALL_DIR**.
### 2. Download & build project
```
git clone https://github.com/FixPlus/VkWrapperExamples --recursive
cd VkWrapperExamples
mkdir build && cd build
cmake .. -DRENDERER_INSTALL_DIR=/path/to/VkWrapper/install -DSPIRV_TOOLS_INSTALL=/path/to/SPIRV-Tools
make install -j <num workers>
```
## Assets
Some examples might need additional assets like textures and/or 3d-models. Right now build configuration does not provide them automatically, so you might want to use your own. 