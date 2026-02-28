#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <fstream>
#include <iostream>
#include <ctime>
#include <cstdlib>
#include <cmath>
#include <string>
#define M_PI 3.14159265358979323846
using namespace std;

// Game states
enum GameState {
    WELCOME_SCREEN,
    GAME_MODE_SELECTION,
    GAME_PLAYING,
    WINNER_SCREEN
};

// Structure for game objects
struct GameObject {
    SDL_Rect rect;
    float rotation;
    float rotationSpeed;
    int size;
    bool isDestroyed; // Whether the object has been destroyed
    bool hasShadow; // Whether there's a shadow at this position
};

// Structure for tanks
struct Tank {
    SDL_Rect rect;
    bool isMoving;
    float speed;
    float rotation; // Rotation angle in degrees (body rotation)
    float gunRotation; // Gun rotation relative to body (-45 to +45)
    float gunRotationSpeed; // Speed of gun rotation
    bool gunRotatingRight; // Direction of gun rotation
    SDL_Rect gunRect; // Gun rectangle for rendering
    float gunScale; // Scale factor for gun size
    int currentAmmo; // Current ammo count (0-5)
    float reloadTimer; // Timer for reloading
    bool canShoot; // Whether tank can shoot
    int hp; // Health points
    bool isDestroyed; // Whether tank is destroyed
    bool hasShadow; // Whether tank has shadow
    int score; // Player score
    bool hasPower; // Whether tank has power-up (size reduction + speed boost)
    float powerTimer; // Timer for power-up duration
    float originalSpeed; // Original speed before power-up
    int originalWidth; // Original width before power-up
    int originalHeight; // Original height before power-up
    int explosionItemCount; // Number of explosion items the tank has
};

// Structure for bullets
struct Bullet {
    SDL_Rect rect;
    float speed;
    float rotation; // Direction of bullet
    bool active;
    int owner; // 0 for blue tank, 1 for red tank
    bool isExplosionBullet; // True for explosion bullets (instant kill)
};

// Structure for explosion effects
struct Explosion {
    SDL_Rect rect;
    float timer;
    float duration;
    bool active;
};

// Structure for power boxes
struct PowerBox {
    SDL_Rect rect;
    bool active;
    float spawnTimer;
    float disappearTimer; // Timer for auto-disappear
    int spawnCount; // Track spawn count to determine type
    int boxType; // 0=shield, 1=power-up
};

// Structure for bomb items
struct BombItem {
    bool active;
    int owner; // 0 for blue tank, 1 for red tank
    SDL_Rect rect;
    float scale; // Scale factor for bomb size
};

// Structure for defensive shields
struct Shield {
    bool active;
    float timer;
    float duration;
    int owner; // 0 for blue tank, 1 for red tank
};

// Forward declarations
void activatePowerUp(Tank* tank);
void activateShield(Shield* shield, int owner);

// Helper function to check if file exists
bool fileExists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

// Helper function to load texture from file
SDL_Texture* loadTexture(const std::string& path, SDL_Renderer* renderer) {
    std::cout << "[DEBUG] Attempting to load: " << path << std::endl;
    
    // Get current working directory
    char* basePath = SDL_GetBasePath();
    if (basePath) {
        std::cout << "[DEBUG] SDL Base Path: " << basePath << std::endl;
        SDL_free(basePath);
    }
    
    // Check if file exists
    if (!fileExists(path)) {
        std::cout << "[ERROR] File does not exist: " << path << std::endl;
        
        // Try alternate path
        std::string altPath = "../" + path;
        std::cout << "[DEBUG] Trying alternate path: " << altPath << std::endl;
        if (fileExists(altPath)) {
            std::cout << "[DEBUG] Found at alternate path!" << std::endl;
            return loadTexture(altPath, renderer);
        }
        return nullptr;
    }
    
    std::cout << "[DEBUG] File exists, loading..." << std::endl;
    
    SDL_Surface* loadedSurface = IMG_Load(path.c_str());
    if (!loadedSurface) {
        std::cout << "[ERROR] Unable to load image " << path << "! SDL_image Error: " << IMG_GetError() << std::endl;
        return nullptr;
    }
    
    std::cout << "[DEBUG] Surface loaded successfully, creating texture..." << std::endl;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, loadedSurface);
    SDL_FreeSurface(loadedSurface);
    
    if (!texture) {
        std::cout << "[ERROR] Unable to create texture from " << path << "! SDL Error: " << SDL_GetError() << std::endl;
    } else {
        std::cout << "[SUCCESS] Texture created successfully from " << path << std::endl;
    }
    
    return texture;
}

// Check if point is inside rectangle
bool isPointInRect(int x, int y, SDL_Rect rect) {
    return (x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h);
}

// Simple text rendering function (using SDL's built-in font)
void renderText(SDL_Renderer* renderer, const char* text, int x, int y, SDL_Color color) {
    // For now, we'll use a simple approach - in a real game you'd use SDL_ttf
    // This is a placeholder that will show the text as debug rectangles
    // You can replace this with proper text rendering later
}

// Function to generate random number between min and max
int random(int min, int max) {
    return min + rand() % (max - min + 1);
}

// Function to check if two rectangles overlap with minimum distance
bool checkCollision(SDL_Rect a, SDL_Rect b, int minDistance = 15) {
    // Expand rectangles by minDistance to ensure minimum spacing
    SDL_Rect expandedA = {a.x - minDistance, a.y - minDistance, a.w + 2*minDistance, a.h + 2*minDistance};
    SDL_Rect expandedB = {b.x - minDistance, b.y - minDistance, b.w + 2*minDistance, b.h + 2*minDistance};
    
    return (expandedA.x < expandedB.x + expandedB.w && expandedA.x + expandedA.w > expandedB.x && 
            expandedA.y < expandedB.y + expandedB.h && expandedA.y + expandedA.h > expandedB.y);
}

// Function to check if object collides with any existing objects
bool checkObjectCollision(SDL_Rect newRect, SDL_Rect* existingRects, int count, int minDistance = 15) {
    for (int i = 0; i < count; i++) {
        if (checkCollision(newRect, existingRects[i], minDistance)) {
            return true;
        }
    }
    return false;
}

// Function to check if tank collides with any game objects (grass or rocks)
bool checkTankCollisionWithObjects(SDL_Rect tankRect, GameObject* grassObjects, GameObject* rockObjects, int grassCount, int rockCount) {
    // Check collision with grass objects (ignore destroyed ones)
    for (int i = 0; i < grassCount; i++) {
        if (grassObjects[i].isDestroyed) continue; // Skip destroyed objects
        
        // Direct collision check without minDistance
        bool collisionX = tankRect.x < grassObjects[i].rect.x + grassObjects[i].rect.w && 
                         tankRect.x + tankRect.w > grassObjects[i].rect.x;
        bool collisionY = tankRect.y < grassObjects[i].rect.y + grassObjects[i].rect.h && 
                         tankRect.y + tankRect.h > grassObjects[i].rect.y;
        
        if (collisionX && collisionY) {
            std::cout << "[DEBUG] Tank collision with grass[" << i << "] at (" 
                     << grassObjects[i].rect.x << "," << grassObjects[i].rect.y 
                     << ") size " << grassObjects[i].rect.w << "x" << grassObjects[i].rect.h << std::endl;
            std::cout << "[DEBUG] Collision details - Tank: (" << tankRect.x << "," << tankRect.y 
                     << ") " << tankRect.w << "x" << tankRect.h << " Grass: (" 
                     << grassObjects[i].rect.x << "," << grassObjects[i].rect.y 
                     << ") " << grassObjects[i].rect.w << "x" << grassObjects[i].rect.h << std::endl;
            std::cout << "[DEBUG] CollisionX: " << collisionX << " CollisionY: " << collisionY << std::endl;
            return true;
        }
    }
    
    // Check collision with rock objects (ignore destroyed ones)
    for (int i = 0; i < rockCount; i++) {
        if (rockObjects[i].isDestroyed) continue; // Skip destroyed objects
        
        // Direct collision check without minDistance
        if (tankRect.x < rockObjects[i].rect.x + rockObjects[i].rect.w && 
            tankRect.x + tankRect.w > rockObjects[i].rect.x && 
            tankRect.y < rockObjects[i].rect.y + rockObjects[i].rect.h && 
            tankRect.y + tankRect.h > rockObjects[i].rect.y) {
            std::cout << "[DEBUG] Tank collision with rock[" << i << "] at (" 
                     << rockObjects[i].rect.x << "," << rockObjects[i].rect.y 
                     << ") size " << rockObjects[i].rect.w << "x" << rockObjects[i].rect.h << std::endl;
            return true;
        }
    }
    
    return false;
}

// Function to check collision between two tanks
bool checkTankCollision(SDL_Rect tank1, SDL_Rect tank2) {
    return (tank1.x < tank2.x + tank2.w && tank1.x + tank1.w > tank2.x && 
            tank1.y < tank2.y + tank2.h && tank1.y + tank1.h > tank2.y);
}

// Function to check if bullet collides with tank
bool checkBulletTankCollision(SDL_Rect bullet, SDL_Rect tank) {
    return (bullet.x < tank.x + tank.w && bullet.x + bullet.w > tank.x && 
            bullet.y < tank.y + tank.h && bullet.y + bullet.h > tank.y);
}

// Function to check if bullet collides with game object
bool checkBulletObjectCollision(SDL_Rect bullet, GameObject obj) {
    if (obj.isDestroyed) return false; // Don't collide with destroyed objects
    return (bullet.x < obj.rect.x + obj.rect.w && bullet.x + bullet.w > obj.rect.x && 
            bullet.y < obj.rect.y + obj.rect.h && bullet.y + bullet.h > obj.rect.y);
}

// Function to destroy game object and create shadow
void destroyGameObject(GameObject* obj) {
    if (!obj->isDestroyed) {
        obj->isDestroyed = true;
        obj->hasShadow = true;
        std::cout << "[DESTROY] Object destroyed at (" << obj->rect.x << "," << obj->rect.y << ")" << std::endl;
    }
}

