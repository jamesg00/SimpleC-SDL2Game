#include <SDL2/SDL.h>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

static const int WIDTH = 300;
static const int HEIGHT = 300;

// ---------- helpers ----------
static inline bool aabb(const SDL_Rect& a, const SDL_Rect& b) {
    return SDL_HasIntersection(&a, &b);
}
static inline int irand(int lo, int hi) { // inclusive lo, exclusive hi
    return lo + (std::rand() % std::max(1, hi - lo));
}

// ========== tiny 7-segment HUD digits (no fonts needed) ==========
struct Seg { int x, y, w, h; }; // local offsets
// 7 segments: 0=A top, 1=B up-right, 2=C down-right, 3=D bottom, 4=E down-left, 5=F up-left, 6=G middle
static const int DIGIT_SEGS[10][7] = {
    /*0*/ {1,1,1,1,1,1,0},
    /*1*/ {0,1,1,0,0,0,0},
    /*2*/ {1,1,0,1,1,0,1},
    /*3*/ {1,1,1,1,0,0,1},
    /*4*/ {0,1,1,0,0,1,1},
    /*5*/ {1,0,1,1,0,1,1},
    /*6*/ {1,0,1,1,1,1,1},
    /*7*/ {1,1,1,0,0,0,0},
    /*8*/ {1,1,1,1,1,1,1},
    /*9*/ {1,1,1,1,0,1,1}
};
static void drawSegment(SDL_Renderer* r, int x, int y, int s, int segIndex) {
    const int t = 2*s, L = 6*s, gap = 1*s;
    SDL_Rect rect;
    switch(segIndex){
        case 0: rect = {x+gap,        y,            L, t}; break;                    // A
        case 1: rect = {x+gap+L,      y+gap,        t, L}; break;                    // B
        case 2: rect = {x+gap+L,      y+gap+L+t,    t, L}; break;                    // C
        case 3: rect = {x+gap,        y+2*gap+2*L+t,L, t}; break;                    // D
        case 4: rect = {x,            y+gap+L+t,    t, L}; break;                    // E
        case 5: rect = {x,            y+gap,        t, L}; break;                    // F
        case 6: rect = {x+gap,        y+gap+L,      L, t}; break;                    // G
        default: rect = {x,y,t,t}; break;
    }
    SDL_RenderFillRect(r, &rect);
}
static void drawDigit(SDL_Renderer* r, int x, int y, int s, int d) {
    SDL_SetRenderDrawColor(r, 255,255,255,255);
    for(int seg=0; seg<7; ++seg){
        if (DIGIT_SEGS[d][seg]) drawSegment(r, x, y, s, seg);
    }
}
static void drawColon(SDL_Renderer* r, int x, int y, int s) {
    SDL_SetRenderDrawColor(r, 255,255,255,255);
    int dot = 2*s;
    SDL_Rect top{ x, y + 4*s, dot, dot };
    SDL_Rect bot{ x, y + 9*s, dot, dot };
    SDL_RenderFillRect(r, &top);
    SDL_RenderFillRect(r, &bot);
}
static void drawMMSS(SDL_Renderer* r, int x, int y, int scale, int totalSeconds){
    int mm = totalSeconds / 60;
    int ss = totalSeconds % 60;
    int m1 = (mm/10)%10, m2 = mm%10, s1 = ss/10, s2 = ss%10;

    int dx = x;
    drawDigit(r, dx, y, scale, m1); dx += 10*scale;
    drawDigit(r, dx, y, scale, m2); dx += 10*scale;
    drawColon(r, dx, y, scale);     dx += 5*scale;
    drawDigit(r, dx, y, scale, s1); dx += 10*scale;
    drawDigit(r, dx, y, scale, s2);
}

// ---------- game ----------
enum GameState { TITLE, PLAYING };

