How to compile:

LINUX

First, you need to install vulkan to your system:

sudo pacman -S vulkan-devel

This engine also needs glfw to create its windows:

sudo pacman -S glfw-wayland # for wayland users
sudo pacman -S glfw-x11 # for x11 users

As well as glm for simple matrix calculations:

sudo pacman -S glm

Finally, we also need cmake to build the project:

sudo pacman -S cmake

If you want shaders to be recompiled, you may also want to install shaderc:

sudo pacman -S shaderc

WINDOWS

Download and install the latest version of the vulkan SDK.
Additionally, ensure that it is in your system PATH.

https://vulkan.lunarg.com/sdk/home#windows

Download and install the latest version of cmake.
NOTE: check the box to add it to your system PATH.

https://cmake.org/download/

Then you want to run vs2022cmake.bat in a console window of your choice.

And open the generated solution file with visual studio.