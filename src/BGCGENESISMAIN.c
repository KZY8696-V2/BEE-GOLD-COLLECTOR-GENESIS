#include <genesis.h>

// SGDK uyumlu RGB makro tanımı (0-7 arası renk tonları)
#ifndef RGB
#define RGB(r, g, b) (((b) << 9) | ((g) << 5) | ((r) << 1))
#endif

// --- LANGUAGE & REGION SUPPORT ---
typedef enum {
    GAME_LANG_EN, // English
    GAME_LANG_JP  // Japanese
} GameLanguage;

typedef enum {
    GAME_REGION_WORLD, // USA / Europe (NTSC-U / E)
    GAME_REGION_JAPAN  // Japan (NTSC-J / Mega Drive)
} GameRegion;

static GameLanguage current_lang = GAME_LANG_EN;
static GameRegion current_region = GAME_REGION_WORLD;

// --- GAME STATES ---
typedef enum {
    STATE_MENU,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_SETTINGS,
    STATE_GAME_OVER
} GameState;

static GameState current_state = STATE_MENU;
static GameState last_state = (GameState)-1; // Ekranın sürekli baştan çizilmesini engellemek için

// --- GAME VARIABLES ---
static int bee_x = 40;
static int bee_y = 100;
static int bee_lives = 3;
static int score = 0;
static u32 timer_counter = 0;
static u16 spawn_timer = 0;
static int pause_selection = 0;
static int settings_selection = 0; // 0: Language, 1: Region, 2: Back

// --- ENEMY BUG STRUCTURE ---
#define MAX_BUGS 4
typedef struct {
    int x, y;
    int active;
    int speed;
} Bug;

static Bug bugs[MAX_BUGS];

// --- DETAILED PIXEL GRAPHICS TILE DATA ---
static const u32 tile_grass[8] = {
    0x33333333, 0x31133113, 0x11111111, 0x12211221,
    0x22222222, 0x22222222, 0x22222222, 0x22222222
};

static const u32 tile_dirt[8] = {
    0x44444444, 0x42444244, 0x22222222, 0x42244224,
    0x44444444, 0x24222422, 0x22222222, 0x44444444
};

static const u32 tile_bee_body[8] = {
    0x08000800, 0x33111133, 0x31422413, 0x12222221,
    0x12222221, 0x31222213, 0x03111130, 0x05000050
};

static const u32 tile_bug_body[8] = {
    0x60033006, 0x33455433, 0x55555555, 0x56555565,
    0x55555555, 0x33455433, 0x60033006, 0x00000000
};

static const u32 tile_flower[8] = {
    0x00333000, 0x03777300, 0x37666730, 0x37616730,
    0x37666730, 0x03777300, 0x00222000, 0x00222000
};

// --- INITIALIZE COLOR PALETTES ---
void init_palettes() {
    PAL_setColor(0, RGB(1, 1, 4));       // Dark Sky
    PAL_setColor(1, RGB(7, 6, 0));       // Yellow (Bee)
    PAL_setColor(2, RGB(1, 1, 1));       // Black/Dark Gray
    PAL_setColor(3, RGB(7, 7, 7));       // White

    PAL_setColor(16 + 1, RGB(1, 5, 2));  // Grass Green
    PAL_setColor(16 + 2, RGB(4, 0, 1));  // Dirt Brown
    PAL_setColor(16 + 3, RGB(3, 2, 0));  // Bug Brown
    PAL_setColor(16 + 4, RGB(7, 0, 0));  // Bug Red
}

// --- LOAD GRAPHICS INTO VRAM ---
void load_graphics() {
    VDP_loadTileData(tile_grass, 1, 1, DMA);
    VDP_loadTileData(tile_dirt, 2, 1, DMA);
    VDP_loadTileData(tile_bee_body, 3, 1, DMA);
    VDP_loadTileData(tile_bug_body, 4, 1, DMA);
    VDP_loadTileData(tile_flower, 5, 1, DMA);
}

// --- DRAW BACKGROUND ---
void draw_background() {
    VDP_clearTextArea(0, 0, 40, 28);

    int x, y;
    for (x = 0; x < 40; x++) {
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, 1), x, 23);
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, 1), x, 24);
        
        for (y = 25; y < 28; y++) {
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, 2), x, y);
        }
    }
}

// --- SHOW MAIN MENU ---
void show_menu() {
    VDP_clearTextArea(0, 0, 40, 28);

    int i;
    for (i = 2; i < 38; i += 6) {
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, 5), i, 21);
    }

    if (current_lang == GAME_LANG_EN) {
        VDP_drawText("BEE GOLD COLLECTOR", 10, 6);
        VDP_drawText("SEGA GENESIS EDITION", 10, 8);
        VDP_drawText("PRESS [A] TO PLAY", 11, 15);
        VDP_drawText("PRESS [B] FOR SETTINGS", 10, 17);
    } else {
        VDP_drawText("HATI KOGANE COLLECTOR", 9, 6);
        VDP_drawText("MEGA DRIVE BAN", 12, 8);
        VDP_drawText("[A] BUTTON DE KAISHI", 10, 15);
        VDP_drawText("[B] BUTTON DE SETTEI", 10, 17);
    }
    
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, 3), 18, 11);
}