int main(int, char**) {
    std::srand((unsigned)std::time(nullptr));

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;
    if (SDL_CreateWindowAndRenderer(WIDTH, HEIGHT, 0, &win, &ren) != 0) {
        SDL_Log("SDL_CreateWindowAndRenderer: %s", SDL_GetError());
        SDL_Quit(); return 1;
    }
    SDL_SetWindowTitle(win, "Rectangle Run (SDL2)");

    // ---- player ----
    SDL_Rect player{ WIDTH/2 - 11, HEIGHT-12-22, 22, 22 }; // centered on ground
    float px = (float)player.x, py = (float)player.y;
    float vx = 0.0f, vy = 0.0f;
    const float moveSpeed = 120.0f;   // px/s
    const float gravity   = 600.0f;   // px/s^2
    const float jumpVel   = -260.0f;  // px/s up
    bool onGround = true;
    int standingPlatform = -1;        // -1 = none
    float prevPy = py;

    // ---- platforms (random) ----
    std::vector<SDL_Rect> plats;
    const int PLAT_COUNT = 24;
    plats.reserve(PLAT_COUNT);
    for (int i = 0; i < PLAT_COUNT; ++i) {
        int w = irand(30, 65);
        int x = irand(0, WIDTH - w);
        int y = irand(-HEIGHT, HEIGHT);
        plats.push_back(SDL_Rect{x, y, w, 10});
    }

    // scroll parameters
    float scrollSpeed = 80.0f;       // px/s downward
    const  float scrollAccel = 3.0f; // small speed-up over time

    // ---- spikes: full bottom row (hazard) ----
    const int SPIKE_H = 12;
    const int spikeTop = HEIGHT - SPIKE_H;
    SDL_Rect spikesStrip{0, spikeTop, WIDTH, SPIKE_H};

    // ---- timer ----
    Uint32 tStart = 0; // set when game actually starts
    int lastTitleBucket = -1;

    auto centerPlayer = [&](){
        px = (float)(WIDTH/2 - player.w/2);
        py = (float)(spikeTop - player.h);
        vx = vy = 0.0f;
        onGround = true;
        standingPlatform = -1;
    };
    auto reshufflePlats = [&](){
        for (auto& p : plats) {
            p.y = irand(-HEIGHT, HEIGHT);
            p.w = irand(30, 65);
            p.x = irand(0, WIDTH - p.w);
        }
    };
    auto resetRun = [&](bool resetSpeed=true){
        centerPlayer();
        if (resetSpeed) scrollSpeed = 80.0f;
        reshufflePlats();
    };

    GameState state = TITLE;
    resetRun(); // show centered player on title

    bool running = true;
    Uint32 prevTicks = SDL_GetTicks();
    while (running) {
        // ------- events -------
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) running = false;

                if (state == TITLE) {
                    if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER) {
                        // start game
                        tStart = SDL_GetTicks();
                        state = PLAYING;
                    }
                } else { // PLAYING
                    if (e.key.keysym.sym == SDLK_SPACE || e.key.keysym.sym == SDLK_UP) {
                        if (onGround || standingPlatform != -1) {
                            vy = jumpVel;          // launch
                            onGround = false;
                            standingPlatform = -1; // detach
                        }
                    }
                    if (e.key.keysym.sym == SDLK_r) { resetRun(); state = TITLE; }
                }
            }
        }

        // ------- dt -------
        Uint32 now = SDL_GetTicks();
        float dt = (now - prevTicks) * 0.001f; // seconds
        prevTicks = now;
        if (dt > 0.033f) dt = 0.033f; // clamp

        // ------- update -------
        // platforms keep moving in both states (looks alive)
        scrollSpeed += scrollAccel * dt;
        float platDy = scrollSpeed * dt; // platforms move DOWN by this amount
        for (auto& p : plats) {
            p.y += (int)platDy;
            if (p.y > HEIGHT) {
                p.y = irand(-80, -10);              // respawn above screen
                p.w = irand(30, 65);
                p.x = irand(0, WIDTH - p.w);
            }
        }

        if (state == PLAYING) {
            // input (held)
            const Uint8* k = SDL_GetKeyboardState(nullptr);
            vx = 0.0f;
            if (k[SDL_SCANCODE_LEFT]  || k[SDL_SCANCODE_A]) vx -= moveSpeed;
            if (k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D]) vx += moveSpeed;

            // physics integrate
            prevPy = py;
            if (standingPlatform == -1) vy += gravity * dt; // no gravity while riding
            px += vx * dt;
            py += vy * dt;

            // walls
            if (px < 0) px = 0;
            if (px + player.w > WIDTH) px = WIDTH - player.w;

            // ----- SPIKE DEATH: detect crossing spike top BEFORE ground clamp -----
            float prevFeet = prevPy + player.h;
            float curFeet  = py     + player.h;
            if (vy > 0.0f && prevFeet < spikeTop && curFeet >= spikeTop) {
                // touched spikes ? reset to TITLE
                resetRun();
                state = TITLE;
                continue; // skip rest of frame logic to avoid flash
            }

            // assume airborne until shown otherwise
            onGround = false;

            // ground clamp (if somehow below spikeTop without crossing check)
            if (py + player.h >= spikeTop) {
                py = (float)(spikeTop - player.h);
                vy = 0.0f;
                onGround = true;
                standingPlatform = -1;
            }

            // write back rect
            player.x = (int)px;
            player.y = (int)py;

            // riding: if standing, ride cleanly
            if (standingPlatform != -1) {
                const SDL_Rect& pl = plats[standingPlatform];
                // stay flush on top
                py = (float)(pl.y - player.h);
                player.y = (int)py;
                vy = 0.0f;
                onGround = true;
                // detach if not horizontally over it / moved away
                bool overHoriz = (player.x + player.w) > pl.x && player.x < (pl.x + pl.w);
                bool nearTop   = std::abs((player.y + player.h) - pl.y) <= 2;
                if (!overHoriz || !nearTop) {
                    standingPlatform = -1;
                    onGround = false;
                }
            }

            // landing from above (crossing test vs platform top)
            if (standingPlatform == -1 && !onGround) {
                prevFeet = prevPy + player.h;
                curFeet  = py     + player.h;
                for (int i = 0; i < (int)plats.size(); ++i) {
                    const SDL_Rect& pl = plats[i];
                    bool horiz = (player.x + player.w) > pl.x && player.x < (pl.x + pl.w);
                    bool crossedTop = (vy > 0.0f) && (prevFeet <= pl.y) && (curFeet >= pl.y);
                    if (horiz && crossedTop) {
                        py = (float)(pl.y - player.h);
                        player.y = (int)py;
                        vy = 0.0f;
                        onGround = true;
                        standingPlatform = i;   // start riding
                        break;
                    }
                }
            }
        } else {
            // TITLE: keep player centered on ground, no physics
            centerPlayer();
            player.x = (int)px;
            player.y = (int)py;
        }

        // ------- timer (title + on-screen) -------
        Uint32 elapsedMs = (state == PLAYING) ? (SDL_GetTicks() - tStart) : 0;
        int totalSec = (int)(elapsedMs / 1000);
        // update window title a few times per sec
        int bucket = totalSec * 4 + (int)((elapsedMs % 1000) / 250);
        if (bucket != lastTitleBucket) {
            lastTitleBucket = bucket;
            int mm = totalSec / 60, ss = totalSec % 60;
            char buf[64];
            SDL_snprintf(buf, sizeof(buf), "Rectangle Run — Time: %d:%02d", mm, ss);
            SDL_SetWindowTitle(win, buf);
        }

        // ================= RENDER =================
        SDL_SetRenderDrawColor(ren, 10,10,14,255);
        SDL_RenderClear(ren);

        // platforms
        SDL_SetRenderDrawColor(ren, 220,220,220,255);
        for (auto& p : plats) SDL_RenderFillRect(ren, &p);

        // player
        SDL_SetRenderDrawColor(ren, 80,180,255,255);
        SDL_RenderFillRect(ren, &player);

        // spikes row (triangles visual; collision uses spikeTop logic)
        std::vector<SDL_Vertex> tris;
        tris.reserve(WIDTH / 6 * 3);
        for (int x = 0; x < WIDTH; x += 6) {
            SDL_FPoint a{(float)x + 3.0f, (float)(spikeTop)};
            SDL_FPoint b{(float)x,        (float)HEIGHT};
            SDL_FPoint c{(float)x + 6.0f, (float)HEIGHT};
            SDL_Color col{255,90,90,255};
            tris.push_back({a, col, {0,0}});
            tris.push_back({b, col, {0,0}});
            tris.push_back({c, col, {0,0}});
        }
        if (!tris.empty()) SDL_RenderGeometry(ren, nullptr, tris.data(), (int)tris.size(), nullptr, 0);

        // HUD: draw MM:SS at top-left (scale 2) — only during PLAYING
        if (state == PLAYING) {
            drawMMSS(ren, 6, 6, 2, totalSec);
        } else {
            // Simple "Press Enter" prompt using rectangles (no fonts)
            // draw a small box at center-top
            SDL_SetRenderDrawColor(ren, 255,255,255,255);
            SDL_Rect box{ WIDTH/2 - 60, 40, 120, 18 };
            SDL_RenderDrawRect(ren, &box);
            // crude “? ENTER” indicator
            SDL_RenderDrawLine(ren, WIDTH/2 - 48, 49, WIDTH/2 - 38, 49);
            SDL_RenderDrawLine(ren, WIDTH/2 - 38, 49, WIDTH/2 - 43, 44);
            SDL_RenderDrawLine(ren, WIDTH/2 - 38, 49, WIDTH/2 - 43, 54);
            // colon + two digits "00:00"
            drawMMSS(ren, WIDTH/2 - 18, 42, 1, 0);
            // (keep it minimalist; the prompt box implies "Press Enter")
        }

        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
