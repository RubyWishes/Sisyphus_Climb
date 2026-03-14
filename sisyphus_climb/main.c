#include "raylib.h"
// Standard C headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdint.h> // Essential for uint32_t on Windows

// Platform-specific logic: Only include Windows.h on Windows and Mac headers on Mac
#if defined(_WIN32)
    #define NOGDI             // Basic optimization for windows.h
    #define NOUSER            
    #include <windows.h>

    // --- THE FIX: Remove Windows macros that conflict with Raylib ---
    #undef PlaySound   
    #undef Rectangle
    #undef CloseWindow
    #undef ShowCursor
    #undef DrawText
#elif defined(__APPLE__)
    #include <libgen.h>
    #include <mach-o/dyld.h>
    #include <unistd.h>
#endif


// 游戏常量

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define TOTAL_WORDS 1000          
#define STORY_TRIGGER 250         
#define INITIAL_Demon_SPEED 0
#define STONE_MOVE_SPEED_X 2      
#define STONE_MOVE_SPEED_Y 1      
#define BACKGROUND_SCROLL_SPEED_X  5.0f   // 横向速度（越大越快）
#define BACKGROUND_SCROLL_SPEED_Y -3.0f  // 纵向速度（负数向上，绝对值越大越快）
#define CLIMB_BOUNDARY_Y 100      
#define TOTAL_TARGET 1000    // 1000正确字目标
#define MAX_CHOICES 4        // 剧情最大选择数

// 游戏状态枚举
typedef enum {
    GAME_STATE_COVER,     
    GAME_STATE_SETTINGS,  
    GAME_STATE_PLAYING,   
    GAME_STATE_STORY,     
    GAME_STATE_GAMEOVER,  
    GAME_STATE_EXIT       
} GameState;

typedef enum {
    STORY_TYPE_NONE,
    STORY_TYPE_DIALOG,       // 纯对话剧情
    STORY_TYPE_CHOICE        // 选择分支剧情
} StoryType;

// 剧情数据结构体
typedef struct {
    StoryType type;          // 剧情类型
    const char* text;        // 剧情文本
    const char* choices[MAX_CHOICES]; // 选择项
    int choiceCount;         // 实际选择数
    int currentChoice;       // 当前选中的选项
    float timer;             // 剧情展示时长（秒）
    bool isDone;             // 剧情是否完成
} StoryData;

// 剧情节点结构体
typedef struct {
    int wordProgress;             
    char storyText[512];          
    char option1[128];            
    char option2[128];            
    float DemonSpeedMultiplier;   
    int endingType;               
} StoryNode;

// 统一的游戏数据结构体（修复重复定义问题）
typedef struct {
    Texture2D coverImg;
    Texture2D bgImg;
    Texture2D sisyphusImg;
    Texture2D DemonImg;
    Texture2D stoneImg;
    Texture2D uiBarImg;
    
    Sound typingSound;
    Music bgmCover;
    Music bgmGame;
    Music bgmStory;
    
    Vector2 sisyphusPos;
    Vector2 DemonPos;
    Vector2 stonePos;
    float bgScrollOffset;       
    float bgScrollOffsetY;   
    
    char currentWord[64];         
    char inputBuffer[64];         
    int wordIndex;                
    int totalTyped;               
    int correctTyped;             
    float gameTime;               
    float DemonSpeed;             
    bool isDemonCatch;            
    
    float volume;                 
    Rectangle progressBar;        
    StoryNode storyNodes[4];      
    int currentStoryNode;         
    int selectedOption;           
    int endingResult;             
    
    char** wordList;              
    int wordListCount;

    // 新增：剧情相关补充字段
    StoryData story;             
    bool isStoryActive;          
    float monsterSpeed;          
    float monsterAccel;          
    Camera2D camera;             
    float cameraSpeed; 
    
    // 恶魔激活与计时（新增）
    bool isDemonActivated;    // 恶魔是否激活（玩家打字后）
    bool isGameStarted;       // 游戏是否开始（计时开关）

    bool storyTriggered[4];       // 标记剧情是否已触发
} GameData;

// 获取可执行文件所在目录
const char* GetExecutableDir() {
    static char exePath[1024] = {0};
    if (exePath[0] != '\0') return exePath;

    uint32_t bufSize = sizeof(exePath);
    if (_NSGetExecutablePath(exePath, &bufSize) == 0) {
        char* dir = dirname(exePath);
        strcpy(exePath, dir);
        printf("✅ 可执行文件目录：%s\n", exePath);
    } else {
        strcpy(exePath, "/Users/boya/Documents/sisyphus_climb");
        printf("⚠️ 获取可执行路径失败，使用默认路径：%s\n", exePath);
    }
    return exePath;
}

// 函数声明
void InitGameData(GameData* game);
void LoadGameResources(GameData* game);
void UnloadGameResources(GameData* game);
void LoadWordList(GameData* game);
void InitStoryNodes(GameData* game);
void UpdateGameCover(GameData* game, GameState* currentState);
void UpdateGameSettings(GameData* game, GameState* currentState);
void UpdateGamePlaying(GameData* game, GameState* currentState, float deltaTime);
void UpdateGameStory(GameData* game, GameState* currentState);
void UpdateGameOver(GameData* game, GameState* currentState);
void DrawGameCover(GameData* game);
void DrawGameSettings(GameData* game);
void DrawGamePlaying(GameData* game);
void DrawGameStory(GameData* game);
void DrawGameOver(GameData* game);
void SpawnNewWord(GameData* game);
bool CheckDemonCatch(GameData* game);

// ========== 补充缺失的函数声明 ==========
float Lerp(float start, float end, float amount);
float Vector2Length(Vector2 v);
Vector2 Vector2Normalize(Vector2 v);
bool CheckDemonCatch(GameData* game);
void InitGameData(GameData* game);
void UpdateGamePlaying(GameData* game, GameState* currentState, float deltaTime);
void DrawGamePlaying(GameData* game);

