#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TILE_SIZE 32
#define MAX_ROWS 500
#define MAX_COLS 500
#define SCREEN_WIDTH 1310
#define SCREEN_HEIGHT 1100
#define GRAVITY 1
#define JUMP_FORCE -18
#define TOTAL_LEVELS 3
#define SAVE_FILE "save.dat"
#define MAP_FILE "maps.txt"
#define PLAYER_SPEED 4
#define PLAYER_SCALE 1.5f

typedef struct {
    Vector2 position; 
    Vector2 velocity;
    bool onGround;
    Rectangle rect; 
} Player;

char maps[TOTAL_LEVELS][MAX_ROWS][MAX_COLS];
int mapRows[TOTAL_LEVELS];
int mapCols[TOTAL_LEVELS];
int trapTimers[MAX_ROWS][MAX_COLS];
int currentLevel = 0;

bool gameOver = false;
bool game_finish = false;


Texture2D start_image;

// Read the map from the text file
void read_map(const char *filename) 
{
    FILE *file = fopen(filename, "r");
    if (!file) 
    {
        perror("Failed to open map file");
        exit(1); // Stop program & have error
    }

    int level = 0, row = 0;
    char line[MAX_COLS + 2];

    while (fgets(line, sizeof(line), file)) 
    {
        if (line[0] == '-' && line[1] == '\n') 
        {
            mapRows[level] = row;
            row = 0;
            level++;

            if (level >= TOTAL_LEVELS)
                break;

            continue;
        }

        int len = (int)strlen(line); 

        if (line[len - 1] == '\n') 
            len--;

        mapCols[level] = len;

        for (int col = 0; col < len; col++) 
        {
            maps[level][row][col] = line[col];
            trapTimers[row][col] = 0;
        }

        row++;
    }
    mapRows[level] = row;
    fclose(file);
}

// Save current game level
void SaveGame(int level) 
{
    FILE *file = fopen(SAVE_FILE, "w");
    if (file) 
    {
        fprintf(file, "%d", level);
        fclose(file);
    }
}

int LoadSave() 
{
    FILE *file = fopen(SAVE_FILE, "r");

    int level = 0;
    
    if (file) 
    {
        fscanf(file, "%d", &level);
        fclose(file);
    }

    if (level < 0 || level >= TOTAL_LEVELS) 
        level = 0;
    
    return level;
}

void DrawMap(int level, Texture2D trap, Texture2D end, Texture2D backgorund, Texture2D ground) 
{
    // Background
    DrawTexturePro(backgorund,
    (Rectangle)
    {
        0, 
        0, 
        backgorund.width, 
        backgorund.height
    },

    (Rectangle)
    {
        0, 
        0, 
        mapCols[level] * TILE_SIZE, 
        mapRows[level] * TILE_SIZE
    },

    (Vector2){0, 0}, 0, WHITE);

    // loop through the map
    for (int y = 0; y < mapRows[level]; y++) 
    {
        for (int x = 0; x < mapCols[level]; x++) 
        {
            char tile = maps[level][y][x];

            Vector2 pos = { x * TILE_SIZE, y * TILE_SIZE };

            switch (tile) 
            {
                case '#':
                    DrawTexture(ground, pos.x, pos.y, WHITE);
                    break;
                case '^':
                    DrawTexture(trap, pos.x, pos.y, RED);
                    break;
                case '*': {
                    int timer = trapTimers[y][x];
                    int riseDuration = 3;
                    float offsetY = 0;

                    if (timer < riseDuration) 
                    {
                        offsetY = TILE_SIZE * (1.0f - (float)timer / riseDuration);
                    }

                    if (timer >= 0) 
                    {
                        DrawTexture(trap, pos.x, pos.y + offsetY, RED);
                    }
                }
                break;
                case 'F':
                    DrawTexture(end, pos.x, pos.y, WHITE);
                    break;
            }
        }
    }
}

void ResetPlayer(Player *p) 
{
    p->position = (Vector2){64, 64};
    p->velocity = (Vector2){0, 0};
    p->onGround = false; // jump
    p->rect = (Rectangle){
        p->position.x,
        p->position.y,
        TILE_SIZE * PLAYER_SCALE,
        TILE_SIZE * PLAYER_SCALE
    };
}