// --- SHOW SETTINGS MENU ---
void show_settings_menu() {
    VDP_clearTextArea(0, 0, 40, 28);

    if (current_lang == GAME_LANG_EN) {
        VDP_drawText("=== SETTINGS MENU ===", 10, 6);

        if (settings_selection == 0) VDP_drawText("> LANGUAGE: [ ENGLISH ] <", 7, 10);
        else                          VDP_drawText("  LANGUAGE: [ ENGLISH ]   ", 7, 10);

        if (current_region == GAME_REGION_WORLD) {
            if (settings_selection == 1) VDP_drawText("> REGION: [ WORLD / US-EU ] <", 5, 13);
            else                          VDP_drawText("  REGION: [ WORLD / US-EU ]   ", 5, 13);
        } else {
            if (settings_selection == 1) VDP_drawText("> REGION: [ JAPAN / NTSC-J ] <", 5, 13);
            else                          VDP_drawText("  REGION: [ JAPAN / NTSC-J ]   ", 5, 13);
        }

        if (settings_selection == 2) VDP_drawText("> BACK TO MAIN MENU <", 9, 17);
        else                          VDP_drawText("  BACK TO MAIN MENU   ", 9, 17);

        VDP_drawText("USE D-PAD & [A] TO EDIT, [B] BACK", 3, 21);
    } else {
        VDP_drawText("=== SETTEI MENU ===", 11, 6);

        if (settings_selection == 0) VDP_drawText("> GENGOU: [ NIHONGO ] <", 8, 10);
        else                          VDP_drawText("  GENGOU: [ NIHONGO ]   ", 8, 10);

        if (current_region == GAME_REGION_WORLD) {
            if (settings_selection == 1) VDP_drawText("> CHIYIKI: [ WORLD ] <", 8, 13);
            else                          VDP_drawText("  CHIYIKI: [ WORLD ]   ", 8, 13);
        } else {
            if (settings_selection == 1) VDP_drawText("> CHIYIKI: [ JAPAN ] <", 8, 13);
            else                          VDP_drawText("  CHIYIKI: [ JAPAN ]   ", 8, 13);
        }

        if (settings_selection == 2) VDP_drawText("> MAIN MENU E MODORU <", 8, 17);
        else                          VDP_drawText("  MAIN MENU E MODORU   ", 8, 17);

        VDP_drawText("[A] DE KETTEI, [B] MODORU", 7, 21);
    }
}

// --- SHOW PAUSE MENU ---
void show_pause_menu() {
    if (current_lang == GAME_LANG_EN) {
        VDP_drawText("=== GAME PAUSED ===", 10, 10);
        if (pause_selection == 0) {
            VDP_drawText("> RESUME GAME [A] <", 10, 14);
            VDP_drawText("  MAIN MENU       ", 10, 16);
        } else {
            VDP_drawText("  RESUME GAME     ", 10, 14);
            VDP_drawText("> MAIN MENU [B]  <", 10, 16);
        }
        VDP_drawText("[A] RESUME / [B] QUIT TO MENU", 5, 20);
    } else {
        VDP_drawText("=== PAUSECHU ===", 12, 10);
        if (pause_selection == 0) {
            VDP_drawText("> SAIKAI [A]   <", 12, 14);
            VDP_drawText("  MAIN MENU    ", 12, 16);
        } else {
            VDP_drawText("  SAIKAI       ", 12, 14);
            VDP_drawText("> MAIN MENU [B] <", 12, 16);
        }
        VDP_drawText("[A] SAIKAI / [B] MODORU", 8, 20);
    }
}

// --- RESET GAME STATE ---
void reset_game() {
    bee_x = 40;
    bee_y = 100;
    bee_lives = 3;
    score = 0;
    timer_counter = 0;
    spawn_timer = 0;
    pause_selection = 0;

    int i;
    for (i = 0; i < MAX_BUGS; i++) {
        bugs[i].active = 0;
    }

    draw_background();
}