// ========== 手动实现缺失的数学函数 ==========
// 插值函数（Lerp）
float Lerp(float start, float end, float amount) {
    return start + (end - start) * amount;
}

// 计算Vector2长度（模长）
float Vector2Length(Vector2 v) {
    return sqrtf(v.x*v.x + v.y*v.y);
}

// Vector2归一化（单位向量）
Vector2 Vector2Normalize(Vector2 v) {
    Vector2 result = {0.0f, 0.0f};
    float length = Vector2Length(v);
    if (length > 0.00001f) {
        result.x = v.x / length;
        result.y = v.y / length;
    }
    return result;
}

int main(void) {
    // 初始化窗口 + 直接全屏（解决展开窗口问题）
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Sisyphus Climb");
    SetTargetFPS(60);
    
    MaximizeWindow();

    InitAudioDevice();
    srand((unsigned int)time(NULL));
    
    GameData game = {0};
    InitGameData(&game);
    LoadGameResources(&game);
    LoadWordList(&game);
    InitStoryNodes(&game);
    
    GameState currentState = GAME_STATE_COVER;
    
    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();
        
        switch (currentState) {
            case GAME_STATE_COVER:
                UpdateGameCover(&game, &currentState);
                break;
            case GAME_STATE_SETTINGS:
                UpdateGameSettings(&game, &currentState);
                break;
            case GAME_STATE_PLAYING:
                UpdateGamePlaying(&game, &currentState, deltaTime);
                break;
            case GAME_STATE_STORY:
                UpdateGameStory(&game, &currentState);
                break;
            case GAME_STATE_GAMEOVER:
                UpdateGameOver(&game, &currentState);
                break;
            default:
                break;
        }
        
        BeginDrawing();
        ClearBackground(GetColor(0x1A1A1AFF));
        
        switch (currentState) {
            case GAME_STATE_COVER:
                DrawGameCover(&game);
                break;
            case GAME_STATE_SETTINGS:
                DrawGameSettings(&game);
                break;
            case GAME_STATE_PLAYING:
                DrawGamePlaying(&game);
                break;
            case GAME_STATE_STORY:
                DrawGameStory(&game);
                break;
            case GAME_STATE_GAMEOVER:
                DrawGameOver(&game);
                break;
            default:
                break;
        }
        
        EndDrawing();
    }
    
    UnloadGameResources(&game);
    if (game.wordList != NULL) {
        for (int i = 0; i < game.wordListCount; i++) {
            free(game.wordList[i]);
        }
        free(game.wordList);
    }
    
    CloseAudioDevice();
    CloseWindow();
    return 0;
}

// 初始化游戏数据
void InitGameData(GameData* game) {
    // 位置初始化
    game->sisyphusPos = (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2};
    game->stonePos = (Vector2){game->sisyphusPos.x + 50, game->sisyphusPos.y + 20};
    game->DemonPos = (Vector2){20.0f, SCREEN_HEIGHT - 80.0f};   
    game->bgScrollOffset = 0.0f;
    game->bgScrollOffsetY = 0.0f;   // 初始化纵向偏移（新增）

    game->totalTyped = 0;
    game->correctTyped = 0;
    game->gameTime = 0.0f;
    game->DemonSpeed = 0.0f;
    game->isDemonCatch = false;
    game->currentStoryNode = 0;
    game->selectedOption = 0;
    game->endingResult = 0;

    game->volume = 1.0f;
    game->progressBar = (Rectangle){50, 30, SCREEN_WIDTH - 100, 30};

    memset(game->currentWord, 0, sizeof(game->currentWord));
    memset(game->inputBuffer, 0, sizeof(game->inputBuffer));
    game->wordIndex = 0;

    game->wordList = NULL;
    game->wordListCount = 0;

    // 初始化剧情触发标记
    memset(game->storyTriggered, false, sizeof(game->storyTriggered));

    // ========== Camera始终跟随玩家（核心：target绑定玩家坐标） ==========
    // 实时更新Camera目标为玩家位置，确保镜头中心永远是玩家
    game->camera = (Camera2D){0};
    game->camera.target = game->sisyphusPos;
    game->camera.offset = (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2};
    game->camera.rotation = 0.0f;
    game->camera.zoom = 1.0f;
    game->cameraSpeed = 1.0f;

    
    // 恶魔相关（核心：初始静止）
    game->monsterSpeed = 0.0f;
    game->monsterAccel = 0.8f;
    game->isDemonActivated = false;
    game->isGameStarted = false;
}

