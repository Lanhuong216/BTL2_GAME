#include <SDL2/SDL.h>
#include <iostream>

int main() {
    std::cout << "Starting simple SDL test..." << std::endl;
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cout << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return -1;
    }
    std::cout << "SDL initialized successfully!" << std::endl;
    
    SDL_Window* window = SDL_CreateWindow("Test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
                                         640, 480, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cout << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return -1;
    }
    std::cout << "Window created successfully!" << std::endl;
    
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cout << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return -1;
    }
    std::cout << "Renderer created successfully!" << std::endl;
    
    // Simple game loop
    bool quit = false;
    SDL_Event e;
    
    std::cout << "Starting game loop..." << std::endl;
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
        }
        
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
        
        SDL_Delay(16); // ~60 FPS
    }
    
    std::cout << "Cleaning up..." << std::endl;
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    std::cout << "Test completed successfully!" << std::endl;
    return 0;
}
