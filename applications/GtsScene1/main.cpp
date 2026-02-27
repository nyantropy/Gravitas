#include <iostream>
#include <filesystem>
#include <vector>

#include "GravitasEngine.hpp"
#include "DefaultScene.hpp"

int main()
{
    GravitasEngine engine = GravitasEngine();
    engine.registerScene("default", std::make_unique<DefaultScene>());
    engine.setActiveScene("default");
    engine.start();

    return EXIT_SUCCESS;
}