// 加载游戏资源
void LoadGameResources(GameData* game) {
    const char* basePath = GetExecutableDir();
    
    char coverPath[1024], bgPath[1024], sisyphusPath[1024], DemonPath[1024], stonePath[1024], uiBarPath[1024];
    sprintf(coverPath, "%s/assets/images/cover.png", basePath);
    sprintf(bgPath, "%s/assets/images/background.png", basePath);
    sprintf(sisyphusPath, "%s/assets/images/sisyphus.png", basePath);
    sprintf(DemonPath, "%s/assets/images/Demon.png", basePath);
    sprintf(stonePath, "%s/assets/images/stone.png", basePath);
    sprintf(uiBarPath, "%s/assets/images/ui_bar.png", basePath);
    
    printf("📌 coverPath: %s\n", coverPath);
    printf("📌 bgPath: %s\n", bgPath);
    printf("📌 sisyphusPath: %s\n", sisyphusPath);
    
    game->coverImg = LoadTexture(coverPath);
    game->bgImg = LoadTexture(bgPath);
    game->sisyphusImg = LoadTexture(sisyphusPath);
    game->DemonImg = LoadTexture(DemonPath);
    game->stoneImg = LoadTexture(stonePath);
    game->uiBarImg = LoadTexture(uiBarPath);

    if (game->coverImg.id == 0) {
        printf("⚠️ cover.png 未找到，使用默认背景\n");
        Image img = GenImageColor(SCREEN_WIDTH, SCREEN_HEIGHT, PURPLE);
        game->coverImg = LoadTextureFromImage(img);
        UnloadImage(img);
    } else {
        printf("✅ 封面图加载成功\n");
    }

    if (game->bgImg.id == 0) {
        printf("⚠️ background.png 未找到，使用默认背景\n");
        Image img = GenImageColor(SCREEN_WIDTH, SCREEN_HEIGHT, LIGHTGRAY);
        game->bgImg = LoadTextureFromImage(img);
        UnloadImage(img);
    } else {
        printf("✅ 背景图加载成功\n");
    }
    
    if (game->sisyphusImg.id == 0) {
        printf("⚠️ sisyphus.png 未找到，使用默认图形\n");
        Image img = GenImageColor(80, 120, BLUE);
        game->sisyphusImg = LoadTextureFromImage(img);
        UnloadImage(img);
    } else {
        printf("✅ 西西弗斯图加载成功\n");
    }
    
    if (game->DemonImg.id == 0) {
        printf("⚠️ Demon.png 未找到，使用默认图形\n");
        Image img = GenImageColor(80, 120, RED);
        game->DemonImg = LoadTextureFromImage(img);
        UnloadImage(img);
    } else {
        printf("✅ 恶灵图加载成功\n");
    }
    
    if (game->stoneImg.id == 0) {
        printf("⚠️ stone.png 未找到，使用默认图形\n");
        Image img = GenImageColor(60, 60, GRAY);
        game->stoneImg = LoadTextureFromImage(img);
        UnloadImage(img);
    } else {
        printf("✅ 石头图加载成功\n");
    }
    
    if (game->uiBarImg.id == 0) {
        printf("⚠️ ui_bar.png 未找到，使用默认图形\n");
        Image img = GenImageColor(SCREEN_WIDTH - 100, 30, WHITE);
        game->uiBarImg = LoadTextureFromImage(img);
        UnloadImage(img);
    } else {
        printf("✅ UI条加载成功\n");
    }
    
    char typingSoundPath[1024], bgmCoverPath[1024], bgmGamePath[1024], bgmStoryPath[1024];
    sprintf(typingSoundPath, "%s/assets/audio/sound_typing.wav", basePath);
    sprintf(bgmCoverPath, "%s/assets/audio/bgm_cover.mp3", basePath);
    sprintf(bgmGamePath, "%s/assets/audio/bgm_game.mp3", basePath);
    sprintf(bgmStoryPath, "%s/assets/audio/bgm_story.mp3", basePath);
    
    printf("📌 打字音效路径：%s\n", typingSoundPath);
    printf("📌 封面BGM路径：%s\n", bgmCoverPath);
    
    game->typingSound = LoadSound(typingSoundPath);
    game->bgmCover = LoadMusicStream(bgmCoverPath);
    game->bgmGame = LoadMusicStream(bgmGamePath);
    game->bgmStory = LoadMusicStream(bgmStoryPath);
    
    if (game->typingSound.stream.buffer == NULL) {
        printf("⚠️ 打字音效加载失败，跳过音效播放\n");
    } else {
        printf("✅ 打字音效加载成功\n");
    }
    
    if (game->bgmCover.stream.buffer == NULL) {
        printf("⚠️ 封面BGM加载失败，跳过BGM播放\n");
    } else {
        printf("✅ 封面BGM加载成功\n");
        PlayMusicStream(game->bgmCover);
    }
    
    if (game->bgmGame.stream.buffer == NULL) {
        printf("⚠️ 游戏BGM加载失败\n");
    } else {
        printf("✅ 游戏BGM加载成功\n");
    }
    
    if (game->bgmStory.stream.buffer == NULL) {
        printf("⚠️ 剧情BGM加载失败\n");
    } else {
        printf("✅ 剧情BGM加载成功\n");
    }
    
    SetSoundVolume(game->typingSound, game->volume);
    SetMusicVolume(game->bgmCover, game->volume);
    SetMusicVolume(game->bgmGame, game->volume);
    SetMusicVolume(game->bgmStory, game->volume);
}

// 释放游戏资源
void UnloadGameResources(GameData* game) {
    UnloadTexture(game->coverImg);
    UnloadTexture(game->bgImg);
    UnloadTexture(game->sisyphusImg);
    UnloadTexture(game->DemonImg);
    UnloadTexture(game->stoneImg);
    UnloadTexture(game->uiBarImg);
    
    UnloadSound(game->typingSound);
    UnloadMusicStream(game->bgmCover);
    UnloadMusicStream(game->bgmGame);
    UnloadMusicStream(game->bgmStory);
}

