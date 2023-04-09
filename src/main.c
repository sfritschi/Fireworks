#include "graphics.h"

int main(void)
{
    Graphics graphics = initGraphics();
    
    renderLoop(graphics);
    
    cleanupGraphics(graphics);
        
    return EXIT_SUCCESS;
}