// Function to update gun rotation
void updateGunRotation(Tank* tank, float deltaTime) {
    const float MIN_GUN_ROTATION = -45.0f;
    const float MAX_GUN_ROTATION = 45.0f;
    
    // Update gun rotation
    if (tank->gunRotatingRight) {
        tank->gunRotation += tank->gunRotationSpeed * deltaTime;
        if (tank->gunRotation >= MAX_GUN_ROTATION) {
            tank->gunRotation = MAX_GUN_ROTATION;
            tank->gunRotatingRight = false;
        }
    } else {
        tank->gunRotation -= tank->gunRotationSpeed * deltaTime;
        if (tank->gunRotation <= MIN_GUN_ROTATION) {
            tank->gunRotation = MIN_GUN_ROTATION;
            tank->gunRotatingRight = true;
        }
    }
}

// Function to update gun rectangle and scale
void updateGunRect(Tank* tank) {
    // Set gun scale (smaller than body)
    tank->gunScale = 0.5f; // 50% of original size
    
    // Calculate gun dimensions
    int gunWidth = (int)(tank->rect.w * tank->gunScale);
    int gunHeight = (int)(tank->rect.h * (tank->gunScale + 0.2f));
    
    // Set specific offsets for each direction
    int offsetX = 0;
    int offsetY = 0;
    
    // Set position based on tank rotation
    if (tank->rotation == 0.0f) {
        // Facing up - gun above center
        offsetX = 0;
        offsetY = -20;
    } else if (tank->rotation == 90.0f) {
        // Facing right - gun to the right
        offsetX = 25;
        offsetY =5;
    } else if (tank->rotation == 180.0f) {
        // Facing down - gun below center
        offsetX = 0;
        offsetY = tank->rect.h/2 -18;
    } else if (tank->rotation == 270.0f) {
        // Facing left - gun to the left
        offsetX = -12;
        offsetY =6;
    }
    
    // Position gun at center of tank body with specific offset
    tank->gunRect.x = tank->rect.x + (tank->rect.w - gunWidth) / 2 + offsetX;
    tank->gunRect.y = tank->rect.y + (tank->rect.h - gunHeight) / 2 + offsetY;
    tank->gunRect.w = gunWidth;
    tank->gunRect.h = gunHeight;
}

// Function to fire bullet from tank
void fireBullet(Bullet* bullet, Tank tank, int owner, bool isExplosionBullet = false) {
    bullet->active = true;
    bullet->owner = owner;
    bullet->rotation = tank.rotation + tank.gunRotation; // Combine body and gun rotation
    bullet->speed = 2.0f;
    bullet->isExplosionBullet = isExplosionBullet;
    
    // Position bullet at tank center
    bullet->rect.w = 8;
    bullet->rect.h = 10;
    bullet->rect.x = tank.rect.x + tank.rect.w/2 - bullet->rect.w/2;
    bullet->rect.y = tank.rect.y + tank.rect.h/2 - bullet->rect.h/2;
}

// Function to update bullet position
void updateBullet(Bullet* bullet) {
    if (!bullet->active) return;
    
    // Convert rotation to radians and calculate movement
    float radians = bullet->rotation * M_PI / 180.0f;
    bullet->rect.x += bullet->speed * sin(radians);
    bullet->rect.y -= bullet->speed * cos(radians);
    
    // Deactivate bullet if it goes off screen
    if (bullet->rect.x < 0 || bullet->rect.x > 960 || 
        bullet->rect.y < 0 || bullet->rect.y > 540) {
        bullet->active = false;
    }
}

// Function to update tank ammo system
void updateTankAmmo(Tank* tank, float deltaTime) {
    const float RELOAD_TIME = 0.5f; // 0.5 seconds
    const int MAX_AMMO = 5;
    
    // Update reload timer
    if (tank->currentAmmo < MAX_AMMO) {
        tank->reloadTimer += deltaTime;
        
        // If reload time is reached, add one ammo
        if (tank->reloadTimer >= RELOAD_TIME) {
            tank->currentAmmo++;
            tank->reloadTimer = 0.0f;
            std::cout << "[AMMO] Tank reloaded! Current ammo: " << tank->currentAmmo << "/" << MAX_AMMO << std::endl;
        }
    }
    
    // Tank can shoot if it has ammo
    tank->canShoot = (tank->currentAmmo > 0);
}

// Function to try to fire bullet (returns true if successful)
bool tryFireBullet(Bullet* bullet, Tank* tank, int owner) {
    if (!tank->canShoot || tank->currentAmmo <= 0) {
        return false;
    }
    
    // Consume ammo
    tank->currentAmmo--;
    tank->reloadTimer = 0.0f; // Reset reload timer
    
    // Fire the bullet
    fireBullet(bullet, *tank, owner);
    std::cout << "[AMMO] Tank fired! Remaining ammo: " << tank->currentAmmo << "/5" << std::endl;
    
    return true;
}

// Function to draw ammo bar
void drawAmmoBar(SDL_Renderer* renderer, Tank tank, int x, int y, int width, int height, SDL_Color color) {
    const int MAX_AMMO = 5;
    const float RELOAD_TIME = 0.5f;
    
    // Draw background bar
    SDL_Rect bgRect = {x, y, width, height};
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_RenderFillRect(renderer, &bgRect);
    
    // Draw ammo segments
    int segmentWidth = width / MAX_AMMO;
    for (int i = 0; i < tank.currentAmmo; i++) {
        SDL_Rect ammoRect = {x + i * segmentWidth, y, segmentWidth - 2, height};
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
        SDL_RenderFillRect(renderer, &ammoRect);
    }
    
    // Draw reloading segment if applicable
    if (tank.currentAmmo < MAX_AMMO && tank.reloadTimer > 0) {
        float reloadProgress = tank.reloadTimer / RELOAD_TIME;
        int reloadWidth = (int)(segmentWidth * reloadProgress);
        SDL_Rect reloadRect = {x + tank.currentAmmo * segmentWidth, y, reloadWidth, height};
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255); // Yellow for reloading
        SDL_RenderFillRect(renderer, &reloadRect);
    }
    
    // Draw border
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &bgRect);
}

// Function to draw HP bar
void drawHPBar(SDL_Renderer* renderer, Tank tank, int x, int y, int width, int height, SDL_Color color) {
    const int MAX_HP = 100;
    
    // Draw background bar
    SDL_Rect bgRect = {x, y, width, height};
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_RenderFillRect(renderer, &bgRect);
    
    // Draw HP bar
    int hpWidth = (int)((float)tank.hp / MAX_HP * width);
    if (hpWidth > 0) {
        SDL_Rect hpRect = {x, y, hpWidth, height};
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
        SDL_RenderFillRect(renderer, &hpRect);
    }
    
    // Draw border
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &bgRect);
}

// Function to create explosion effect
void createExplosion(Explosion* explosion, SDL_Rect position) {
    explosion->active = true;
    explosion->timer = 0.0f;
    explosion->duration = 1.0f; // 1 second duration
    explosion->rect.x = position.x + position.w/2 - 32; // Center explosion
    explosion->rect.y = position.y + position.h/2 - 32;
    explosion->rect.w = 64;
    explosion->rect.h = 64;
}

// Function to update explosion effect
void updateExplosion(Explosion* explosion, float deltaTime) {
    if (!explosion->active) return;
    
    explosion->timer += deltaTime;
    if (explosion->timer >= explosion->duration) {
        explosion->active = false;
    }
}

// Function to destroy tank
void destroyTank(Tank* tank) {
    if (!tank->isDestroyed) {
        tank->isDestroyed = true;
        tank->hasShadow = true;
        tank->hp = 0;
        std::cout << "[DESTROY] Tank destroyed!" << std::endl;
    }
}

// Function to draw score using number images
void drawScoreWithNumbers(SDL_Renderer* renderer, SDL_Texture* numberTextures[], int score, int x, int y, int digitWidth, int digitHeight) {
    // Convert score to string to get individual digits
    std::string scoreStr = std::to_string(score);
    
    // Calculate starting position to center the score
    int totalWidth = scoreStr.length() * digitWidth;
    int startX = x - totalWidth / 2;
    
    // Draw each digit
    for (size_t i = 0; i < scoreStr.length(); i++) {
        int digit = scoreStr[i] - '0'; // Convert char to int
        
        if (digit >= 0 && digit <= 9 && numberTextures[digit]) {
            SDL_Rect digitRect = {
                startX + i * digitWidth,
                y,
                digitWidth,
                digitHeight
            };
            SDL_RenderCopy(renderer, numberTextures[digit], NULL, &digitRect);
        }
    }
    
    std::cout << "[SCORE] Displaying score: " << score << std::endl;
}

// Function to spawn power box at random location
void spawnPowerBox(PowerBox* powerBox, GameObject* grassObjects, GameObject* rockObjects, int grassCount, int rockCount, Tank* blueTank, Tank* redTank) {
    if (powerBox->active) return; // Don't spawn if already active
    
    // Increment spawn count
    powerBox->spawnCount++;
    
    // Determine box type: 0=shield, 1=power-up, 2=explosion (every 3rd box)
    powerBox->boxType = powerBox->spawnCount % 2;
    
    // Find a random position that doesn't collide with objects or tanks
    int attempts = 0;
    do {
        powerBox->rect.x = random(50, 860);
        powerBox->rect.y = random(50, 440);
        powerBox->rect.w = 20;
        powerBox->rect.h = 20;
        attempts++;
    } while ((checkTankCollisionWithObjects(powerBox->rect, grassObjects, rockObjects, grassCount, rockCount) ||
              checkTankCollision(powerBox->rect, blueTank->rect) ||
              checkTankCollision(powerBox->rect, redTank->rect)) && attempts < 50);
    
    powerBox->active = true;
    powerBox->disappearTimer = 5.0f; // 5 seconds to disappear
    
    if (powerBox->boxType == 0) {
        std::cout << "[POWERBOX] Shield box spawned at (" << powerBox->rect.x << "," << powerBox->rect.y << ") - Defensive shield!" << std::endl;
    } else {
        std::cout << "[POWERBOX] Power-up box spawned at (" << powerBox->rect.x << "," << powerBox->rect.y << ") - Size reduction + Speed boost!" << std::endl;
    }
}
// Function to update power box spawning
void updatePowerBoxSpawning(PowerBox* powerBox, float deltaTime, GameObject* grassObjects, GameObject* rockObjects, int grassCount, int rockCount, Tank* blueTank, Tank* redTank) {
    const float SPAWN_INTERVAL = 3.0f; // 3 seconds
    
    if (powerBox->active) {
        // Update disappear timer
        powerBox->disappearTimer -= deltaTime;
        if (powerBox->disappearTimer <= 0.0f) {
            powerBox->active = false;
            std::cout << "[POWERBOX] Power box disappeared after 5 seconds!" << std::endl;
        }
    } else {
        powerBox->spawnTimer += deltaTime;
        if (powerBox->spawnTimer >= SPAWN_INTERVAL) {
            spawnPowerBox(powerBox, grassObjects, rockObjects, grassCount, rockCount, blueTank, redTank);
            powerBox->spawnTimer = 0.0f;
        }
    }
}