// 加载单词库
void LoadWordList(GameData* game) {
    const char* exeDir = GetExecutableDir();
    char easyPath[1024], mediumPath[1024], hardPath[1024];
    sprintf(easyPath, "%s/assets/words/easy.txt", exeDir);
    sprintf(mediumPath, "%s/assets/words/medium.txt", exeDir);
    sprintf(hardPath, "%s/assets/words/hard.txt", exeDir);
    
    printf("📌 单词文件路径：%s\n", easyPath);
    
    const char* files[] = {easyPath, mediumPath, hardPath};
    char buffer[1024];
    int totalWords = 0;
    
    for (int f = 0; f < 3; f++) {
        FILE* file = fopen(files[f], "r");
        if (!file) {
            printf("⚠️ %s 未找到，跳过\n", files[f]);
            continue;
        }
        while (fgets(buffer, sizeof(buffer), file)) {
            totalWords++;
        }
        fclose(file);
    }
    
    if (totalWords == 0) {
        printf("⚠️ 未找到单词文件，使用默认单词库\n");
        totalWords = 10;
        game->wordList = (char**)malloc(totalWords * sizeof(char*));
        game->wordListCount = totalWords;
        
        const char* defaultWords[] = {"sisyphus", "rock", "fate", "struggle", "meaning", 
                                     "cycle", "pain", "hope", "choice", "freedom"};
        for (int i = 0; i < totalWords; i++) {
            game->wordList[i] = (char*)malloc(strlen(defaultWords[i]) + 1);
            strcpy(game->wordList[i], defaultWords[i]);
        }
    } else {
        game->wordList = (char**)malloc(totalWords * sizeof(char*));
        game->wordListCount = 0;
        
        for (int f = 0; f < 3; f++) {
            FILE* file = fopen(files[f], "r");
            if (!file) continue;
            
            while (fgets(buffer, sizeof(buffer), file)) {
                buffer[strcspn(buffer, "\n")] = 0;
                if (strlen(buffer) > 0) {
                    game->wordList[game->wordListCount] = (char*)malloc(strlen(buffer) + 1);
                    strcpy(game->wordList[game->wordListCount], buffer);
                    game->wordListCount++;
                }
            }
            fclose(file);
        }
        
        // 打乱单词顺序
        for (int i = game->wordListCount - 1; i > 0; i--) {
            int j = rand() % (i + 1);
            char* temp = game->wordList[i];
            game->wordList[i] = game->wordList[j];
            game->wordList[j] = temp;
        }
    }
    
    SpawnNewWord(game);
    printf("✅ 单词库加载完成，共 %d 个单词\n", game->wordListCount);
}

// 初始化剧情节点
void InitStoryNodes(GameData* game) {
    game->storyNodes[0] = (StoryNode){
        .wordProgress = 250,
        .storyText = "你已推石前行250步，西西弗斯感到疲惫。他问自己：我推石是为了证明反抗的意义，还是只是徒劳的执念？",
        .option1 = "1. 坚持：即使无意义，反抗本身即是意义",
        .option2 = "2. 放弃：承认徒劳，放慢脚步接受命运",
        .DemonSpeedMultiplier = 1.0f,
        .endingType = 1               
    };
    
    game->storyNodes[1] = (StoryNode){
        .wordProgress = 500,
        .storyText = "前行500步，西西弗斯看见山下的鲜花，短暂的美好让他动摇。痛苦是真实的，还是我为自己编织的牢笼？",
        .option1 = "1. 沉浸：抓住短暂美好，暂时忘记痛苦",
        .option2 = "2. 清醒：直面所有痛苦，不逃避现实",
        .DemonSpeedMultiplier = 1.5f,
        .endingType = 2               
    };
    
    game->storyNodes[2] = (StoryNode){
        .wordProgress = 750,
        .storyText = "仅剩250步登顶，西西弗斯听见山下人的议论。我是为自己推石，还是为了向他人证明我不是懦夫？",
        .option1 = "1. 为己：不在乎他人看法，只为自己的选择",
        .option2 = "2. 为人：渴望被理解，证明自己的价值",
        .DemonSpeedMultiplier = 1.1f,
        .endingType = 0               
    };
    
    game->storyNodes[3] = (StoryNode){
        .wordProgress = 1000,
        .storyText = "终于登顶，石头即将滚落。西西弗斯明白，这不是终点，只是循环的一环。你会选择再次推石，还是放手？",
        .option1 = "1. 再次推石：接受循环，在重复中寻找意义",
        .option2 = "2. 放手：让石头滚落，结束永恒的惩罚",
        .DemonSpeedMultiplier = 0.0f,
        .endingType = 2               
    };
}

// 生成新的随机单词
void SpawnNewWord(GameData* game) {
    if (game->wordListCount == 0) return;
    
    int randIndex = rand() % game->wordListCount;
    strcpy(game->currentWord, game->wordList[randIndex]);
    
    memset(game->inputBuffer, 0, sizeof(game->inputBuffer));
    game->wordIndex = 0;
}

// 检查恶灵是否抓住西西弗斯
bool CheckDemonCatch(GameData* game) {
    // 超大碰撞盒：防止穿透
    Rectangle sisyphusRect = {
        game->sisyphusPos.x - 50,
        game->sisyphusPos.y - 50,
        (float)game->sisyphusImg.width + 100,
        (float)game->sisyphusImg.height + 100
    };
    Rectangle DemonRect = {
        game->DemonPos.x - 50,
        game->DemonPos.y - 50,
        (float)game->DemonImg.width + 100,
        (float)game->DemonImg.height + 100
    };
    
    // 调试：绘制碰撞盒边框
    DrawRectangleLinesEx(sisyphusRect, 2, Fade(RED, 0.8f));
    DrawRectangleLinesEx(DemonRect, 2, Fade(BLUE, 0.8f));
    
    return CheckCollisionRecs(sisyphusRect, DemonRect);
}

// 更新封面界面逻辑
void UpdateGameCover(GameData* game, GameState* currentState) {
    if (game->bgmCover.stream.buffer != NULL) {
        UpdateMusicStream(game->bgmCover);
    }
    
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mousePos = GetMousePosition();
        Rectangle btnStart = {SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2 + 50, 300, 60};
        Rectangle btnSettings = {SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2 + 120, 300, 60};
        Rectangle btnQuit = {SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2 + 190, 300, 60};
        
        if (CheckCollisionPointRec(mousePos, btnStart)) {
            InitGameData(game);
            if (game->wordList == NULL) LoadWordList(game);
            if (strlen(game->currentWord) == 0) SpawnNewWord(game);
            
            if (game->bgmCover.stream.buffer != NULL) {
                StopMusicStream(game->bgmCover);
            }
            if (game->bgmGame.stream.buffer != NULL) {
                PlayMusicStream(game->bgmGame);
                SetMusicVolume(game->bgmGame, game->volume);
            }
            
            game->isDemonCatch = false;
            *currentState = GAME_STATE_PLAYING;
            printf("✅ Game Start!Current word: %s，Character Position：(%.0f, %.0f)，Monster Position：(%.0f, %.0f)\n", 
                   game->currentWord, 
                   game->sisyphusPos.x, game->sisyphusPos.y,
                   game->DemonPos.x, game->DemonPos.y);
        } else if (CheckCollisionPointRec(mousePos, btnSettings)) {
            *currentState = GAME_STATE_SETTINGS;
            printf("✅ 切换到设置界面\n");
        } else if (CheckCollisionPointRec(mousePos, btnQuit)) {
            UnloadGameResources(game);
            if (game->wordList != NULL) {
                for (int i = 0; i < game->wordListCount; i++) {
                    free(game->wordList[i]);
                }
                free(game->wordList);
            }
            CloseAudioDevice();
            CloseWindow();
            exit(0);
        }
    }
}

