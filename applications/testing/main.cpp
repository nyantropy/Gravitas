#include <iostream>
#include "GravitasInclude.h"

int main() 
{
    Gravitas app = Gravitas();

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}