int main() 
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Game");
    InitAudioDevice();
    SetTargetFPS(60);

    read_map(MAP_FILE);
    currentLevel = LoadSave();

    Player player;
    ResetPlayer(&player);

    // Load images 
    start_image = LoadTexture("Image/Start.png");
    Texture2D playerTexture = LoadTexture("Image/Run.png");
    Texture2D trapture = LoadTexture("Image/Killer.png");
    Texture2D endture = LoadTexture("Image/End.png");
    Texture2D backgorundture = LoadTexture("Image/Background.png");
    Texture2D ground = LoadTexture("Image/Block.png");

    // Load sounds
    Sound gameOverSound = LoadSound("Sounds/Death.wav");
    Sound jumpSound = LoadSound("Sounds/DJump.wav");
    
    bool gameStarted = false; 

    int frameWidth = playerTexture.width / 5;
    int frameHeight = playerTexture.height;
    int currentFrame = 0;
    int framesCounter = 0;
    int framesSpeed = 8;

    while (!WindowShouldClose()) {
        // Player
        if (!gameOver) {
            player.velocity.x = 0;
            if (IsKeyDown(KEY_LEFT)) 
                player.velocity.x = -PLAYER_SPEED;
            
            if (IsKeyDown(KEY_RIGHT)) 
                player.velocity.x = PLAYER_SPEED;

            if (IsKeyPressed(KEY_UP) && player.onGround) {
                player.velocity.y = JUMP_FORCE;
                player.onGround = false;
                PlaySound(jumpSound);
            }

            // Player Animation
            if (player.velocity.x != 0) {
                framesCounter++;
                if (framesCounter >= (60 / framesSpeed)) {
                    framesCounter = 0;
                    currentFrame = (currentFrame + 1) % 4;
                }
            } else { // not moving
                currentFrame = 0;
                framesCounter = 0;
            }

            player.velocity.y += GRAVITY;
            player.position.x += player.velocity.x;
            player.rect.x = player.position.x;

            // left most tile (avoid going out of map)
            int minX = player.position.x / TILE_SIZE - 1;
            if (minX < 0) 
                minX = 0;

            // right most tile 
            int maxX = minX + 3;
            if (maxX > mapCols[currentLevel]) 
                maxX = mapCols[currentLevel];

            // Loop (tile could touch)
            // #
            // Left & Right
            for (int y = 0; y < mapRows[currentLevel]; y++) 
            {
                for (int x = minX; x < maxX; x++) 
                {
                    if (maps[currentLevel][y][x] == '#') 
                    {
                        Rectangle tileRect = 
                        {
                            x * TILE_SIZE, 
                            y * TILE_SIZE, 
                            TILE_SIZE, 
                            TILE_SIZE
                        }; 

                        if (CheckCollisionRecs(player.rect, tileRect))
                        {
                            if (player.velocity.x > 0)
                            {
                                player.position.x = tileRect.x - player.rect.width;
                            }
                            else if (player.velocity.x < 0)
                            {
                                player.position.x = tileRect.x + tileRect.width;
                            }
                            player.velocity.x = 0;
                            player.rect.x = player.position.x; 
                        }
                    }
                }
            }

            player.position.y += player.velocity.y; 
            player.rect.y = player.position.y; 

            bool landing = false;
            // true = player is on ground
            // false = player jump
            
            // Jump & Fall
            for (int y = 0; y < mapRows[currentLevel]; y++) 
            {
                for (int x = minX; x < maxX; x++) 
                {
                    if (maps[currentLevel][y][x] == '#') 
                    {
                        Rectangle tileRect = 
                        {
                            x * TILE_SIZE, 
                            y * TILE_SIZE, 
                            TILE_SIZE, 
                            TILE_SIZE
                        };

                        if (CheckCollisionRecs(player.rect, tileRect)) 
                        {
                            landing = true;
                            if (player.velocity.y > 0) 
                            {
                                player.position.y = tileRect.y - player.rect.height;
                                player.velocity.y = 0;
                                player.onGround = true;
                            } 
                            else if (player.velocity.y < 0) 
                            {
                                player.position.y = tileRect.y + tileRect.height;
                                player.velocity.y = 0;
                            }

                            player.rect.y = player.position.y;
                        }
                    }
                }
            }

            if (!landing) 
                player.onGround = false;

            
            // Traps 
            // ^ = trap, * = active trap, ! = trap that can be activated

            // !
            for (int y = 0; y < mapRows[currentLevel]; y++) 
            {
                for (int x = 0; x < mapCols[currentLevel]; x++) {
                    if (maps[currentLevel][y][x] == '!') 
                    {
                        Rectangle tileRect = 
                        {
                            x * TILE_SIZE, 
                            y * TILE_SIZE, 
                            TILE_SIZE, 
                            TILE_SIZE
                        };

                        if (CheckCollisionRecs(player.rect, tileRect)) {
                            maps[currentLevel][y][x] = '*';
                            trapTimers[y][x] = 0;
                        }
                    } 
                    else if (maps[currentLevel][y][x] == '*')
                    {
                        trapTimers[y][x]++;
                    }
                }
            }

            bool touch_trap = false;
            
            // ^ & *
            for (int y = 0; y < mapRows[currentLevel]; y++) 
            {
                for (int x = 0; x < mapCols[currentLevel]; x++) {
                    if (maps[currentLevel][y][x] == '^' ||
                        (maps[currentLevel][y][x] == '*' && trapTimers[y][x] >= 5)) 
                    {
                        Rectangle tileRect = 
                        {
                            x * TILE_SIZE,
                            y * TILE_SIZE, 
                            TILE_SIZE, 
                            TILE_SIZE
                        };

                        if (CheckCollisionRecs(player.rect, tileRect)) 
                            touch_trap = true;
                    }
                }
            }

            // DIE
            if (touch_trap && !gameOver) 
            {
                gameOver = true;
                PlaySound(gameOverSound);
            }

            bool reachedDoor = false;

            // End place (Final)
            for (int y = 0; y < mapRows[currentLevel]; y++) 
            {
                for (int x = 0; x < mapCols[currentLevel]; x++) {
                    if (maps[currentLevel][y][x] == 'F') 
                    {
                        Rectangle tileRect = 
                        {
                            x * TILE_SIZE, 
                            y * TILE_SIZE, 
                            TILE_SIZE, 
                            TILE_SIZE
                        };

                        if (CheckCollisionRecs(player.rect, tileRect)) 
                            reachedDoor = true;
                    }
                }
            }

            // Level +
            if (reachedDoor) {
                currentLevel++; // next level

                if (currentLevel >= TOTAL_LEVELS)
                {
                    gameOver = true;
                    game_finish = true; // all levels completed
                } 
                else // if have more level
                {
                    ResetPlayer(&player); // reset player at starting point
                }

                SaveGame(currentLevel);
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (!gameStarted) 
        {
            DrawTexturePro(start_image, 
                (Rectangle)
                {
                    0, 
                    0, 
                    start_image.width, 
                    start_image.height
                },

                (Rectangle)
                {
                    0,
                    0, 
                    SCREEN_WIDTH, 
                    SCREEN_HEIGHT
                }, 

                (Vector2)
                {0, 0}, 0.0f, WHITE);

            DrawText("Press SPACE to start", 100, SCREEN_HEIGHT - 150, 50, WHITE);

            if (IsKeyPressed(KEY_SPACE)) 
                gameStarted = true;

            EndDrawing();
            continue;
        }

        DrawMap(currentLevel, trapture, endture, backgorundture, ground);

        Rectangle sourceRec;

        // Draw player left & right
        if (player.velocity.x < 0) {
            // flip horizontally (left)
            sourceRec = (Rectangle){(currentFrame + 1) * frameWidth, 0, -frameWidth, frameHeight};
        } 
        else 
        {
            // normal (right & stand)
            sourceRec = (Rectangle){currentFrame * frameWidth, 0, frameWidth, frameHeight};
        }

        // Original position of the player
        Rectangle place_player = 
        {
            player.position.x, 
            player.position.y,
            TILE_SIZE * PLAYER_SCALE, 
            TILE_SIZE * PLAYER_SCALE
        };

        Vector2 origin = {0, 0}; // origin point (top left corner)

        DrawTexturePro
        (
            playerTexture, 
            sourceRec, 
            place_player, 
            origin, 
            0.0f, 
            WHITE
        );
        
        // Remarks at the top
        DrawText("Use Arrow Keys to move. Up to jump.", 250, 40, 30, DARKGRAY);

        DrawText(TextFormat("Level: %d", currentLevel + 1), 950, 40, 30, DARKGRAY);

        // Win
        if (gameOver) 
        {
            if (game_finish) 
            {
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK);
                DrawText("CONGRATULATIONS! YOU FINISHED ALL LEVELS", SCREEN_WIDTH / 2 - 500, SCREEN_HEIGHT / 2 - 100, 40, GOLD);
                DrawText("Press 'ENTER' to Restart from Level 1", SCREEN_WIDTH / 2 - 400, SCREEN_HEIGHT / 2, 40, WHITE);

                if (IsKeyPressed(KEY_ENTER)) 
                {
                    currentLevel = 0;
                    SaveGame(currentLevel);
                    ResetPlayer(&player);
                    gameOver = false;
                    game_finish = false;
                }
            } 
            else // game over (not finished)
            {
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.6f));
                DrawText("GAME OVER", SCREEN_WIDTH / 2 - 250, SCREEN_HEIGHT / 2 - 100, 80, RED);
                DrawText("Press 'R' to Try Again", SCREEN_WIDTH / 2 - 240, SCREEN_HEIGHT / 2 + 20, 40, WHITE);
                DrawText("Press 'ENTER' to Start From Level 1", SCREEN_WIDTH / 2 - 380, SCREEN_HEIGHT / 2 + 100, 40, WHITE);
            
                if (gameOver && IsKeyPressed(KEY_R)) 
                {
                    ResetPlayer(&player); // reset player
                    gameOver = false;

                    for (int y = 0; y < mapRows[currentLevel]; y++)
                    {
                        for (int x = 0; x < mapCols[currentLevel]; x++) 
                        {
                            // Reset traps
                            if (maps[currentLevel][y][x] == '*') 
                            {
                                maps[currentLevel][y][x] = '!';
                                trapTimers[y][x] = 0;
                            }
                        }
                    }
                }
                
                // ENTER - go back first level
                if (IsKeyPressed(KEY_ENTER)) 
                {
                    currentLevel = 0;
                    SaveGame(currentLevel);
                    ResetPlayer(&player);
                    gameOver = false;
                }
            }
        } 

        EndMode2D();
        EndDrawing();
    }

    UnloadTexture(playerTexture);
    UnloadTexture(trapture);
    UnloadTexture(endture);
    UnloadTexture(backgorundture);
    UnloadTexture(ground);
    UnloadTexture(start_image);

    UnloadSound(gameOverSound);

    CloseAudioDevice();
    CloseWindow();
    return 0;
}