// 更新设置界面逻辑
void UpdateGameSettings(GameData* game, GameState* currentState) {
    static float volumeTimer = 0.0f;
    float deltaTime = GetFrameTime();
    volumeTimer += deltaTime;
    
    if (IsKeyDown(KEY_LEFT) && game->volume > 0.0f) {
        if (volumeTimer >= 0.1f) {
            game->volume -= 0.05f;
            if (game->volume < 0.0f) game->volume = 0.0f;
            SetSoundVolume(game->typingSound, game->volume);
            SetMusicVolume(game->bgmCover, game->volume);
            SetMusicVolume(game->bgmGame, game->volume);
            SetMusicVolume(game->bgmStory, game->volume);
            printf("🔊 Adjust the volume to: %.1f\n", game->volume);
            volumeTimer = 0.0f;
        }
    }
    if (IsKeyDown(KEY_RIGHT) && game->volume < 1.0f) {
        if (volumeTimer >= 0.1f) {
            game->volume += 0.05f;
            if (game->volume > 1.0f) game->volume = 1.0f;
            SetSoundVolume(game->typingSound, game->volume);
            SetMusicVolume(game->bgmCover, game->volume);
            SetMusicVolume(game->bgmGame, game->volume);
            SetMusicVolume(game->bgmStory, game->volume);
            printf("🔊 Adjust the volume to: %.1f\n", game->volume);
            volumeTimer = 0.0f;
        }
    }
    
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        *currentState = GAME_STATE_COVER;
        if (game->bgmCover.stream.buffer != NULL) PlayMusicStream(game->bgmCover);
        printf("✅ Return to the cover\n");
    }
    
    if (IsKeyReleased(KEY_LEFT) || IsKeyReleased(KEY_RIGHT)) {
        volumeTimer = 0.0f;
    }
}

// 更新游戏进行中逻辑（修复核心问题）
void UpdateGamePlaying(GameData* game, GameState* currentState, float deltaTime) {
    if (game->bgmGame.stream.buffer != NULL) {
        UpdateMusicStream(game->bgmGame);
    }

    if (game->isGameStarted) {
        game->gameTime += deltaTime;
    }

    // 背景斜向滚动（X向右 + Y向上）
    game->bgScrollOffset += BACKGROUND_SCROLL_SPEED_X * deltaTime; // 横向偏移
    game->bgScrollOffsetY += BACKGROUND_SCROLL_SPEED_Y * deltaTime; // 纵向偏移

// 循环滚动
if (game->bgScrollOffset >= game->bgImg.width)  game->bgScrollOffset = 0;
if (game->bgScrollOffsetY <= -game->bgImg.height) game->bgScrollOffsetY = 0;

    // ========== 唯一的恶魔移动逻辑：仅激活后才动 ==========
    if (game->isDemonActivated) {
        // 缓慢加速，初始速度极低
        game->monsterSpeed += game->monsterAccel * deltaTime;
        // 限制最大速度（改为8，远低于原来的18）
        if (game->monsterSpeed > 8.0f) game->monsterSpeed = 8.0f;
        
        // 计算朝向玩家的方向
        Vector2 dir = {
            game->sisyphusPos.x - game->DemonPos.x,
            game->sisyphusPos.y - game->DemonPos.y
        };
        float dirLen = Vector2Length(dir);
        // 距离大于10像素才移动，防止穿透
        if (dirLen > 10.0f) {
            dir = Vector2Normalize(dir);
            // 进一步降低移动系数，确保速度可控
            game->DemonPos.x += dir.x * game->monsterSpeed * 0.15f;
            game->DemonPos.y += dir.y * game->monsterSpeed * 0.08f;
        }
    }

    // 优化恶魔边界：只限制左/下边界，不限制右/上（避免阻挡玩家跟随）
    if (game->DemonPos.x < 20) game->DemonPos.x = 20;
    // 移除右边界限制：game->DemonPos.x > SCREEN_WIDTH - 80
    if (game->DemonPos.y < CLIMB_BOUNDARY_Y) game->DemonPos.y = CLIMB_BOUNDARY_Y;
    // 移除下边界限制：game->DemonPos.y > SCREEN_HEIGHT - 50
    
    // 调试打印
    bool isCollide = CheckDemonCatch(game);
    printf("📌 碰撞状态：%s | 已打字：%d | 游戏时间：%.1f秒\n",
           isCollide ? "是" : "否", game->totalTyped, game->gameTime);
    printf("📌 角色位置：(%.0f, %.0f) | 怪物位置：(%.0f, %.0f)\n",
           game->sisyphusPos.x, game->sisyphusPos.y,
           game->DemonPos.x, game->DemonPos.y);

    // 碰撞检测（唯一入口）
    if (game->gameTime > 1.0f && game->totalTyped > 0) {
        if (CheckDemonCatch(game)) {
        game->isDemonCatch = true;
        *currentState = GAME_STATE_GAMEOVER; // 改为你的结束状态
        printf("❌ 被抓住！玩家(%.0f,%.0f) | 恶魔(%.0f,%.0f)\n",
               game->sisyphusPos.x, game->sisyphusPos.y,
               game->DemonPos.x, game->DemonPos.y);
        return;
    }
    }
    
    // 修复输入处理逻辑（核心修复）
    int key = GetCharPressed();
    while (key > 0) {
        if ((key >= 32) && (key <= 126) && (game->wordIndex < strlen(game->currentWord))) {
            char inputChar = (char)key;
            
                if (!game->isGameStarted) {
                game->isGameStarted = true;
                game->isDemonActivated = true;
                game->gameTime = 0.0f; // 重置计时，从0开始
                printf("🎮 游戏开始！计时/怪物追逐启动\n");
            }
            
            if (inputChar == game->currentWord[game->wordIndex]) {
                // 输入正确：正常推进
                game->inputBuffer[game->wordIndex] = inputChar;
                game->wordIndex++;
                game->totalTyped++;
                game->correctTyped++;
                
                // 玩家移动（Camera会跟随）
                game->sisyphusPos.x += 20.0f;
                game->sisyphusPos.y -= 5.0f;
                game->stonePos.x = game->sisyphusPos.x + 50.0f;
                game->stonePos.y = game->sisyphusPos.y + 20.0f;
                
                // 播放音效（增加空指针保护）
                if (game->typingSound.stream.buffer != NULL) {
                    PlaySound(game->typingSound);
                }
                
                // 单词输入完成，生成新单词
                if (game->wordIndex >= strlen(game->currentWord)) {
                    SpawnNewWord(game);
                }
                
                // 修复剧情触发逻辑（防止重复触发）
                int storyIndex = -1;
                if (game->correctTyped == 250) storyIndex = 0;
                else if (game->correctTyped == 500) storyIndex = 1;
                else if (game->correctTyped == 750) storyIndex = 2;
                else if (game->correctTyped == 1000) storyIndex = 3;
                
                if (storyIndex >= 0 && !game->storyTriggered[storyIndex]) {
                    game->storyTriggered[storyIndex] = true;
                    if (game->bgmGame.stream.buffer != NULL) StopMusicStream(game->bgmGame);
                    if (game->bgmStory.stream.buffer != NULL) {
                        PlayMusicStream(game->bgmStory);
                    }
                    game->currentStoryNode = storyIndex;
                    *currentState = GAME_STATE_STORY;
                    printf("📖 触发剧情：正确字数 %d\n", game->correctTyped);
                    return;
                }

                // 通关检测
                if (game->correctTyped >= TOTAL_WORDS) {
                    *currentState = GAME_STATE_GAMEOVER;
                    printf("🏆 完成1000正确字数挑战，游戏通关\n");
                    return;
                }
            } else {
                // 输入错误：仅统计总输入数
                game->totalTyped++;
                printf("❌ Typed wrong:%c\n", inputChar);
            }
            
            printf("✅ Total typed: %d | Characters accurate: %d | Accuracy: %.1f%%\n",
                   game->totalTyped, game->correctTyped,
                   game->totalTyped > 0 ? (float)game->correctTyped/game->totalTyped*100 : 0);
        }
        // 获取下一个按键（修复重复调用问题）
        key = GetCharPressed();
    }
    
    // 进度条更新
    float progress = (float)game->correctTyped / 1000; // 1000为通关字数
    game->progressBar.width = (SCREEN_WIDTH - 100) * progress;
}

