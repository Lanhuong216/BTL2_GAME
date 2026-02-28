cmake_minimum_required(VERSION 3.20)

project(hello-sdl2)
add_executable(app)

target_sources(app
PRIVATE
    main.cpp
)

# Set SDL2 paths manually
set(SDL2_INCLUDE_DIRS "C:/Users/hp/Downloads/Document/GAME/BTL2/vcpkg/installed/x64-windows/include")
set(SDL2_LIBRARIES "C:/Users/hp/Downloads/Document/GAME/BTL2/vcpkg/installed/x64-windows/lib/SDL2.lib")
set(SDL2_MAIN_LIBRARIES "C:/Users/hp/Downloads/Document/GAME/BTL2/vcpkg/installed/x64-windows/lib/manual-link/SDL2main.lib")
set(SDL2_IMAGE_LIBRARIES "C:/Users/hp/Downloads/Document/GAME/BTL2/vcpkg/installed/x64-windows/lib/SDL2_image.lib")
set(SDL2_TTF_LIBRARIES "C:/Users/hp/Downloads/Document/GAME/BTL2/vcpkg/installed/x64-windows/lib/SDL2_ttf.lib")

# Include directories
target_include_directories(app PRIVATE ${SDL2_INCLUDE_DIRS})

# Link libraries
target_link_libraries(app PRIVATE ${SDL2_MAIN_LIBRARIES} ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES})

# Compile definitions - removed SDL_MAIN_USE_CALLBACKS since we're using main()