// Function to check if tank collects power box
bool checkPowerBoxCollection(PowerBox* powerBox, Tank* tank, Shield* shield, int tankOwner) {
    if (!powerBox->active || tank->isDestroyed) return false;
    
    // Check collision between tank and power box
    if (tank->rect.x < powerBox->rect.x + powerBox->rect.w && 
        tank->rect.x + tank->rect.w > powerBox->rect.x && 
        tank->rect.y < powerBox->rect.y + powerBox->rect.h && 
        tank->rect.y + tank->rect.h > powerBox->rect.y) {
        
        powerBox->active = false;
        
        if (powerBox->boxType == 0) {
            // Shield box
            activateShield(shield, tankOwner);
            std::cout << "[POWERBOX] Tank " << tankOwner << " collected shield box! Defensive shield activated!" << std::endl;
        } else {
            // Power-up box (size reduction + speed boost)
            activatePowerUp(tank);
            std::cout << "[POWERBOX] Tank collected power-up box! Size reduced, speed doubled!" << std::endl;
        } 
        return true;
    }
    return false;
}

// Function to activate shield
void activateShield(Shield* shield, int owner) {
    shield->active = true;
    shield->timer = 0.0f;
    shield->duration = 30.0f; // 10 seconds
    shield->owner = owner;
    std::cout << "[SHIELD] Shield activated for tank " << owner << std::endl;
}

// Function to update shield
void updateShield(Shield* shield, float deltaTime) {
    if (!shield->active) return;
    
    shield->timer += deltaTime;
    if (shield->timer >= shield->duration) {
        shield->active = false;
        std::cout << "[SHIELD] Shield expired" << std::endl;
    }
}

// Function to check if tank has active shield
bool hasActiveShield(Shield* shield, int owner) {
    return shield->active && shield->owner == owner;
}

// Function to reflect bullet
void reflectBullet(Bullet* bullet, Tank* targetTank) {
    // Reverse the bullet direction
    bullet->rotation += 180.0f;
    if (bullet->rotation >= 360.0f) {
        bullet->rotation -= 360.0f;
    }
    
    // Change ownership to the shielded tank
    bullet->owner = (bullet->owner == 0) ? 1 : 0;
    
    std::cout << "[REFLECT] Bullet reflected by shielded tank!" << std::endl;
}

// Function to activate power-up (size reduction + speed boost)
void activatePowerUp(Tank* tank) {
    if (!tank->hasPower) {
        tank->hasPower = true;
        tank->powerTimer = 15.0f; // 15 seconds duration
        
        // Store original values
        tank->originalSpeed = tank->speed;
        tank->originalWidth = tank->rect.w;
        tank->originalHeight = tank->rect.h;
        
        // Apply power-up effects
        tank->speed *= 2.0f; // Double speed
    
   
        std::cout << "[POWERUP] Tank activated power-up! Size reduced, speed doubled!" << std::endl;
    }
}

// Function to update power-up timer
void updatePowerUp(Tank* tank, float deltaTime) {
    if (tank->hasPower) {
        tank->powerTimer -= deltaTime;
        
        if (tank->powerTimer <= 0.0f) {
            // Restore original values
            tank->speed = tank->originalSpeed;
            
            // Restore original size and position
            tank->rect.x -= tank->originalWidth / 4;
            tank->rect.y -= tank->originalHeight / 4;
            tank->rect.w = tank->originalWidth;
            tank->rect.h = tank->originalHeight;
            
            tank->hasPower = false;
            std::cout << "[POWERUP] Tank power-up expired! Size and speed restored." << std::endl;
        }
    }
}

// Function to fire explosion bullet
bool fireExplosionBullet(Bullet* bullet, Tank* tank, int owner) {
    if (tank->explosionItemCount > 0) {
        tank->explosionItemCount--; // Use one explosion item
        fireBullet(bullet, *tank, owner, true); // Fire explosion bullet
        std::cout << "[EXPLOSION] Tank fired explosion bullet! Remaining items: " << tank->explosionItemCount << std::endl;
        return true;
    }
    return false;
}

// Function to update bomb items position
void updateBombItems(BombItem* bombItems, int maxItems, Tank* blueTank, Tank* redTank) {
    // Update blue tank bomb items
    for (int i = 0; i < blueTank->explosionItemCount && i < maxItems; i++) {
        bombItems[i].active = true;
        bombItems[i].owner = 0;
        bombItems[i].rect.x = blueTank->rect.x + blueTank->rect.w + 5 + (i * 20); // Position to the right
        bombItems[i].rect.y = blueTank->rect.y + blueTank->rect.h / 2 - 12; // Center vertically
        bombItems[i].rect.w = 24; // Larger bomb
        bombItems[i].rect.h = 24;
        bombItems[i].scale = 0.8f; // Slightly larger scale
    }
    
    // Update red tank bomb items
    for (int i = 0; i < redTank->explosionItemCount && i < maxItems; i++) {
        bombItems[i + maxItems/2].active = true;
        bombItems[i + maxItems/2].owner = 1;
        bombItems[i + maxItems/2].rect.x = redTank->rect.x + redTank->rect.w + 5 + (i * 20); // Position to the right
        bombItems[i + maxItems/2].rect.y = redTank->rect.y + redTank->rect.h / 2 - 12; // Center vertically
        bombItems[i + maxItems/2].rect.w = 24; // Larger bomb
        bombItems[i + maxItems/2].rect.h = 24;
        bombItems[i + maxItems/2].scale = 0.8f; // Slightly larger scale
    }
}