// 更新剧情选择界面逻辑
void UpdateGameStory(GameData* game, GameState* currentState) {
    if (game->bgmStory.stream.buffer != NULL) {
        UpdateMusicStream(game->bgmStory);
    }
    
    if (IsKeyPressed(KEY_ONE)) {
        game->selectedOption = 1;
        float multiplier = game->storyNodes[game->currentStoryNode].DemonSpeedMultiplier;
        
        game->DemonSpeed *= multiplier;
        game->endingResult = game->storyNodes[game->currentStoryNode].endingType;
        
        game->currentStoryNode++;
        if (game->bgmStory.stream.buffer != NULL) StopMusicStream(game->bgmStory);
        if (game->bgmGame.stream.buffer != NULL) {
            PlayMusicStream(game->bgmGame);
            SetMusicVolume(game->bgmGame, game->volume);
        }
        *currentState = GAME_STATE_PLAYING;
        printf("✅ 选择选项1，恶灵速度倍率：%.1f\n", multiplier);
    }
    if (IsKeyPressed(KEY_TWO)) {
        game->selectedOption = 2;
        float multiplier = 1.0f;
        if (game->currentStoryNode == 0) multiplier = 1.15f;
        else if (game->currentStoryNode == 1) multiplier = 1.15f;
        else if (game->currentStoryNode == 2) multiplier = 1.2f;
        else multiplier = 0.0f;
        
        game->DemonSpeed *= multiplier;
        game->endingResult = game->storyNodes[game->currentStoryNode].endingType + 1;
        
        game->currentStoryNode++;
        if (game->bgmStory.stream.buffer != NULL) 
        StopMusicStream(game->bgmStory);
        if (game->bgmGame.stream.buffer != NULL) {
            PlayMusicStream(game->bgmGame);
            SetMusicVolume(game->bgmGame, game->volume);
        }
        *currentState = GAME_STATE_PLAYING;
        printf("✅ 选择选项2，恶灵速度倍率：%.1f\n", multiplier);
    }
}

// 更新游戏结束界面逻辑
void UpdateGameOver(GameData* game, GameState* currentState) {
    bool anyKeyPressed = false;
    for (int i = KEY_A; i <= KEY_Z; i++) {
        if (IsKeyPressed(i)) {
            anyKeyPressed = true;
            break;
        }
    }
    for (int i = KEY_ZERO; i <= KEY_NINE; i++) {
        if (IsKeyPressed(i)) {
            anyKeyPressed = true;
            break;
        }
    }
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE) ||
        IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_TAB)) {
        anyKeyPressed = true;
    }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT)) {
        anyKeyPressed = true;
    }
    
    if (anyKeyPressed) {
        InitGameData(game);
        if (game->wordList == NULL) LoadWordList(game);
        *currentState = GAME_STATE_COVER;
        if (game->bgmGame.stream.buffer != NULL) {
            PlayMusicStream(game->bgmCover);
        }
        printf("✅ 返回封面界面\n");
    }
}