// --- UPDATE GAME LOGIC ---
void update_game() {
    u16 value = JOY_readJoypad(JOY_1);

    if (value & BUTTON_UP)    { if (bee_y > 16)  bee_y -= 2; }
    if (value & BUTTON_DOWN)  { if (bee_y < 170) bee_y += 2; }
    if (value & BUTTON_LEFT)  { if (bee_x > 8)   bee_x -= 2; }
    if (value & BUTTON_RIGHT) { if (bee_x < 280) bee_x += 2; }

    bee_x += 1;
    if (bee_x > 300) bee_x = 20;

    spawn_timer++;
    if (spawn_timer > 60) {
        spawn_timer = 0;
        int i;
        for (i = 0; i < MAX_BUGS; i++) {
            if (!bugs[i].active) {
                bugs[i].active = 1;
                bugs[i].x = 320;
                bugs[i].y = 30 + (random() % 120);
                bugs[i].speed = 1 + (random() % 2);
                break;
            }
        }
    }

    int i;
    for (i = 0; i < MAX_BUGS; i++) {
        if (bugs[i].active) {
            bugs[i].x -= bugs[i].speed;

            if (bugs[i].x < -16) {
                bugs[i].active = 0;
                score += 10;
            }

            if (abs(bee_x - bugs[i].x) < 12 && abs(bee_y - bugs[i].y) < 12) {
                bee_lives--;
                bugs[i].active = 0;
                
                if (bee_lives <= 0) {
                    current_state = STATE_GAME_OVER;
                }
            }
        }
    }

    timer_counter++;
    if (timer_counter >= 900) {
        timer_counter = 0;
        bee_lives += 2;
        if (bee_lives > 9) bee_lives = 9;
    }

    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, 3), bee_x >> 3, bee_y >> 3);

    for (i = 0; i < MAX_BUGS; i++) {
        if (bugs[i].active) {
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, 4), bugs[i].x >> 3, bugs[i].y >> 3);
        }
    }

    char buf[32];
    if (current_lang == GAME_LANG_EN) {
        sprintf(buf, "LIVES: %d  SCORE: %d", bee_lives, score);
    } else {
        sprintf(buf, "INCHI: %d  TENTOU: %d", bee_lives, score);
    }
    VDP_drawText(buf, 2, 2);
}

// --- MAIN ENTRY POINT ---
int main(bool hardReset) {
    VDP_init();
    SPR_init();
    JOY_init();
    
    init_palettes();
    load_graphics();

    u16 last_state_btn = 0;

    while(1) {
        u16 value = JOY_readJoypad(JOY_1);
        u16 pressed = value & ~last_state_btn;

        // Ekran sadece menü/durum değiştiğinde 1 kez çizilsin
        if (current_state != last_state) {
            if (current_state == STATE_MENU) show_menu();
            else if (current_state == STATE_SETTINGS) show_settings_menu();
            else if (current_state == STATE_PAUSED) show_pause_menu();
            else if (current_state == STATE_GAME_OVER) {
                VDP_clearTextArea(0, 0, 40, 28);
                if (current_lang == GAME_LANG_EN) {
                    VDP_drawText(" GAME OVER! ", 14, 10);
                    VDP_drawText("[A] RESTART GAME", 12, 14);
                    VDP_drawText("[B] MAIN MENU   ", 12, 16);
                } else {
                    VDP_drawText(" GAMU OBAA! ", 14, 10);
                    VDP_drawText("[A] RESTART    ", 12, 14);
                    VDP_drawText("[B] MAIN MENU  ", 12, 16);
                }
            }
            last_state = current_state;
        }

        switch(current_state) {
            case STATE_MENU:
                if (pressed & BUTTON_A) {
                    reset_game();
                    current_state = STATE_PLAYING;
                }
                if (pressed & BUTTON_B) {
                    settings_selection = 0;
                    current_state = STATE_SETTINGS;
                }
                break;

            case STATE_SETTINGS:
                if (pressed & BUTTON_UP)   { 
                    settings_selection = (settings_selection > 0) ? settings_selection - 1 : 2; 
                    show_settings_menu();
                }
                if (pressed & BUTTON_DOWN) { 
                    settings_selection = (settings_selection < 2) ? settings_selection + 1 : 0; 
                    show_settings_menu();
                }

                if (pressed & BUTTON_A) {
                    if (settings_selection == 0) {
                        current_lang = (current_lang == GAME_LANG_EN) ? GAME_LANG_JP : GAME_LANG_EN;
                        show_settings_menu();
                    } else if (settings_selection == 1) {
                        current_region = (current_region == GAME_REGION_WORLD) ? GAME_REGION_JAPAN : GAME_REGION_WORLD;
                        show_settings_menu();
                    } else {
                        current_state = STATE_MENU;
                    }
                }
                if (pressed & BUTTON_B) {
                    current_state = STATE_MENU; 
                }
                break;

            case STATE_PLAYING:
                if (pressed & BUTTON_B) {
                    current_state = STATE_PAUSED; 
                    break;
                }
                update_game();
                break;

            case STATE_PAUSED:
                if (pressed & BUTTON_UP)   { pause_selection = 0; show_pause_menu(); }
                if (pressed & BUTTON_DOWN) { pause_selection = 1; show_pause_menu(); }

                if (pressed & BUTTON_A) {
                    draw_background();
                    current_state = STATE_PLAYING; 
                }
                if (pressed & BUTTON_B) {
                    current_state = STATE_MENU; 
                }
                break;

            case STATE_GAME_OVER:
                if (pressed & BUTTON_A) {
                    reset_game();                  
                    current_state = STATE_PLAYING;
                }
                if (pressed & BUTTON_B) {
                    current_state = STATE_MENU;    
                }
                break;
        }

        last_state_btn = value;
        SYS_doVBlankProcess();
    }

    return 0;
}