// Function to initialize game objects (grass and rocks) with fixed positions
void initializeGameObjects(GameObject* grassObjects, GameObject* rockObjects, int grassCount, int rockCount, SDL_Rect blueTankRect, SDL_Rect redTankRect) {
    // Fixed positions for grass objects (20 objects)
    SDL_Rect grassPositions[20] = {
        {50, 100, 30, 40}, {150, 200, 20, 40}, {250, 50, 20, 40}, {200, 300, 40, 50}, {350, 150, 20, 40},
        {400, 400, 30, 30}, {500, 250, 40, 40}, {550, 500, 20, 30}, {650, 100, 30, 30}, {700, 350, 20, 30},
        {800, 200, 20, 30}, {850, 450, 20, 50}, {900, 50, 20, 50}, {900, 500, 20, 50}, {750, 500, 20, 30},
        {600, 30, 20, 30}, {450, 500, 20, 30}, {300, 450, 20, 50}, {10, 500, 20, 30}, {940, 500, 20, 30 }
    };
    
    // Fixed positions for rock objects (15 objects)
    SDL_Rect rockPositions[15] = {
        {100, 50, 40, 80}, {300, 250, 50, 50}, {450, 100, 30, 50}, {500, 450, 30, 50}, {600, 200, 40, 40},
        {750, 30, 30, 30}, {800, 300, 30, 50}, {20, 400, 30, 30}, {100, 350, 30, 30}, {250, 150, 30, 45 },
        {350, 500, 30, 60}, {400, 20, 30, 30}, {650, 400, 30, 30}, {700, 500, 30, 60}, {900, 380, 30, 40}
    };
    
    // Initialize grass objects with fixed positions (keep original aspect ratio)
    for (int i = 0; i < grassCount; i++) {
        grassObjects[i].rect.x = grassPositions[i].x;
        grassObjects[i].rect.y = grassPositions[i].y;
        grassObjects[i].rect.w = grassPositions[i].w;
        grassObjects[i].rect.h = grassPositions[i].w; // Keep square aspect ratio
        grassObjects[i].size = grassObjects[i].rect.w;
        grassObjects[i].rotation = random(0, 360);
        grassObjects[i].isDestroyed = false;
        grassObjects[i].hasShadow = false;
    }
    
    // Initialize rock objects with fixed positions (keep original aspect ratio)
    for (int i = 0; i < rockCount; i++) {
        rockObjects[i].rect.x = rockPositions[i].x;
        rockObjects[i].rect.y = rockPositions[i].y;
        rockObjects[i].rect.w = rockPositions[i].w;
        rockObjects[i].rect.h = rockPositions[i].w; // Keep square aspect ratio
        rockObjects[i].size = rockObjects[i].rect.w;
        rockObjects[i].rotation = random(0, 360);
        rockObjects[i].isDestroyed = false;
        rockObjects[i].hasShadow = false;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "    GAME DEBUG LOG" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Print working directory info
    char* basePath = SDL_GetBasePath();
    if (basePath) {
        std::cout << "[INFO] Executable path: " << basePath << std::endl;
        SDL_free(basePath);
    }
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cout << "[ERROR] SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return -1;
    }
    std::cout << "[SUCCESS] SDL initialized" << std::endl;
    
    // Initialize SDL_image
    int imgFlags = IMG_INIT_PNG;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        std::cout << "[ERROR] SDL_image could not initialize! SDL_image Error: " << IMG_GetError() << std::endl;
        return -1;
    }
    std::cout << "[SUCCESS] SDL_image initialized" << std::endl;
    
    // Audio initialization commented out - SDL_mixer not available
    // if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
    //     std::cout << "[ERROR] SDL_mixer could not initialize! SDL_mixer Error: " << Mix_GetError() << std::endl;
    //     return -1;
    // }
    // std::cout << "[SUCCESS] SDL_mixer initialized" << std::endl;
    
    SDL_Window* window = SDL_CreateWindow("Game", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
                                         960, 540, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cout << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return -1;
    }
    
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cout << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return -1;
    }
    
    // Load welcome screen background
    SDL_Texture* welcomeBackground = loadTexture("resource/welcome_screen.png", renderer);
    if (!welcomeBackground) {
        std::cout << "Failed to load welcome screen background!" << std::endl;
        return -1;
    }
    
    // Load start button
    SDL_Texture* startButton = loadTexture("resource/start_button.png", renderer);
    if (!startButton) {
        std::cout << "Failed to load start button!" << std::endl;
        return -1;
    }
    
    // Load game mode background
    SDL_Texture* gameModeBackground = loadTexture("resource/gamemode_bg.png", renderer);
    if (!gameModeBackground) {
        std::cout << "Failed to load game mode background!" << std::endl;
        return -1;
    }
    
    // Load multiplayer buttons
    SDL_Texture* multiplayerButton = loadTexture("resource/multiplayer.png", renderer);
    if (!multiplayerButton) {
        std::cout << "Failed to load multiplayer button!" << std::endl;
        return -1;
    }
    
    SDL_Texture* multiplayerButtonHover = loadTexture("resource/multiplayer_hover.png", renderer);
    if (!multiplayerButtonHover) {
        std::cout << "Failed to load multiplayer hover button!" << std::endl;
        return -1;
    }
    

    
    // Load game assets
    SDL_Texture* gameBackground = loadTexture("resource/background.png", renderer);
    if (!gameBackground) {
        std::cout << "Failed to load game background!" << std::endl;
        return -1;
    }
    
    // Load tank body and gun textures
    SDL_Texture* blueBody = loadTexture("resource/blue-body.png", renderer);
    if (!blueBody) {
        std::cout << "Failed to load blue body!" << std::endl;
        return -1;
    }
    
    SDL_Texture* blueGun = loadTexture("resource/blue-gun.png", renderer);
    if (!blueGun) {
        std::cout << "Failed to load blue gun!" << std::endl;
        return -1;
    }
    
    SDL_Texture* redBody = loadTexture("resource/red-body.png", renderer);
    if (!redBody) {
        std::cout << "Failed to load red body!" << std::endl;
        return -1;
    }
    
    SDL_Texture* redGun = loadTexture("resource/red-gun.png", renderer);
    if (!redGun) {
        std::cout << "Failed to load red gun!" << std::endl;
        return -1;
    }
    
    SDL_Texture* grass = loadTexture("resource/grass.png", renderer);
    if (!grass) {
        std::cout << "Failed to load grass!" << std::endl;
        return -1;
    }
    
    SDL_Texture* rock = loadTexture("resource/rock.png", renderer);
    if (!rock) {
        std::cout << "Failed to load rock!" << std::endl;
        return -1;
    }
    
    // Load bullet textures
    SDL_Texture* blueBullet = loadTexture("resource/blue-bullet.png", renderer);
    if (!blueBullet) {
        std::cout << "Failed to load blue bullet!" << std::endl;
        return -1;
    }
    
    SDL_Texture* redBullet = loadTexture("resource/red-bullet.png", renderer);
    if (!redBullet) {
        std::cout << "Failed to load red bullet!" << std::endl;
        return -1;
    }
    
    // Load shadow textures (optional - use original textures if shadows don't exist)
    SDL_Texture* grassShadow = loadTexture("resource/shadow.png", renderer);
    if (!grassShadow) {
        std::cout << "Warning: Failed to load grass shadow, using original grass texture!" << std::endl;
        grassShadow = grass; // Use original grass texture as fallback
    }
    
    SDL_Texture* rockShadow = loadTexture("resource/shadow.png", renderer);
    if (!rockShadow) {
        std::cout << "Warning: Failed to load rock shadow, using original rock texture!" << std::endl;
        rockShadow = rock; // Use original rock texture as fallback
    }
    
    // Load explosion texture
    SDL_Texture* explosionTexture = loadTexture("resource/explosion.png", renderer);
    if (!explosionTexture) {
        std::cout << "Warning: Failed to load explosion texture!" << std::endl;
        // Create a simple colored rectangle as fallback
        explosionTexture = nullptr;
    }
    
    // Load tank shadow texture
    SDL_Texture* tankShadow = loadTexture("resource/shadow.png", renderer);
    if (!tankShadow) {
        std::cout << "Warning: Failed to load tank shadow, using blue body as fallback!" << std::endl;
        tankShadow = blueBody; // Use blue body as fallback
    }
    
    // Load shield tank textures
    SDL_Texture* blueShieldTank = loadTexture("resource/blue-shield.png", renderer);
    if (!blueShieldTank) {
        std::cout << "Failed to load blue-shield tank!" << std::endl;
        return -1;
    }
    
    SDL_Texture* redShieldTank = loadTexture("resource/red-shield.png", renderer);
    if (!redShieldTank) {
        std::cout << "Failed to load red-shield tank!" << std::endl;
        return -1;
    }
    
    // Load winner images
    SDL_Texture* blueWinImage = loadTexture("resource/blue-win.png", renderer);
    if (!blueWinImage) {
        std::cout << "Failed to load blue-win image!" << std::endl;
        return -1;
    }
    
    SDL_Texture* redWinImage = loadTexture("resource/red-win.png", renderer);
    if (!redWinImage) {
        std::cout << "Failed to load red-win image!" << std::endl;
        return -1;
    }
    
    // Load winner screen buttons
    SDL_Texture* playAgainButton = loadTexture("resource/play-again.png", renderer);
    if (!playAgainButton) {
        std::cout << "Failed to load play-again button!" << std::endl;
        return -1;
    }
    
    SDL_Texture* homeButton = loadTexture("resource/home-button.png", renderer);
    if (!homeButton) {
        std::cout << "Failed to load home button!" << std::endl;
        return -1;
    }
    
    // Load number images (0-9)
    SDL_Texture* numberTextures[10];
    for (int i = 0; i < 10; i++) {
        std::string numberPath = "resource/" + std::to_string(i) + ".png";
        numberTextures[i] = loadTexture(numberPath, renderer);
        if (!numberTextures[i]) {
            std::cout << "Failed to load number " << i << " image!" << std::endl;
            return -1;
        }
    }
    
    // Load bomb texture
    SDL_Texture* bombTexture = loadTexture("resource/bomb.png", renderer);
    if (!bombTexture) {
        std::cout << "Failed to load bomb texture!" << std::endl;
        return -1;
    }
    
    // Audio loading commented out - SDL_mixer not available
    // Mix_Music* backgroundMusic = Mix_LoadMUS("resource/background.mp3");
    // if (!backgroundMusic) {
    //     std::cout << "Warning: Failed to load background music!" << std::endl;
    // }
    // 
    // Mix_Chunk* explosionSound = Mix_LoadWAV("resource/explosion.mp3");
    // if (!explosionSound) {
    //     std::cout << "Warning: Failed to load explosion sound!" << std::endl;
    // }
    // 
    // Mix_Chunk* winnerSound = Mix_LoadWAV("resource/winner.mp3");
    // if (!winnerSound) {
    //     std::cout << "Warning: Failed to load winner sound!" << std::endl;
    // }
    
    // Load power box texture
    SDL_Texture* powerBoxTexture = loadTexture("resource/box.png", renderer);
    if (!powerBoxTexture) {
        std::cout << "Failed to load power box texture!" << std::endl;
        return -1;
    }
    
    
    // Get button dimensions
    int buttonWidth, buttonHeight;
    SDL_QueryTexture(startButton, NULL, NULL, &buttonWidth, &buttonHeight);
    
    // Position start button at center-bottom of screen
    SDL_Rect startButtonRect;
    startButtonRect.w = buttonWidth;
    startButtonRect.h = buttonHeight;
    startButtonRect.x = (960 - buttonWidth)/2 + 200;
    startButtonRect.y = (540 - buttonHeight) / 2;
    cout<<"startButtonRect.x: "<<startButtonRect.x<<endl;
    cout<<"startButtonRect.y: "<<startButtonRect.y<<endl;
    cout<<"startButtonRect.w: "<<startButtonRect.w<<endl;
    cout<<"startButtonRect.h: "<<startButtonRect.h<<endl;
    
    // Get multiplayer button dimensions
    int multiplayerWidth, multiplayerHeight;
    SDL_QueryTexture(multiplayerButton, NULL, NULL, &multiplayerWidth, &multiplayerHeight);
    
 
    
    // Create game mode button rectangles
    SDL_Rect multiplayerButtonRect;
    multiplayerButtonRect.w = multiplayerWidth;
    multiplayerButtonRect.h = multiplayerHeight;
    multiplayerButtonRect.x = (960 - multiplayerButtonRect.w) / 2;
    multiplayerButtonRect.y = 200;
    
 
    // Get winner screen button dimensions
    int playAgainWidth, playAgainHeight;
    SDL_QueryTexture(playAgainButton, NULL, NULL, &playAgainWidth, &playAgainHeight);
    
    int homeWidth, homeHeight;
    SDL_QueryTexture(homeButton, NULL, NULL, &homeWidth, &homeHeight);
    
    // Create winner screen button rectangles
    SDL_Rect playAgainButtonRect;
    playAgainButtonRect.w = playAgainWidth;
    playAgainButtonRect.h = playAgainHeight;
    playAgainButtonRect.x = (960 - playAgainButtonRect.w) / 2;
    playAgainButtonRect.y = 400;
    
    SDL_Rect homeButtonRect;
    homeButtonRect.w = homeWidth;
    homeButtonRect.h = homeHeight;
    homeButtonRect.x = (960 - homeButtonRect.w) / 2;
    homeButtonRect.y = 450;
    
    // Initialize tanks
    Tank blueTankObj, redTankObj;
    
    // Get tank dimensions from body texture
    int tankWidth, tankHeight;
    SDL_QueryTexture(blueBody, NULL, NULL, &tankWidth, &tankHeight);
    
    // Blue tank at bottom-left
    blueTankObj.rect.x = 50;
    blueTankObj.rect.y = 540 - tankHeight - 50;
    blueTankObj.rect.w = tankWidth;
    blueTankObj.rect.h = tankHeight;
    blueTankObj.isMoving = false;
    blueTankObj.speed = 2.0f;
    blueTankObj.rotation = 0.0f; // Start facing up
    blueTankObj.gunRotation = 0.0f; // Gun starts at center
    blueTankObj.gunRotationSpeed = 30.0f; // 30 degrees per second
    blueTankObj.gunRotatingRight = true; // Start rotating right
    blueTankObj.gunScale = 0.5f; // 60% of body size
    blueTankObj.currentAmmo = 5; // Start with full ammo
    blueTankObj.reloadTimer = 0.0f;
    blueTankObj.canShoot = true;
    blueTankObj.hp = 100; // Start with full HP
    blueTankObj.isDestroyed = false;
    blueTankObj.hasShadow = false;
    blueTankObj.score = 0; // Start with 0 score
    blueTankObj.hasPower = false; // No power-up initially
    blueTankObj.powerTimer = 0.0f;
    blueTankObj.originalSpeed = blueTankObj.speed;
    blueTankObj.originalWidth = tankWidth;
    blueTankObj.originalHeight = tankHeight;
    blueTankObj.explosionItemCount = 0; // No explosion items initially
    
    // Initialize gun rectangle
    updateGunRect(&blueTankObj);
    
    // Red tank at top-right (moved further from grass[12] at 900,50)
    redTankObj.rect.x = 960 - tankWidth - 100;
    redTankObj.rect.y = 50;
    redTankObj.rect.w = tankWidth;
    redTankObj.rect.h = tankHeight;
    redTankObj.isMoving = false;
    redTankObj.speed = 2.0f;
    redTankObj.rotation = 180.0f; // Start facing down
    redTankObj.gunRotation = 0.0f; // Gun starts at center
    redTankObj.gunRotationSpeed = 30.0f; // 30 degrees per second
    redTankObj.gunRotatingRight = true; // Start rotating right
    redTankObj.gunScale = 0.5f; // 50% of body size
    redTankObj.currentAmmo = 5; // Start with full ammo
    redTankObj.reloadTimer = 0.0f;
    redTankObj.canShoot = true;
    redTankObj.hp = 100; // Start with full HP
    redTankObj.isDestroyed = false;
    redTankObj.hasShadow = false;
    redTankObj.score = 0; // Start with 0 score
    redTankObj.hasPower = false; // No power-up initially
    redTankObj.powerTimer = 0.0f;
    redTankObj.originalSpeed = redTankObj.speed;
    redTankObj.originalWidth = tankWidth;
    redTankObj.originalHeight = tankHeight;
    redTankObj.explosionItemCount = 0; // No explosion items initially
    
    // Initialize gun rectangle
    updateGunRect(&redTankObj);
    
    // Initialize game objects
    const int GRASS_COUNT = 20;
    const int ROCK_COUNT = 15;
    GameObject grassObjects[GRASS_COUNT];
    GameObject rockObjects[ROCK_COUNT];
    
    // Initialize bullets
    const int MAX_BULLETS = 5;
    Bullet bullets[MAX_BULLETS];
    for (int i = 0; i < MAX_BULLETS; i++) {
        bullets[i].active = false;
    }
    
    // Initialize explosions
    const int MAX_EXPLOSIONS = 3;
    Explosion explosions[MAX_EXPLOSIONS];
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        explosions[i].active = false;
    }
    
    // Winner tracking
    int winner = -1; // -1 = no winner, 0 = blue tank wins, 1 = red tank wins
    
    // Initialize power box
    PowerBox powerBox;
    powerBox.active = false;
    powerBox.spawnTimer = 0.0f;
    powerBox.disappearTimer = 0.0f;
    powerBox.spawnCount = 0;
    powerBox.boxType = 0;
    
    // Initialize bomb items
    const int MAX_BOMB_ITEMS = 10;
    BombItem bombItems[MAX_BOMB_ITEMS];
    for (int i = 0; i < MAX_BOMB_ITEMS; i++) {
        bombItems[i].active = false;
        bombItems[i].owner = -1;
        bombItems[i].scale = 0.8f;
    }
    
    // Initialize shield
    Shield shield;
    shield.active = false;
    shield.timer = 0.0f;
    shield.duration = 0.0f;
    shield.owner = -1;
    
    // Seed random number generator
    srand(time(NULL));
    initializeGameObjects(grassObjects, rockObjects, GRASS_COUNT, ROCK_COUNT, blueTankObj.rect, redTankObj.rect);
    
    // Background music commented out - SDL_mixer not available
    // if (backgroundMusic) {
    //     Mix_PlayMusic(backgroundMusic, -1); // Loop indefinitely
    //     std::cout << "[AUDIO] Background music started" << std::endl;
    // }
    
    bool quit = false;
    GameState currentState = WELCOME_SCREEN;
    SDL_Event e;
    
    // Track key states for tank movement
    const Uint8* keystate = SDL_GetKeyboardState(NULL);
    bool blueTankKeysPressed = false;
    bool redTankKeysPressed = false;
    
    // Timing variables
    Uint32 lastTime = SDL_GetTicks();
    float deltaTime = 0.0f;
    
    while (!quit) {
        // Calculate delta time
        Uint32 currentTime = SDL_GetTicks();
        deltaTime = (currentTime - lastTime) / 1000.0f; // Convert to seconds
        lastTime = currentTime;
        
        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);
        bool showPointer = false;
        
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (currentState == WELCOME_SCREEN) {
                // Check if start button was clicked
                    if (isPointInRect(mouseX, mouseY, startButtonRect)) {
                        currentState = GAME_MODE_SELECTION;
                        std::cout << "Switched to Game Mode Selection!" << std::endl;
                    }
                }
                else if (currentState == GAME_MODE_SELECTION) {
                    // Check if multiplayer button was clicked
                    if (isPointInRect(mouseX, mouseY, multiplayerButtonRect)) {
                        currentState = GAME_PLAYING;
                        std::cout << "Multiplayer mode selected!" << std::endl;
                    }
                  
                }
                else if (currentState == WINNER_SCREEN) {
                    // Check if play again button was clicked
                    if (isPointInRect(mouseX, mouseY, playAgainButtonRect)) {
                        // Reset game state
                        currentState = GAME_PLAYING;
                        winner = -1;
                        
                        // Reset tanks
                            blueTankObj.rect.x = 50;
                            blueTankObj.rect.y = 540 - tankHeight - 50;
                            blueTankObj.rect.w = tankWidth;
                            blueTankObj.rect.h = tankHeight;
                            blueTankObj.isMoving = false;
                            blueTankObj.speed = 2.0f;
                            blueTankObj.rotation = 0.0f; // Start facing up
                            blueTankObj.gunRotation = 0.0f; // Gun starts at center
                            blueTankObj.gunRotationSpeed = 30.0f; // 30 degrees per second
                            blueTankObj.gunRotatingRight = true; // Start rotating right
                            blueTankObj.gunScale = 0.5f; // 60% of body size
                            blueTankObj.currentAmmo = 5; // Start with full ammo
                            blueTankObj.reloadTimer = 0.0f;
                            blueTankObj.canShoot = true;
                            blueTankObj.hp = 100; // Start with full HP
                            blueTankObj.isDestroyed = false;
                            blueTankObj.hasShadow = false;
                            blueTankObj.score = 0; // Start with 0 score
                            updateGunRect(&blueTankObj); // Update gun rectangle
                                                
                            redTankObj.rect.x = 960 - tankWidth - 100;
                            redTankObj.rect.y = 50;
                            redTankObj.rect.w = tankWidth;
                            redTankObj.rect.h = tankHeight;
                            redTankObj.isMoving = false;
                            redTankObj.speed = 2.0f;
                            redTankObj.rotation = 180.0f; // Start facing down
                            redTankObj.gunRotation = 0.0f; // Gun starts at center
                            redTankObj.gunRotationSpeed = 30.0f; // 30 degrees per second
                            redTankObj.gunRotatingRight = true; // Start rotating right
                            redTankObj.gunScale = 0.5f; // 50% of body size
                            redTankObj.currentAmmo = 5; // Start with full ammo
                            redTankObj.reloadTimer = 0.0f;
                            redTankObj.canShoot = true;
                            redTankObj.hp = 100; // Start with full HP
                            redTankObj.isDestroyed = false;
                            redTankObj.hasShadow = false;
                            redTankObj.score = 0; // Start with 0 score
                            updateGunRect(&redTankObj); // Update gun rectangle
                                                
                        // Reset bullets
                        for (int i = 0; i < MAX_BULLETS; i++) {
                            bullets[i].active = false;
                        }
                        
                        // Reset explosions
                        for (int i = 0; i < MAX_EXPLOSIONS; i++) {
                            explosions[i].active = false;
                        }
                        
                        // Reset game objects
                        initializeGameObjects(grassObjects, rockObjects, GRASS_COUNT, ROCK_COUNT, blueTankObj.rect, redTankObj.rect);
                        
                        std::cout << "Game restarted!" << std::endl;
                    }
                    // Check if home button was clicked
                    else if (isPointInRect(mouseX, mouseY, homeButtonRect)) {
                        currentState = WELCOME_SCREEN;
                        std::cout << "Returned to welcome screen!" << std::endl;
                    }
                }
            }
            else if (e.type == SDL_KEYDOWN && currentState == GAME_PLAYING) {
                // Blue tank shooting (F key) - only if not destroyed
                if (e.key.keysym.sym == SDLK_f && !blueTankObj.isDestroyed) {
                    // Find inactive bullet slot and try to fire
                    for (int i = 0; i < MAX_BULLETS; i++) {
                        if (!bullets[i].active) {
                            if (tryFireBullet(&bullets[i], &blueTankObj, 0)) {
                                break; // Successfully fired, exit loop
                            } else {
                                std::cout << "Blue tank out of ammo!" << std::endl;
                                break; // No ammo, exit loop
                            }
                        }
                    }
                }
                // Red tank shooting (/ key) - only if not destroyed
                else if (e.key.keysym.sym == SDLK_SLASH && !redTankObj.isDestroyed) {
                    // Find inactive bullet slot and try to fire
                    for (int i = 0; i < MAX_BULLETS; i++) {
                        if (!bullets[i].active) {
                            if (tryFireBullet(&bullets[i], &redTankObj, 1)) {
                                break; // Successfully fired, exit loop
                            } else {
                                std::cout << "Red tank out of ammo!" << std::endl;
                                break; // No ammo, exit loop
                            }
                        }
                    }
                }
                // Blue tank explosion power (J key)
                else if (e.key.keysym.sym == SDLK_j && !blueTankObj.isDestroyed) {
                    // Find inactive bullet slot and try to fire explosion bullet
                    for (int i = 0; i < MAX_BULLETS; i++) {
                        if (!bullets[i].active) {
                            if (fireExplosionBullet(&bullets[i], &blueTankObj, 0)) {
                                break; // Successfully fired explosion bullet
                            } else {
                                std::cout << "Blue tank has no explosion items!" << std::endl;
                                break;
                            }
                        }
                    }
                }
                // Red tank explosion power (. key)
                else if (e.key.keysym.sym == SDLK_PERIOD && !redTankObj.isDestroyed) {
                    // Find inactive bullet slot and try to fire explosion bullet
                    for (int i = 0; i < MAX_BULLETS; i++) {
                        if (!bullets[i].active) {
                            if (fireExplosionBullet(&bullets[i], &redTankObj, 1)) {
                                break; // Successfully fired explosion bullet
                            } else {
                                std::cout << "Red tank has no explosion items!" << std::endl;
                                break;
                            }
                        }
                    }
                }
            }
        }
        
        // Check for cursor pointer on buttons
        if (currentState == WELCOME_SCREEN && isPointInRect(mouseX, mouseY, startButtonRect)) {
            showPointer = true;
        }
        else if (currentState == GAME_MODE_SELECTION && 
                 (isPointInRect(mouseX, mouseY, multiplayerButtonRect) )) {
            showPointer = true;
        }
        else if (currentState == WINNER_SCREEN && 
                 (isPointInRect(mouseX, mouseY, playAgainButtonRect) || 
                  isPointInRect(mouseX, mouseY, homeButtonRect))) {
            showPointer = true;
        }
        
        // Set cursor
        if (showPointer) {
            SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND));
        } else {
            SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW));
        }
        
        SDL_RenderClear(renderer);
        
        if (currentState == WELCOME_SCREEN) {
            // Draw welcome screen
            SDL_RenderCopy(renderer, welcomeBackground, NULL, NULL);
            SDL_RenderCopy(renderer, startButton, NULL, &startButtonRect);
        }
        else if (currentState == GAME_MODE_SELECTION) {
            // Draw game mode selection screen
            SDL_RenderCopy(renderer, gameModeBackground, NULL, NULL);
            
            // Check if buttons are hovered
            bool multiplayerHovered = isPointInRect(mouseX, mouseY, multiplayerButtonRect);
            
            // Draw multiplayer button (normal or hover)
            if (multiplayerHovered) {
                SDL_RenderCopy(renderer, multiplayerButtonHover, NULL, &multiplayerButtonRect);
            } else {
                SDL_RenderCopy(renderer, multiplayerButton, NULL, &multiplayerButtonRect);
            }
            
           
        }
        else if (currentState == GAME_PLAYING) {
            // Update gun rotation for both tanks
            updateGunRotation(&blueTankObj, deltaTime);
            updateGunRotation(&redTankObj, deltaTime);
            
            // Update gun rectangles (position and size)
            updateGunRect(&blueTankObj);
            updateGunRect(&redTankObj);
            
            // Update ammo systems
            updateTankAmmo(&blueTankObj, deltaTime);
            updateTankAmmo(&redTankObj, deltaTime);
            
            // Update power-ups
            updatePowerUp(&blueTankObj, deltaTime);
            updatePowerUp(&redTankObj, deltaTime);
            
            // Update bomb items
            updateBombItems(bombItems, MAX_BOMB_ITEMS, &blueTankObj, &redTankObj);
            
            // Update explosions
            for (int i = 0; i < MAX_EXPLOSIONS; i++) {
                updateExplosion(&explosions[i], deltaTime);
            }
            
            // Update power box spawning
            updatePowerBoxSpawning(&powerBox, deltaTime, grassObjects, rockObjects, GRASS_COUNT, ROCK_COUNT, &blueTankObj, &redTankObj);
            
            // Update shield
            updateShield(&shield, deltaTime);
            
            // Check power box collection
            checkPowerBoxCollection(&powerBox, &blueTankObj, &shield, 0);
            checkPowerBoxCollection(&powerBox, &redTankObj, &shield, 1);
            
            // Check blue tank movement (WASD keys)
            blueTankKeysPressed = (keystate[SDL_SCANCODE_W] || keystate[SDL_SCANCODE_S] || 
                                  keystate[SDL_SCANCODE_A] || keystate[SDL_SCANCODE_D]);
            
            // Check red tank movement (Arrow keys)
            redTankKeysPressed = (keystate[SDL_SCANCODE_UP] || keystate[SDL_SCANCODE_DOWN] || 
                                 keystate[SDL_SCANCODE_LEFT] || keystate[SDL_SCANCODE_RIGHT]);
            
            // Update blue tank (only if not destroyed)
            if (blueTankKeysPressed && !blueTankObj.isDestroyed) {
                blueTankObj.isMoving = true;
                std::cout << "[DEBUG] Blue tank keys pressed - W:" << keystate[SDL_SCANCODE_W] 
                         << " S:" << keystate[SDL_SCANCODE_S] << " A:" << keystate[SDL_SCANCODE_A] 
                         << " D:" << keystate[SDL_SCANCODE_D] << std::endl;
                
                // Try to move blue tank based on WASD keys
                if (keystate[SDL_SCANCODE_W] && blueTankObj.rect.y > 0) {
                    blueTankObj.rotation = 0.0f; // Face up
                    SDL_Rect newBlueTankRect = blueTankObj.rect;
                    newBlueTankRect.y -= blueTankObj.speed;
                    // Check collision before applying movement
                    if (!checkTankCollisionWithObjects(newBlueTankRect, grassObjects, rockObjects, GRASS_COUNT, ROCK_COUNT) && 
                        !checkTankCollision(newBlueTankRect, redTankObj.rect)) {
                        blueTankObj.rect.y = newBlueTankRect.y;
                    }
                }
                if (keystate[SDL_SCANCODE_S] && blueTankObj.rect.y < 540 - blueTankObj.rect.h) {
                    blueTankObj.rotation = 180.0f; // Face down
                    SDL_Rect newBlueTankRect = blueTankObj.rect;
                    newBlueTankRect.y += blueTankObj.speed;
                    // Check collision before applying movement
                    if (!checkTankCollisionWithObjects(newBlueTankRect, grassObjects, rockObjects, GRASS_COUNT, ROCK_COUNT) && 
                        !checkTankCollision(newBlueTankRect, redTankObj.rect)) {
                        blueTankObj.rect.y = newBlueTankRect.y;
                    }
                }
                if (keystate[SDL_SCANCODE_A] && blueTankObj.rect.x > 0) {
                    blueTankObj.rotation = 270.0f; // Face left
                    SDL_Rect newBlueTankRect = blueTankObj.rect;
                    newBlueTankRect.x -= blueTankObj.speed;
                    // Check collision before applying movement
                    if (!checkTankCollisionWithObjects(newBlueTankRect, grassObjects, rockObjects, GRASS_COUNT, ROCK_COUNT) && 
                        !checkTankCollision(newBlueTankRect, redTankObj.rect)) {
                        blueTankObj.rect.x = newBlueTankRect.x;
                    }
                }
                if (keystate[SDL_SCANCODE_D] && blueTankObj.rect.x < 960 - blueTankObj.rect.w) {
                    blueTankObj.rotation = 90.0f; // Face right
                    SDL_Rect newBlueTankRect = blueTankObj.rect;
                    newBlueTankRect.x += blueTankObj.speed;
                    // Check collision before applying movement
                    if (!checkTankCollisionWithObjects(newBlueTankRect, grassObjects, rockObjects, GRASS_COUNT, ROCK_COUNT) && 
                        !checkTankCollision(newBlueTankRect, redTankObj.rect)) {
                        blueTankObj.rect.x = newBlueTankRect.x;
                    }
                }
            } else {
                blueTankObj.isMoving = false;
            }
            
            // Update red tank (only if not destroyed)
            if (redTankKeysPressed && !redTankObj.isDestroyed) {
                redTankObj.isMoving = true;
                std::cout << "[DEBUG] Red tank keys pressed - UP:" << keystate[SDL_SCANCODE_UP] 
                         << " DOWN:" << keystate[SDL_SCANCODE_DOWN] << " LEFT:" << keystate[SDL_SCANCODE_LEFT] 
                         << " RIGHT:" << keystate[SDL_SCANCODE_RIGHT] << std::endl;
                
                // Try to move red tank based on arrow keys
                if (keystate[SDL_SCANCODE_UP] && redTankObj.rect.y > 0) {
                    redTankObj.rotation = 0.0f; // Face up
                    SDL_Rect newRedTankRect = redTankObj.rect;
                    newRedTankRect.y -= redTankObj.speed;
                    // Check collision before applying movement
                    if (!checkTankCollisionWithObjects(newRedTankRect, grassObjects, rockObjects, GRASS_COUNT, ROCK_COUNT) && 
                        !checkTankCollision(newRedTankRect, blueTankObj.rect)) {
                        redTankObj.rect.y = newRedTankRect.y;
                    }
                }
                if (keystate[SDL_SCANCODE_DOWN] && redTankObj.rect.y < 540 - redTankObj.rect.h) {
                    redTankObj.rotation = 180.0f; // Face down
                    SDL_Rect newRedTankRect = redTankObj.rect;
                    newRedTankRect.y += redTankObj.speed;
                    // Check collision before applying movement
                    if (!checkTankCollisionWithObjects(newRedTankRect, grassObjects, rockObjects, GRASS_COUNT, ROCK_COUNT) && 
                        !checkTankCollision(newRedTankRect, blueTankObj.rect)) {
                        redTankObj.rect.y = newRedTankRect.y;
                    }
                }
                if (keystate[SDL_SCANCODE_LEFT] && redTankObj.rect.x > 0) {
                    redTankObj.rotation = 270.0f; // Face left
                    SDL_Rect newRedTankRect = redTankObj.rect;
                    newRedTankRect.x -= redTankObj.speed;
                    // Check collision before applying movement
                    if (!checkTankCollisionWithObjects(newRedTankRect, grassObjects, rockObjects, GRASS_COUNT, ROCK_COUNT) && 
                        !checkTankCollision(newRedTankRect, blueTankObj.rect)) {
                        redTankObj.rect.x = newRedTankRect.x;
                    }
                }
                if (keystate[SDL_SCANCODE_RIGHT] && redTankObj.rect.x < 960 - redTankObj.rect.w) {
                    redTankObj.rotation = 90.0f; // Face right
                    SDL_Rect newRedTankRect = redTankObj.rect;
                    newRedTankRect.x += redTankObj.speed;
                    // Check collision before applying movement
                    if (!checkTankCollisionWithObjects(newRedTankRect, grassObjects, rockObjects, GRASS_COUNT, ROCK_COUNT) && 
                        !checkTankCollision(newRedTankRect, blueTankObj.rect)) {
                        redTankObj.rect.x = newRedTankRect.x;
                    }
                }
            } else {
                redTankObj.isMoving = false;
            }
            
            // Update bullets
            for (int i = 0; i < MAX_BULLETS; i++) {
                if (bullets[i].active) {
                    updateBullet(&bullets[i]);
                    
                    // Check bullet collision with tanks
                    if (bullets[i].owner == 0) { // Blue tank bullet
                        if (checkBulletTankCollision(bullets[i].rect, redTankObj.rect) && !redTankObj.isDestroyed) {
                            // Check if red tank has active shield
                            if (hasActiveShield(&shield, 1)) {
                                // Reflect bullet
                                reflectBullet(&bullets[i], &redTankObj);
                            } else {
                                // Normal hit or explosion bullet
                                if (bullets[i].isExplosionBullet) {
                                    // Explosion bullet - 3x damage
                                    std::cout << "Blue tank hit red tank with explosion bullet! 3x damage!" << std::endl;
                                    redTankObj.hp -= 75; // 3x damage (25 * 3)
                                    blueTankObj.score += 300; // +300 points for explosion bullet hit
                                    
                                    // Explosion sound would play here
                                    
                                    // Check if red tank is destroyed
                                    if (redTankObj.hp <= 0) {
                                        destroyTank(&redTankObj);
                                        winner = 0; // Blue tank wins
                                        currentState = WINNER_SCREEN;
                                        
                                        // Winner sound would play here
                                    }
                                } else {
                                    // Normal bullet
                                    std::cout << "Blue tank hit red tank!" << std::endl;
                                    redTankObj.hp -= 25; // Damage
                                    blueTankObj.score += 100; // +100 points for hitting opponent
                                    
                            // Explosion sound would play here
                                    
                                    // Check if red tank is destroyed
                                    if (redTankObj.hp <= 0) {
                                        destroyTank(&redTankObj);
                                        winner = 0; // Blue tank wins
                                        currentState = WINNER_SCREEN;
                                        
                                        // Play winner sound
                                        // Winner sound would play here
                                    }
                                }
                                
                                // Create explosion effect
                                for (int j = 0; j < MAX_EXPLOSIONS; j++) {
                                    if (!explosions[j].active) {
                                        createExplosion(&explosions[j], redTankObj.rect);
                                        break;
                                    }
                                }
                                
                                bullets[i].active = false;
                            }
                        }
                    } else { // Red tank bullet
                        if (checkBulletTankCollision(bullets[i].rect, blueTankObj.rect) && !blueTankObj.isDestroyed) {
                            // Check if blue tank has active shield
                            if (hasActiveShield(&shield, 0)) {
                                // Reflect bullet
                                reflectBullet(&bullets[i], &blueTankObj);
                            } else {
                                // Normal hit or explosion bullet
                                if (bullets[i].isExplosionBullet) {
                                    // Explosion bullet - 3x damage
                                    std::cout << "Red tank hit blue tank with explosion bullet! 3x damage!" << std::endl;
                                    blueTankObj.hp -= 75; // 3x damage (25 * 3)
                                    redTankObj.score += 300; // +300 points for explosion bullet hit
                                    
                                    // Explosion sound would play here
                                    
                                    // Check if blue tank is destroyed
                                    if (blueTankObj.hp <= 0) {
                                        destroyTank(&blueTankObj);
                                        winner = 1; // Red tank wins
                                        currentState = WINNER_SCREEN;
                                        
                                        // Winner sound would play here
                                    }
                                } else {
                                    // Normal bullet
                                    std::cout << "Red tank hit blue tank!" << std::endl;
                                    blueTankObj.hp -= 25; // Damage
                                    redTankObj.score += 100; // +100 points for hitting opponent
                                    
                            // Explosion sound would play here
                                    
                                    // Check if blue tank is destroyed
                                    if (blueTankObj.hp <= 0) {
                                        destroyTank(&blueTankObj);
                                        winner = 1; // Red tank wins
                                        currentState = WINNER_SCREEN;
                                        
                                        // Play winner sound
                                        // Winner sound would play here
                                    }
                                }
                                
                                // Create explosion effect
                                for (int j = 0; j < MAX_EXPLOSIONS; j++) {
                                    if (!explosions[j].active) {
                                        createExplosion(&explosions[j], blueTankObj.rect);
                                        break;
                                    }
                                }
                                
                                bullets[i].active = false;
                            }
                        }
                    }
                    
                    // Check bullet collision with grass objects
                    for (int j = 0; j < GRASS_COUNT; j++) {
                        if (checkBulletObjectCollision(bullets[i].rect, grassObjects[j])) {
                            std::cout << "Bullet hit grass object at (" << grassObjects[j].rect.x 
                                     << "," << grassObjects[j].rect.y << ")" << std::endl;
                            destroyGameObject(&grassObjects[j]);
                            
                            // Add score based on bullet owner
                            if (bullets[i].owner == 0) {
                                blueTankObj.score += 10; // +10 points for destroying grass
                            } else {
                                redTankObj.score += 10; // +10 points for destroying grass
                            }
                            
                            // Play explosion sound
                            // Explosion sound would play here
                            
                            bullets[i].active = false;
                            break; // Bullet is destroyed, no need to check more objects
                        }
                    }
                    
                    // Check bullet collision with rock objects
                    for (int j = 0; j < ROCK_COUNT; j++) {
                        if (checkBulletObjectCollision(bullets[i].rect, rockObjects[j])) {
                            std::cout << "Bullet hit rock object at (" << rockObjects[j].rect.x 
                                     << "," << rockObjects[j].rect.y << ")" << std::endl;
                            destroyGameObject(&rockObjects[j]);
                            
                            // Add score based on bullet owner
                            if (bullets[i].owner == 0) {
                                blueTankObj.score += 10; // +10 points for destroying rock
                            } else {
                                redTankObj.score += 10; // +10 points for destroying rock
                            }
                            
                            // Play explosion sound
                            // Explosion sound would play here
                            
                            bullets[i].active = false;
                            break; // Bullet is destroyed, no need to check more objects
                        }
                    }
                }
            }
            
            // Draw game background
            SDL_RenderCopy(renderer, gameBackground, NULL, NULL);
            
            // Draw grass objects and shadows
            for (int i = 0; i < GRASS_COUNT; i++) {
                if (grassObjects[i].isDestroyed && grassObjects[i].hasShadow) {
                    // Draw shadow
                    SDL_RenderCopyEx(renderer, grassShadow, NULL, &grassObjects[i].rect, 
                                   grassObjects[i].rotation, NULL, SDL_FLIP_NONE);
                } else if (!grassObjects[i].isDestroyed) {
                    // Draw normal grass
                    SDL_RenderCopyEx(renderer, grass, NULL, &grassObjects[i].rect, 
                                   grassObjects[i].rotation, NULL, SDL_FLIP_NONE);
                }
            }
            
            // Draw rock objects and shadows
            for (int i = 0; i < ROCK_COUNT; i++) {
                if (rockObjects[i].isDestroyed && rockObjects[i].hasShadow) {
                    // Draw shadow
                    SDL_RenderCopyEx(renderer, rockShadow, NULL, &rockObjects[i].rect, 
                                   rockObjects[i].rotation, NULL, SDL_FLIP_NONE);
                } else if (!rockObjects[i].isDestroyed) {
                    // Draw normal rock
                    SDL_RenderCopyEx(renderer, rock, NULL, &rockObjects[i].rect, 
                                   rockObjects[i].rotation, NULL, SDL_FLIP_NONE);
                }
            }
            
            // Draw tanks with rotation (or shadows if destroyed)
            if (blueTankObj.isDestroyed && blueTankObj.hasShadow) {
                // Draw blue tank shadow
                SDL_RenderCopyEx(renderer, tankShadow, NULL, &blueTankObj.rect, 
                               blueTankObj.rotation, NULL, SDL_FLIP_NONE);
            } else if (!blueTankObj.isDestroyed) {
                // Draw blue tank body (normal or shield)
                if (hasActiveShield(&shield, 0)) {
                    // Draw blue shield body with scaled up size
                    float shieldScale = 1.15f; // 15% larger
                    SDL_Rect scaledRect = {
                        blueTankObj.rect.x - (int)(blueTankObj.rect.w * (shieldScale - 1.0f) / 2),
                        blueTankObj.rect.y - (int)(blueTankObj.rect.h * (shieldScale - 1.0f) / 2),
                        (int)(blueTankObj.rect.w * shieldScale),
                        (int)(blueTankObj.rect.h * shieldScale)
                    };
                    SDL_RenderCopyEx(renderer, blueShieldTank, NULL, &scaledRect, 
                                   blueTankObj.rotation, NULL, SDL_FLIP_NONE);
                } else {
                    // Draw normal blue body
                    SDL_RenderCopyEx(renderer, blueBody, NULL, &blueTankObj.rect, 
                                   blueTankObj.rotation, NULL, SDL_FLIP_NONE);
                }
                
                // Draw blue tank gun (always the same)
                SDL_RenderCopyEx(renderer, blueGun, NULL, &blueTankObj.gunRect, 
                               blueTankObj.rotation + blueTankObj.gunRotation, NULL, SDL_FLIP_NONE);
            }
            
            if (redTankObj.isDestroyed && redTankObj.hasShadow) {
                // Draw red tank shadow
                SDL_RenderCopyEx(renderer, tankShadow, NULL, &redTankObj.rect, 
                               redTankObj.rotation, NULL, SDL_FLIP_NONE);
            } else if (!redTankObj.isDestroyed) {
                // Draw red tank body (normal or shield)
                if (hasActiveShield(&shield, 1)) {
                    // Draw red shield body with scaled up size
                    float shieldScale = 1.15f; // 15% larger
                    SDL_Rect scaledRect = {
                        redTankObj.rect.x - (int)(redTankObj.rect.w * (shieldScale - 1.0f) / 2),
                        redTankObj.rect.y - (int)(redTankObj.rect.h * (shieldScale - 1.0f) / 2),
                        (int)(redTankObj.rect.w * shieldScale),
                        (int)(redTankObj.rect.h * shieldScale)
                    };
                    SDL_RenderCopyEx(renderer, redShieldTank, NULL, &scaledRect, 
                                   redTankObj.rotation, NULL, SDL_FLIP_NONE);
                } else {
                    // Draw normal red body
                    SDL_RenderCopyEx(renderer, redBody, NULL, &redTankObj.rect, 
                                   redTankObj.rotation, NULL, SDL_FLIP_NONE);
                }
                
                // Draw red tank gun (always the same)
                SDL_RenderCopyEx(renderer, redGun, NULL, &redTankObj.gunRect, 
                               redTankObj.rotation + redTankObj.gunRotation, NULL, SDL_FLIP_NONE);
            }
            
            // Draw bullets
            for (int i = 0; i < MAX_BULLETS; i++) {
                if (bullets[i].active) {
                    if (bullets[i].owner == 0) { // Blue tank bullet
                        SDL_RenderCopyEx(renderer, blueBullet, NULL, &bullets[i].rect, 
                                       bullets[i].rotation, NULL, SDL_FLIP_NONE);
                    } else { // Red tank bullet
                        SDL_RenderCopyEx(renderer, redBullet, NULL, &bullets[i].rect, 
                                       bullets[i].rotation, NULL, SDL_FLIP_NONE);
                    }
                }
            }
            
            // Draw power box with different visual for different types
            if (powerBox.active) {
                if (powerBox.boxType == 0) {
                    // Shield box - normal color
                    SDL_RenderCopy(renderer, powerBoxTexture, NULL, &powerBox.rect);
                } else {
                    // Power-up box - yellow tint
                    SDL_SetTextureColorMod(powerBoxTexture, 255, 255, 0);
                    SDL_RenderCopy(renderer, powerBoxTexture, NULL, &powerBox.rect);
                    SDL_SetTextureColorMod(powerBoxTexture, 255, 255, 255); // Reset
                } 
            }
            
            
            // Draw bomb items (bombs following tanks)
            for (int i = 0; i < MAX_BOMB_ITEMS; i++) {
                if (bombItems[i].active && bombTexture) {
                    SDL_Rect scaledRect = {
                        bombItems[i].rect.x,
                        bombItems[i].rect.y,
                        (int)(bombItems[i].rect.w * bombItems[i].scale),
                        (int)(bombItems[i].rect.h * bombItems[i].scale)
                    };
                    SDL_RenderCopy(renderer, bombTexture, NULL, &scaledRect);
                }
            }
            
            // Draw explosions
            for (int i = 0; i < MAX_EXPLOSIONS; i++) {
                if (explosions[i].active && explosionTexture) {
                    SDL_RenderCopy(renderer, explosionTexture, NULL, &explosions[i].rect);
                }
            }
            
            // Draw ammo bars and HP bars
            SDL_Color blueColor = {0, 100, 255, 255}; // Blue color
            SDL_Color redColor = {255, 100, 0, 255};  // Red color
            SDL_Color greenColor = {0, 255, 0, 255};  // Green color for HP
            
            // Blue tank ammo bar (bottom left)
            drawAmmoBar(renderer, blueTankObj, 10, 500, 200, 20, blueColor);
            
            // Blue tank HP bar (below ammo bar)
            drawHPBar(renderer, blueTankObj, 10, 500, 200, 15, greenColor);
            
            // Red tank ammo bar (top right)
            drawAmmoBar(renderer, redTankObj, 750, 10, 200, 20, redColor);
            
            // Red tank HP bar (below ammo bar)
            drawHPBar(renderer, redTankObj, 750, 35, 200, 15, greenColor);
            
            // Draw scores using number images
            drawScoreWithNumbers(renderer, numberTextures, blueTankObj.score, 30, 450, 20, 30);
            drawScoreWithNumbers(renderer, numberTextures, redTankObj.score, 900, 50, 20, 30);
            
            // Debug: Log tank positions every 60 frames (about 1 second at 60 FPS)
            static int frameCounter = 0;
            frameCounter++;
            if (frameCounter % 60 == 0) {
                std::cout << "[DEBUG] Tank positions - Blue: (" << blueTankObj.rect.x 
                         << "," << blueTankObj.rect.y << ") Red: (" << redTankObj.rect.x 
                         << "," << redTankObj.rect.y << ")" << std::endl;
            }
        }
        else if (currentState == WINNER_SCREEN) {
            // Draw winner screen
            SDL_RenderCopy(renderer, gameBackground, NULL, NULL);
            
            // Draw winner image
            SDL_Rect winnerImageRect = {330, 150, 300, 150}; // Center the image
            if (winner == 0) {
                // Blue tank wins
                SDL_RenderCopy(renderer, blueWinImage, NULL, &winnerImageRect);
                std::cout << "BLUE TANK WINS! Final Score: " << blueTankObj.score << std::endl;
            } else if (winner == 1) {
                // Red tank wins
                SDL_RenderCopy(renderer, redWinImage, NULL, &winnerImageRect);
                std::cout << "RED TANK WINS! Final Score: " << redTankObj.score << std::endl;
            }
            
            // Draw final score using number images
            if (winner == 0) {
                drawScoreWithNumbers(renderer, numberTextures, blueTankObj.score, 480, 250, 25, 35);
            } else if (winner == 1) {
                drawScoreWithNumbers(renderer, numberTextures, redTankObj.score, 480, 250, 25, 35);
            }
            
            // Draw buttons
            SDL_RenderCopy(renderer, playAgainButton, NULL, &playAgainButtonRect);
            SDL_RenderCopy(renderer, homeButton, NULL, &homeButtonRect);
            
            // Draw remaining explosions
            for (int i = 0; i < MAX_EXPLOSIONS; i++) {
                if (explosions[i].active && explosionTexture) {
                    SDL_RenderCopy(renderer, explosionTexture, NULL, &explosions[i].rect);
                }
            }
        }
        
        SDL_RenderPresent(renderer);
    }
    
    // Cleanup
    SDL_DestroyTexture(welcomeBackground);
    SDL_DestroyTexture(startButton);
    SDL_DestroyTexture(gameModeBackground);
    SDL_DestroyTexture(multiplayerButton);
    SDL_DestroyTexture(multiplayerButtonHover);

    SDL_DestroyTexture(gameBackground);
    SDL_DestroyTexture(blueBody);
    SDL_DestroyTexture(blueGun);
    SDL_DestroyTexture(redBody);
    SDL_DestroyTexture(redGun);
    SDL_DestroyTexture(grass);
    SDL_DestroyTexture(rock);
    SDL_DestroyTexture(blueBullet);
    SDL_DestroyTexture(redBullet);
    SDL_DestroyTexture(grassShadow);
    SDL_DestroyTexture(rockShadow);
    SDL_DestroyTexture(explosionTexture);
    SDL_DestroyTexture(tankShadow);
    SDL_DestroyTexture(blueWinImage);
    SDL_DestroyTexture(redWinImage);
    SDL_DestroyTexture(playAgainButton);
    SDL_DestroyTexture(homeButton);
    
    // Cleanup number textures
    for (int i = 0; i < 10; i++) {
        SDL_DestroyTexture(numberTextures[i]);
    }
    
    // Cleanup power box and shield textures
    SDL_DestroyTexture(powerBoxTexture);
    SDL_DestroyTexture(blueShieldTank);
    SDL_DestroyTexture(redShieldTank);
    SDL_DestroyTexture(bombTexture);
    
    // Audio cleanup commented out - SDL_mixer not available
    // Mix_FreeMusic(backgroundMusic);
    // Mix_FreeChunk(explosionSound);
    // Mix_FreeChunk(winnerSound);
    // Mix_CloseAudio();
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    
    return 0;
}