// 绘制封面界面
void DrawGameCover(GameData* game) {
    Rectangle source = {0, 0, (float)game->coverImg.width, (float)game->coverImg.height};
    Rectangle dest = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    DrawTexturePro(game->coverImg, source, dest, (Vector2){0,0}, 0, WHITE);
    
    const char* title = "Sisyphus climb";
    int fontSize = 80;
    Vector2 titlePos = {
        SCREEN_WIDTH/2 - MeasureText(title, fontSize)/2,
        SCREEN_HEIGHT/4
    };
    
    DrawText(title, titlePos.x + 4, titlePos.y + 4, fontSize, DARKGRAY);
    DrawText(title, titlePos.x - 2, titlePos.y, fontSize, MAROON);
    DrawText(title, titlePos.x + 2, titlePos.y, fontSize, MAROON);
    DrawText(title, titlePos.x, titlePos.y - 2, fontSize, MAROON);
    DrawText(title, titlePos.x, titlePos.y + 2, fontSize, MAROON);
    DrawText(title, titlePos.x, titlePos.y, fontSize, GOLD);
    
    Rectangle btnStart = {SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2 + 50, 300, 60};
    Rectangle btnSettings = {SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2 + 120, 300, 60};
    Rectangle btnQuit = {SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2 + 190, 300, 60};
    
    Vector2 mousePos = GetMousePosition();
    bool isHoverStart = CheckCollisionPointRec(mousePos, btnStart);
    bool isHoverSettings = CheckCollisionPointRec(mousePos, btnSettings);
    bool isHoverQuit = CheckCollisionPointRec(mousePos, btnQuit);
    
    DrawRectangleRounded((Rectangle){btnStart.x+4, btnStart.y+4, btnStart.width, btnStart.height}, 0.2, 10, DARKGRAY);
    DrawRectangleRounded((Rectangle){btnSettings.x+4, btnSettings.y+4, btnSettings.width, btnSettings.height}, 0.2, 10, DARKGRAY);
    DrawRectangleRounded((Rectangle){btnQuit.x+4, btnQuit.y+4, btnQuit.width, btnQuit.height}, 0.2, 10, DARKGRAY);
    
    Color btnColor = {120, 0, 0, 200};
    Color hoverColor = {180, 0, 0, 230};
    DrawRectangleRounded(btnStart, 0.2, 10, isHoverStart ? hoverColor : btnColor);
    DrawRectangleRounded(btnSettings, 0.2, 10, isHoverSettings ? hoverColor : btnColor);
    DrawRectangleRounded(btnQuit, 0.2, 10, isHoverQuit ? hoverColor : btnColor);
    
    int btnFontSize = 30;
    DrawText("Start Game", btnStart.x + (btnStart.width - MeasureText("Start Game", btnFontSize))/2, 
             btnStart.y + (btnStart.height - btnFontSize)/2, btnFontSize, WHITE);
    DrawText("Game Setting", btnSettings.x + (btnSettings.width - MeasureText("Game Setting", btnFontSize))/2, 
             btnSettings.y + (btnSettings.height - btnFontSize)/2, btnFontSize, WHITE);
    DrawText("Exit Game", btnQuit.x + (btnQuit.width - MeasureText("Exit Game", btnFontSize))/2, 
             btnQuit.y + (btnQuit.height - btnFontSize)/2, btnFontSize, WHITE);
}

// 绘制设置界面
void DrawGameSettings(GameData* game) {
    Rectangle sourceRect = { 0, 0, (float)game->bgImg.width, (float)game->bgImg.height };
    Rectangle destRect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
    DrawTexturePro(game->bgImg, sourceRect, destRect, (Vector2){0, 0}, 0.0f, WHITE);
    
    DrawText("Game Setting", SCREEN_WIDTH/2 - 100, 100, 50, BLACK);
    DrawText("Game Setting", SCREEN_WIDTH/2 - 98, 98, 50, WHITE);
    
    DrawText("Volume Adjustment: ←/→ Key", SCREEN_WIDTH/2 - 150, 300, 30, BLACK);
    DrawRectangle(SCREEN_WIDTH/2 - 200, 350, 400, 50, LIGHTGRAY);
    DrawRectangle(SCREEN_WIDTH/2 - 200, 350, 400 * game->volume, 50, DARKGRAY);
    DrawText(TextFormat("Volume: %.0f%%", game->volume * 100), SCREEN_WIDTH/2 - 50, 360, 20, WHITE);
    
    DrawText("Press the Return key to return to the cover", SCREEN_WIDTH/2 - 180, 500, 25, BLACK);
}

