# Application examples of VkWrapper

## About
Collection of applications using https://github.com/FixPlus/VkWrapper
## Prerequisites
* Install VkWrapper and it's dependencies: https://github.com/FixPlus/VkWrapper
* GLFW 3.3 or higher
* glslang: https://github.com/FixPlus/glslang/tree/linkage (linkage branch)
  * Note: you should use this fork, because some needed functionality hasn't got into Khronos yet
## Build
### 1. Install dependencies
First, you should download and build glslang (link is above). Replace glslangValidator inside Vulkan SDK by custom-built one.
### 2. Download & build project
```
git clone https://github.com/FixPlus/VkWrapperExamples --recursive
cd VkWrapperExamples
mkdir build && cd build
cmake -S .. -B . 
make all -j <num workers>
```
## Assets
There is a submodule provided which contains a set of gltf sample models. However, there are still some assets that are not install automatically and may need to be manually placed in ./source/data folder in build directory