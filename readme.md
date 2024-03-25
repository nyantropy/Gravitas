How to compile:

LINUX

First, you need to install vulkan to your system:

sudo pacman -S vulkan-devel

This engine also needs glfw to create its windows:

sudo pacman -S glfw-wayland # for wayland users
sudo pacman -S glfw-x11 # for x11 users

As well as glm for simple matrix calculations:

sudo pacman -S glm

If you want shaders to be recompiled, you may also want to install shaderc:

sudo pacman -S shaderc