// 绘制游戏进行中界面
void DrawGamePlaying(GameData* game) {

    // 镜头实时跟随玩家
    game->camera.target = game->sisyphusPos;
    game->camera.offset = (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2};
    game->camera.rotation = 0.0f;
    game->camera.zoom = 1.0f;

    // ========== 强制启用Camera（所有游戏元素随玩家移动） ==========
    BeginMode2D(game->camera);

        // 修复：全屏铺满 + 快速斜向滚动（变量作用域修正）
    // ======================
    // 定义缩放比例（确保在使用前声明）
    float scaleX = (float)SCREEN_WIDTH / game->bgImg.width;
    float scaleY = (float)SCREEN_HEIGHT / game->bgImg.height;
    // 背景源矩形（完整声明）
    Rectangle src = { 0, 0, (float)game->bgImg.width, (float)game->bgImg.height };


    // 绘制无缝斜向滚动背景（X+Y轴同时偏移）
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) { // 新增Y轴循环，确保纵向无缝
            Rectangle sourceRect = {0, 0, (float)game->bgImg.width, (float)game->bgImg.height};

            Rectangle destRect = {
                // X轴：横向滚动 + 多背景拼接
                (float)i * game->bgImg.width + game->bgScrollOffset,
                // Y轴：纵向滚动 + 多背景拼接
                (float)j * game->bgImg.height + game->bgScrollOffsetY,
                (float)game->bgImg.width,
                (float)game->bgImg.height
            };


    // 画第一层（主背景）
    Rectangle dst1 = {
        -game->bgScrollOffset,
        -game->bgScrollOffsetY,
        (float)game->bgImg.width * scaleX,
        (float)game->bgImg.height * scaleY
    };
            DrawTexturePro(game->bgImg, sourceRect, dst1, (Vector2){0,0}, 0, WHITE);

            // 画第二层（无缝拼接）
    Rectangle dst2 = {
        -game->bgScrollOffset + game->bgImg.width * scaleX,
        -game->bgScrollOffsetY - game->bgImg.height * scaleY,
        (float)game->bgImg.width * scaleX,
        (float)game->bgImg.height * scaleY
    };
    DrawTexturePro(game->bgImg, src, dst2, (Vector2){0,0}, 0, WHITE);

        }
    }

    // 绘制玩家、石头、怪物（随Camera移动）
    DrawTexture(game->sisyphusImg, (int)game->sisyphusPos.x, (int)game->sisyphusPos.y, WHITE);
    DrawTexture(game->stoneImg, (int)game->stonePos.x, (int)game->stonePos.y, WHITE);
    DrawTexture(game->DemonImg, (int)game->DemonPos.x, (int)game->DemonPos.y, WHITE);

    EndMode2D();

    // UI（固定在屏幕）
    DrawTexture(game->uiBarImg, 50, 30, WHITE);
    DrawRectangleRec(game->progressBar, GREEN);
    DrawText(TextFormat("Progress: %d/1000", game->correctTyped), 60, 70, 20, BLACK);
    DrawText(TextFormat("Time: %.1fs", game->gameTime), SCREEN_WIDTH - 200, 70, 20, BLACK);

    // 单词输入框
    int wordX = SCREEN_WIDTH/2 - (MeasureText(game->currentWord, 40)/2);
    DrawText(game->currentWord, wordX, 150, 40, RED);
    DrawText(game->inputBuffer, wordX, 150, 40, BLUE);
}

// 绘制剧情选择界面
void DrawGameStory(GameData* game) {
    Rectangle sourceRect = { 0, 0, (float)game->bgImg.width, (float)game->bgImg.height };
    Rectangle destRect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
    DrawTexturePro(game->bgImg, sourceRect, destRect, (Vector2){0, 0}, 0.0f, WHITE);
    
    DrawText(game->storyNodes[game->currentStoryNode].storyText, 100, 100, 25, BLACK);
    DrawText(game->storyNodes[game->currentStoryNode].option1, 100, 400, 30, BLUE);
    DrawText(game->storyNodes[game->currentStoryNode].option2, 100, 450, 30, RED);
    DrawText("Press 1/2 key to select your decision", SCREEN_WIDTH/2 - 180, 600, 25, BLACK);
}

// 绘制游戏结束界面
void DrawGameOver(GameData* game) {
    Rectangle sourceRect = { 0, 0, (float)game->bgImg.width, (float)game->bgImg.height };
    Rectangle destRect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
    DrawTexturePro(game->bgImg, sourceRect, destRect, (Vector2){0, 0}, 0.0f, WHITE);
    
    float accuracy = game->totalTyped > 0 ? (float)game->correctTyped / game->totalTyped * 100 : 0.0f;
    
    int titleX = 0;
    if (game->isDemonCatch) {
        titleX = SCREEN_WIDTH/2 - (MeasureText("You're caught by Demon...", 40)/2);
        DrawText("You're caught by monster...", titleX, 200, 40, RED);
        DrawText("Sisyphus failed to fulfill his punishment and fell into eternal darkness", SCREEN_WIDTH/2 - 300, 260, 25, BLACK);
    } else if (game->correctTyped >= TOTAL_WORDS) {
        titleX = SCREEN_WIDTH/2 - (MeasureText("You've completed the challenge!", 40)/2);
        DrawText("You've completed the challenge!", titleX, 200, 40, GREEN);
        
        char endingText[256] = {0};
        if (game->endingResult == 0) {
            strcpy(endingText, "Ending: An ordinary cycle - You accepted your fate, neither sorrow nor joy");
        } else if (game->endingResult == 1) {
            strcpy(endingText, "Ending: A tragic Rebellion - You fought, but ultimately lost to fate");
        } else {
            strcpy(endingText, "Conclusion: The Awakening of Philosophy - The significance does not lie in the outcome, but in the act of pushing the stone itself");
        }
        int endingX = SCREEN_WIDTH/2 - (MeasureText(endingText, 25)/2);
        DrawText(endingText, endingX, 260, 25, BLACK);
    }
    
    char timeText[64] = {0};
    sprintf(timeText, "Total Duration: %.1fs", game->gameTime);
    int timeX = SCREEN_WIDTH/2 - (MeasureText(timeText, 30)/2);
    DrawText(timeText, timeX, 350, 30, BLACK);
    
    char accuracyText[64] = {0};
    sprintf(accuracyText, "Typing Accuracy Rate: %.1f%%", accuracy);
    int accX = SCREEN_WIDTH/2 - (MeasureText(accuracyText, 30)/2);
    DrawText(accuracyText, accX, 400, 30, BLACK);
    
    char countText[64] = {0};
    sprintf(countText, "Total Word Count: %d (accurate: %d)", game->totalTyped, game->correctTyped);
    int countX = SCREEN_WIDTH/2 - (MeasureText(countText, 30)/2);
    DrawText(countText, countX, 450, 30, BLACK);
    
    int hintX = SCREEN_WIDTH/2 - (MeasureText("Press any key to return to the cover", 25)/2);
    DrawText("Press any key to return to the cover", hintX, 600, 25, BLACK);
}