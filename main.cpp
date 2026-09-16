#include <windows.h>
#include <vector>
#include <string>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <algorithm>

using namespace std;

// ============================================================
// ENEMY SHOOTING GAME - GUN EDITION
// Clean version: mouse aiming, moving bullets, recoil,
// muzzle flash, 4 weapons, enemy types, levels and power-ups.
// ============================================================

const int W = 900;
const int H = 600;
const int TOP = 75;
const int BOTTOM = 475;
const int TIMER_ID = 1;

enum State { MENU, COUNTDOWN, PLAYING, PAUSED, LEVEL_DONE, GAME_OVER };
enum WeaponType { PISTOL, RIFLE, SHOTGUN, SNIPER };
enum EnemyType { NORMAL, FAST, TANK, BOSS };
enum PowerType { HEALTH, AMMO, SHIELD, RAPID };

struct Weapon {
    const char* name;
    int damage;
    int magazine;
    int ammo;
    int delay;
    float bulletSpeed;
};

struct Enemy {
    float x, y;
    float vx, vy;
    int radius;
    int hp, maxHp;
    EnemyType type;
    bool alive;
};

struct Bullet {
    float x, y;
    float vx, vy;
    int damage;
    bool active;
};

struct Particle {
    float x, y;
    float vx, vy;
    int life;
    bool active;
};

struct PowerUp {
    float x, y;
    PowerType type;
    bool active;
};

vector<Enemy> enemies;
vector<Bullet> bullets;
vector<Particle> particles;
vector<PowerUp> powerUps;

Weapon guns[4];
WeaponType gun = PISTOL;

State state = MENU;

int score = 0;
int highScore = 0;
int level = 1;
int playerHP = 100;
int grenades = 3;
int kills = 0;
int shots = 0;
int hits = 0;
int combo = 0;
int bestCombo = 0;

bool shield = false;
bool rapid = false;
bool reloading = false;

DWORD reloadStart = 0;
DWORD rapidStart = 0;
DWORD levelStart = 0;
DWORD lastFire = 0;
DWORD muzzleStart = 0;

bool muzzle = false;
float recoil = 0.0f;

POINT mouse = {450, 250};

int countdown = 3;
DWORD countdownStart = 0;
int levelMode = 0;


HFONT fBig, fMed, fSmall, fButton;
HDC backDC = NULL;
HBITMAP backBmp = NULL;

RECT playButton = {340,350,560,405};
RECT menuExitButton = {340,420,560,470};
RECT reloadButton = {120,510,215,550};
RECT grenadeButton = {225,510,330,550};
RECT pauseButton = {340,510,435,550};
RECT weaponButton = {445,510,570,550};
RECT exitButton = {580,510,675,550};
RECT againButton = {310,390,590,440};

const DWORD RELOAD_TIME = 900;
const DWORD MUZZLE_TIME = 70;
const DWORD RAPID_TIME = 6000;
const int LEVEL_TIME = 35;

// ============================================================
// BASIC HELPERS
// ============================================================

int rnd(int a, int b)
{
    return a + rand() % (b - a + 1);
}

bool inside(RECT r, int x, int y)
{
    return x >= r.left && x <= r.right &&
           y >= r.top && y <= r.bottom;
}

void txt(HDC dc, const string& s, int x, int y, int w, int h,
         HFONT font, UINT align = DT_LEFT)
{
    HFONT old = (HFONT)SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(245,245,245));

    RECT r = {x,y,x+w,y+h};
    DrawTextA(dc, s.c_str(), -1, &r, align);

    SelectObject(dc, old);
}

void center(HDC dc, const string& s, int y, HFONT font)
{
    txt(dc, s, 0, y, W, 45, font, DT_CENTER);
}

void button(HDC dc, RECT r, const string& label)
{
    HBRUSH b = CreateSolidBrush(RGB(45,48,58));
    HPEN p = CreatePen(PS_SOLID, 2, RGB(150,155,170));

    HBRUSH ob = (HBRUSH)SelectObject(dc,b);
    HPEN op = (HPEN)SelectObject(dc,p);

    RoundRect(dc,r.left,r.top,r.right,r.bottom,8,8);

    SelectObject(dc,ob);
    SelectObject(dc,op);
    DeleteObject(b);
    DeleteObject(p);

    txt(dc,label,r.left,r.top+10,r.right-r.left,25,fButton,DT_CENTER);
}

void beep()
{
    MessageBeep(MB_OK);
}

// ============================================================
// HIGH SCORE
// ============================================================

void loadHighScore()
{
    ifstream in("highscore.txt");
    if(in) in >> highScore;
    else highScore = 0;
}

void saveHighScore()
{
    if(score > highScore) highScore = score;

    ofstream out("highscore.txt");
    if(out) out << highScore;
}

// ============================================================
// WEAPONS
// ============================================================

void setupGuns()
{
    guns[PISTOL] = {"PISTOL",25,12,12,280,14.0f};
    guns[RIFLE]  = {"RIFLE",15,30,30,90,17.0f};
    guns[SHOTGUN]= {"SHOTGUN",22,6,6,550,13.0f};
    guns[SNIPER] = {"SNIPER",80,5,5,800,22.0f};
}

void changeGun(WeaponType w)
{
    if(reloading) return;
    gun = w;
    beep();
}

void reload()
{
    if(reloading) return;

    if(guns[gun].ammo >= guns[gun].magazine) return;

    reloading = true;
    reloadStart = GetTickCount();
}

void updateReload()
{
    if(!reloading) return;

    if(GetTickCount() - reloadStart >= RELOAD_TIME)
    {
        guns[gun].ammo = guns[gun].magazine;
        reloading = false;
        beep();
    }
}

// ============================================================
// ENEMIES
// ============================================================

Enemy makeEnemy(EnemyType type)
{
    Enemy e;

    e.type = type;
    e.alive = true;

    e.x = rnd(60,840);
    e.y = rnd(TOP+50,BOTTOM-90);

    e.vx = (float)rnd(-20,20)/10.0f;
    e.vy = (float)rnd(-15,15)/10.0f;

    if(e.vx == 0) e.vx = 1.0f;
    if(e.vy == 0) e.vy = 0.8f;

    if(type == NORMAL)
    {
        e.radius = 23;
        e.hp = e.maxHp = 1;
        e.vx *= 1.0f;
        e.vy *= 1.0f;
    }
    else if(type == FAST)
    {
        e.radius = 19;
        e.hp = e.maxHp = 1;
        e.vx *= 1.8f;
        e.vy *= 1.8f;
    }
    else if(type == TANK)
    {
        e.radius = 32;
        e.hp = e.maxHp = 3 + level/3;
        e.vx *= 0.65f;
        e.vy *= 0.65f;
    }
    else
    {
        e.radius = 52;
        e.hp = e.maxHp = 12 + level*2;
        e.vx *= 0.45f;
        e.vy *= 0.45f;
    }

    // Make the starting levels more active without changing the rest of the game.
    float d = 1.0f + (level-1)*0.07f;
    if(level == 1) d *= 1.35f;
    else if(level == 2) d *= 1.50f;
    else if(level == 3) d *= 1.60f;
    e.vx *= d;
    e.vy *= d;

    return e;
}

void makeLevel()
{
    enemies.clear();
    bullets.clear();
    particles.clear();
    powerUps.clear();

    // Every level has its own challenge pattern.
    levelMode = (level - 1) % 6;

    int count = min(4 + level, 12);

    for(int i=0;i<count;i++)
    {
        EnemyType t = NORMAL;

        // Level 1: Warm-up
        // Level 2: Fast assault
        // Level 3: Armored enemies
        // Level 4: Mixed attack
        // Level 5: Boss battle + support enemies
        // Level 6+: rotating challenge patterns that keep changing
        if(levelMode == 0)
        {
            t = NORMAL;
        }
        else if(levelMode == 1)
        {
            t = (i % 3 == 0) ? NORMAL : FAST;
        }
        else if(levelMode == 2)
        {
            t = (i % 3 == 0) ? FAST : TANK;
        }
        else if(levelMode == 3)
        {
            int r = rnd(1,100);
            if(r <= 35) t = FAST;
            else if(r <= 65) t = TANK;
            else t = NORMAL;
        }
        else if(levelMode == 4)
        {
            // Boss levels still contain different support enemies.
            if(i == 0) t = BOSS;
            else if(i % 3 == 0) t = TANK;
            else if(i % 2 == 0) t = FAST;
            else t = NORMAL;
        }
        else
        {
            // Rapid mixed wave for higher levels.
            int r = rnd(1,100);
            if(r <= 40) t = FAST;
            else if(r <= 72) t = TANK;
            else t = NORMAL;
        }

        // Avoid adding the boss twice on the regular boss milestone.
        if(t == BOSS && i != 0) t = TANK;
        if(level % 5 == 0 && i == 0) t = BOSS;

        enemies.push_back(makeEnemy(t));
    }

    // Keep the original boss-every-5-level feature.
    if(level % 5 == 0)
    {
        bool bossExists = false;
        for(size_t i=0;i<enemies.size();i++)
            if(enemies[i].type == BOSS) bossExists = true;

        if(!bossExists) enemies.push_back(makeEnemy(BOSS));
    }

    // A small reward appears at the start of each level so every new
    // wave has something useful to collect.
    PowerUp startPower;
    startPower.x = (float)rnd(120,780);
    startPower.y = (float)rnd(TOP+60,BOTTOM-110);
    startPower.type = (PowerType)((level-1)%4);
    startPower.active = true;
    powerUps.push_back(startPower);

    levelStart = GetTickCount();
    countdownStart = GetTickCount();
    countdown = 3;
    state = COUNTDOWN;
}

void newGame()
{
    score = 0;
    level = 1;
    playerHP = 100;
    grenades = 3;
    kills = 0;
    shots = 0;
    hits = 0;
    combo = 0;
    bestCombo = 0;

    shield = false;
    rapid = false;
    reloading = false;
    recoil = 0;
    muzzle = false;

    setupGuns();
    gun = PISTOL;

    makeLevel();
}

bool levelClear()
{
    for(size_t i=0;i<enemies.size();i++)
        if(enemies[i].alive) return false;

    return true;
}

// ============================================================
// PARTICLES / EXPLOSION
// ============================================================

void explosion(float x, float y)
{
    for(int i=0;i<18;i++)
    {
        Particle p;

        float a = (float)(rand()%628)/100.0f;
        float sp = (float)rnd(10,35)/10.0f;

        p.x=x;
        p.y=y;
        p.vx=cos(a)*sp;
        p.vy=sin(a)*sp;
        p.life=rnd(12,25);
        p.active=true;

        particles.push_back(p);
    }
}

void updateParticles()
{
    for(size_t i=0;i<particles.size();i++)
    {
        if(!particles[i].active) continue;

        particles[i].x += particles[i].vx;
        particles[i].y += particles[i].vy;
        particles[i].vy += 0.12f;
        particles[i].life--;

        if(particles[i].life <= 0)
            particles[i].active=false;
    }
}

// ============================================================
// POWER UPS
// ============================================================

void spawnPower(float x,float y)
{
    if(rand()%100 > 22) return;

    PowerUp p;
    p.x=x;
    p.y=y;
    p.type=(PowerType)(rand()%4);
    p.active=true;

    powerUps.push_back(p);
}

void updatePowerUps()
{
    for(size_t i=0;i<powerUps.size();i++)
    {
        if(!powerUps[i].active) continue;

        float dx=mouse.x-powerUps[i].x;
        float dy=mouse.y-powerUps[i].y;

        if(dx*dx+dy*dy < 25*25)
        {
            switch(powerUps[i].type)
            {
                case HEALTH:
                    playerHP=min(100,playerHP+25);
                    break;

                case AMMO:
                    guns[gun].ammo=guns[gun].magazine;
                    break;

                case SHIELD:
                    shield=true;
                    break;

                case RAPID:
                    rapid=true;
                    rapidStart=GetTickCount();
                    break;
            }

            powerUps[i].active=false;
            beep();
        }
    }
}

// ============================================================
// BULLETS
// ============================================================

void fireOne(float angle,int damage,float speed)
{
    Bullet b;

    // Gun muzzle is near the bottom center.
    float gx=W/2.0f;
    float gy=535.0f;

    b.x=gx+cos(angle)*55;
    b.y=gy+sin(angle)*55;

    b.vx=cos(angle)*speed;
    b.vy=sin(angle)*speed;

    b.damage=damage;
    b.active=true;

    bullets.push_back(b);
}

void shoot()
{
    if(state != PLAYING || reloading) return;

    DWORD now=GetTickCount();

    int delay=guns[gun].delay;

    if(rapid) delay/=2;

    if(now-lastFire < (DWORD)delay) return;

    if(guns[gun].ammo <= 0)
    {
        reload();
        return;
    }

    lastFire=now;
    guns[gun].ammo--;
    shots++;

    float gx=W/2.0f;
    float gy=535.0f;

    float dx=mouse.x-gx;
    float dy=mouse.y-gy;

    float len=sqrt(dx*dx+dy*dy);
    if(len<1) len=1;

    dx/=len;
    dy/=len;

    float base=atan2(dy,dx);

    muzzle=true;
    muzzleStart=now;

    if(gun == SHOTGUN)
    {
        for(int i=0;i<6;i++)
        {
            float spread=((rand()%31)-15)/100.0f;
            fireOne(base+spread,guns[gun].damage,
                    guns[gun].bulletSpeed);
        }
    }
    else
    {
        fireOne(base,guns[gun].damage,guns[gun].bulletSpeed);
    }

    recoil=14.0f;
}

void updateBullets()
{
    for(size_t i=0;i<bullets.size();i++)
    {
        Bullet &b=bullets[i];

        if(!b.active) continue;

        b.x+=b.vx;
        b.y+=b.vy;

        // Keep bullets alive in the whole window so the gun can also
        // fire horizontally to the left or right from the bottom.
        if(b.x<0 || b.x>W || b.y<0 || b.y>H)
        {
            b.active=false;
            continue;
        }

        for(size_t j=0;j<enemies.size();j++)
        {
            Enemy &e=enemies[j];

            if(!e.alive) continue;

            float dx=b.x-e.x;
            float dy=b.y-e.y;

            if(dx*dx+dy*dy <= e.radius*e.radius)
            {
                e.hp-=b.damage;
                b.active=false;
                hits++;
                combo++;

                if(combo>bestCombo) bestCombo=combo;

                int pts=10;

                if(e.type==FAST) pts=25;
                if(e.type==TANK) pts=50;
                if(e.type==BOSS) pts=150;

                if(gun==SNIPER) pts*=2;

                score += pts + combo*2;

                explosion(e.x,e.y);

                if(e.hp<=0)
                {
                    e.alive=false;
                    kills++;

                    score += (e.type==BOSS ? 250 : 20);

                    spawnPower(e.x,e.y);
                    beep();
                }

                break;
            }
        }
    }

    vector<Bullet> aliveBullets;

    for(size_t i=0;i<bullets.size();i++)
        if(bullets[i].active)
            aliveBullets.push_back(bullets[i]);

    bullets=aliveBullets;
}

// ============================================================
// ENEMY UPDATE
// ============================================================

void updateEnemies()
{
    DWORD now = GetTickCount();
    float timeWave = now / 220.0f;

    for(size_t i=0;i<enemies.size();i++)
    {
        Enemy &e=enemies[i];

        if(!e.alive) continue;

        // Different movement styles make each level feel different.
        if(levelMode == 1)
        {
            // Fast assault: sharper side-to-side movement.
            e.x += e.vx * 1.35f;
            e.y += e.vy * 0.95f + sin((timeWave + (float)i)*0.9f) * 1.1f;
        }
        else if(levelMode == 2)
        {
            // Armored wave: slower but stronger, with vertical pressure.
            e.x += e.vx * 1.08f;
            e.y += e.vy * 1.22f;
        }
        else if(levelMode == 3)
        {
            // Mixed wave: zig-zag movement.
            e.x += e.vx;
            e.y += e.vy + sin((timeWave + (float)i)*1.4f) * 1.2f;
        }
        else if(levelMode == 4)
        {
            // Boss/support wave: large sweeping movement.
            e.x += e.vx * 0.95f;
            e.y += e.vy + cos((timeWave + (float)i)*0.8f) * 0.8f;
        }
        else if(levelMode == 5)
        {
            // High-level rush: quick unpredictable movement.
            e.x += e.vx * 1.25f;
            e.y += e.vy * 1.10f;
        }
        else
        {
            // Level 1: make the starting enemies move faster so the game
            // feels active from the very beginning.
            e.x += e.vx * 1.25f;
            e.y += e.vy * 1.18f + sin((timeWave + (float)i)*0.9f) * 0.8f;
        }

        if(e.x<e.radius || e.x>W-e.radius)
            e.vx*=-1;

        if(e.y<TOP+e.radius || e.y>BOTTOM-e.radius)
            e.vy*=-1;

        if(e.type != BOSS)
        {
            e.vx=max(-5.5f,min(5.5f,e.vx));
            e.vy=max(-4.5f,min(4.5f,e.vy));
        }
    }
}

// ============================================================
// GRENADE
// ============================================================

void grenade()
{
    if(state!=PLAYING || grenades<=0) return;

    grenades--;

    explosion((float)mouse.x,(float)mouse.y);

    for(size_t i=0;i<enemies.size();i++)
    {
        Enemy &e=enemies[i];

        if(!e.alive) continue;

        float dx=e.x-mouse.x;
        float dy=e.y-mouse.y;
        float d=sqrt(dx*dx+dy*dy);

        if(d<140)
        {
            e.hp-=4;

            if(e.hp<=0)
            {
                e.alive=false;
                kills++;
                score+=75;
            }
        }
    }
}

// ============================================================
// DRAW COLORFUL BACKGROUND
// ============================================================

void drawBackground(HDC dc)
{
    // Dark blue/purple base
    HBRUSH bg=CreateSolidBrush(RGB(9,12,24));
    RECT all={0,0,W,H};
    FillRect(dc,&all,bg);
    DeleteObject(bg);

    // Color bands for a more lively game arena
    HBRUSH topGlow=CreateSolidBrush(RGB(18,24,48));
    RECT topArea={0,TOP,W,180};
    FillRect(dc,&topArea,topGlow);
    DeleteObject(topGlow);

    HBRUSH midGlow=CreateSolidBrush(RGB(20,32,42));
    RECT midArea={0,180,W,330};
    FillRect(dc,&midArea,midGlow);
    DeleteObject(midGlow);

    HBRUSH lowGlow=CreateSolidBrush(RGB(24,25,38));
    RECT lowArea={0,330,W,BOTTOM};
    FillRect(dc,&lowArea,lowGlow);
    DeleteObject(lowGlow);

    // Neon-style grid
    HPEN grid1=CreatePen(PS_SOLID,1,RGB(30,60,78));
    HPEN old=(HPEN)SelectObject(dc,grid1);

    for(int x=0;x<W;x+=45)
    {
        MoveToEx(dc,x,TOP,NULL);
        LineTo(dc,x,BOTTOM);
    }

    for(int y=TOP;y<BOTTOM;y+=45)
    {
        MoveToEx(dc,0,y,NULL);
        LineTo(dc,W,y);
    }

    SelectObject(dc,old);
    DeleteObject(grid1);

    // Soft horizon lines
    HPEN line1=CreatePen(PS_SOLID,2,RGB(70,160,190));
    old=(HPEN)SelectObject(dc,line1);
    MoveToEx(dc,0,145,NULL);
    LineTo(dc,W,145);
    SelectObject(dc,old);
    DeleteObject(line1);

    HPEN line2=CreatePen(PS_SOLID,3,RGB(120,75,190));
    old=(HPEN)SelectObject(dc,line2);
    MoveToEx(dc,0,BOTTOM,NULL);
    LineTo(dc,W,BOTTOM);
    SelectObject(dc,old);
    DeleteObject(line2);

    // Small decorative lights
    HBRUSH light=CreateSolidBrush(RGB(60,190,220));
    HBRUSH oldBrush=(HBRUSH)SelectObject(dc,light);
    for(int x=20;x<W;x+=70)
        Ellipse(dc,x,92,x+5,97);
    SelectObject(dc,oldBrush);
    DeleteObject(light);
}

// ============================================================
// DRAW MORE REALISTIC / COLORFUL ENEMIES
// ============================================================

void enemyPart(HDC dc,COLORREF fill,COLORREF outline,
               int left,int top,int right,int bottom)
{
    HBRUSH b=CreateSolidBrush(fill);
    HPEN p=CreatePen(PS_SOLID,2,outline);
    HBRUSH ob=(HBRUSH)SelectObject(dc,b);
    HPEN op=(HPEN)SelectObject(dc,p);
    RoundRect(dc,left,top,right,bottom,8,8);
    SelectObject(dc,ob);
    SelectObject(dc,op);
    DeleteObject(b);
    DeleteObject(p);
}

void enemyCircle(HDC dc,COLORREF fill,COLORREF outline,
                 int left,int top,int right,int bottom)
{
    HBRUSH b=CreateSolidBrush(fill);
    HPEN p=CreatePen(PS_SOLID,2,outline);
    HBRUSH ob=(HBRUSH)SelectObject(dc,b);
    HPEN op=(HPEN)SelectObject(dc,p);
    Ellipse(dc,left,top,right,bottom);
    SelectObject(dc,ob);
    SelectObject(dc,op);
    DeleteObject(b);
    DeleteObject(p);
}

void drawEnemy(HDC dc,Enemy &e)
{
    if(!e.alive) return;

    int x=(int)e.x;
    int y=(int)e.y;
    int r=e.radius;

    // Different armor colors for each enemy type
    COLORREF armor=RGB(205,52,64);
    COLORREF darkArmor=RGB(120,20,35);
    COLORREF visor=RGB(255,220,220);

    if(e.type==FAST)
    {
        armor=RGB(255,132,35);
        darkArmor=RGB(165,55,15);
        visor=RGB(255,245,195);
    }
    else if(e.type==TANK)
    {
        armor=RGB(72,105,122);
        darkArmor=RGB(35,55,68);
        visor=RGB(190,235,245);
    }
    else if(e.type==BOSS)
    {
        armor=RGB(116,70,220);
        darkArmor=RGB(58,32,125);
        visor=RGB(235,190,255);
    }

    // Shadow
    enemyCircle(dc,RGB(5,7,12),RGB(5,7,12),
                x-r+4,y-r+8,x+r+7,y+r+11);

    // Main body armor
    enemyPart(dc,armor,darkArmor,
              x-r+2,y-2,x+r-2,y+r+13);

    // Shoulder pads
    enemyCircle(dc,armor,darkArmor,
                x-r-9,y+5,x-r+7,y+20);
    enemyCircle(dc,armor,darkArmor,
                x+r-7,y+5,x+r+9,y+20);

    // Chest plate
    enemyPart(dc,darkArmor,RGB(225,225,235),
              x-r+9,y+8,x+r-9,y+24);

    // Chest core / badge
    COLORREF core=RGB(70,220,255);
    if(e.type==FAST) core=RGB(255,220,50);
    if(e.type==TANK) core=RGB(80,255,150);
    if(e.type==BOSS) core=RGB(255,90,220);
    enemyCircle(dc,core,RGB(235,245,255),
                x-6,y+11,x+6,y+23);

    // Helmet / head
    enemyCircle(dc,armor,darkArmor,
                x-r+6,y-r-4,x+r-6,y+9);

    // Helmet top ridge
    enemyPart(dc,darkArmor,darkArmor,
              x-r+11,y-r-8,x+r-11,y-r+1);

    // Visor
    enemyPart(dc,RGB(24,32,45),visor,
              x-r+13,y-r+1,x+r-13,y+r/4+1);

    // Glowing eyes / visor lights
    HBRUSH eye=CreateSolidBrush(visor);
    HBRUSH oe=(HBRUSH)SelectObject(dc,eye);
    int ey=(int)(y-r/4);
    Ellipse(dc,x-r/2,ey,x-r/2+7,ey+7);
    Ellipse(dc,x+r/2-7,ey,x+r/2,ey+7);
    SelectObject(dc,oe);
    DeleteObject(eye);

    // Small weapon/arm details
    HPEN arm=CreatePen(PS_SOLID,5,darkArmor);
    HPEN old=(HPEN)SelectObject(dc,arm);
    MoveToEx(dc,x-r+1,y+12,NULL);
    LineTo(dc,x-r-13,y+23);
    MoveToEx(dc,x+r-1,y+12,NULL);
    LineTo(dc,x+r+13,y+23);
    SelectObject(dc,old);
    DeleteObject(arm);

    // Legs
    HPEN legs=CreatePen(PS_SOLID,7,darkArmor);
    old=(HPEN)SelectObject(dc,legs);
    MoveToEx(dc,x-10,y+r+5,NULL);
    LineTo(dc,x-15,y+r+17);
    MoveToEx(dc,x+10,y+r+5,NULL);
    LineTo(dc,x+15,y+r+17);
    SelectObject(dc,old);
    DeleteObject(legs);

    // Tank / boss extra plating
    if(e.type==TANK || e.type==BOSS)
    {
        enemyPart(dc,darkArmor,RGB(235,235,240),
                  x-r-5,y-r+10,x-r+4,y+r-3);
        enemyPart(dc,darkArmor,RGB(235,235,240),
                  x+r-4,y-r+10,x+r+5,y+r-3);
    }

    if(e.type==FAST)
    {
        // Speed fins
        POINT fin1[3]={{x-r+2,y-6},{x-r-14,y-16},{x-r-4,y+4}};
        POINT fin2[3]={{x+r-2,y-6},{x+r+14,y-16},{x+r+4,y+4}};
        HBRUSH fb=CreateSolidBrush(RGB(255,170,30));
        HBRUSH of=(HBRUSH)SelectObject(dc,fb);
        Polygon(dc,fin1,3);
        Polygon(dc,fin2,3);
        SelectObject(dc,of);
        DeleteObject(fb);
    }

    if(e.type==BOSS)
    {
        // Boss horns / crown
        POINT horn1[3]={{x-r+6,y-r-5},{x-r+12,y-r-25},{x-r+18,y-r-5}};
        POINT horn2[3]={{x+r-18,y-r-5},{x+r-12,y-r-25},{x+r-6,y-r-5}};
        HBRUSH hb=CreateSolidBrush(RGB(255,75,110));
        HBRUSH oh=(HBRUSH)SelectObject(dc,hb);
        Polygon(dc,horn1,3);
        Polygon(dc,horn2,3);
        SelectObject(dc,oh);
        DeleteObject(hb);
    }

    // Health bar
    int bw=max(34,r*2+4);
    int filled=(bw*e.hp)/max(1,e.maxHp);

    HBRUSH hbg=CreateSolidBrush(RGB(45,15,20));
    RECT barBg={x-bw/2,y-r-18,x+bw/2,y-r-9};
    FillRect(dc,&barBg,hbg);
    DeleteObject(hbg);

    COLORREF hpColor=RGB(50,235,100);
    if(e.hp*3<e.maxHp*1) hpColor=RGB(255,70,70);
    else if(e.hp*3<e.maxHp*2) hpColor=RGB(255,190,50);

    HBRUSH hfg=CreateSolidBrush(hpColor);
    RECT barFill={x-bw/2,y-r-18,x-bw/2+filled,y-r-9};
    FillRect(dc,&barFill,hfg);
    DeleteObject(hfg);

    // Enemy type label
    if(e.type==FAST)
        txt(dc,"FAST",x-r-6,y+r+19,r*2+12,20,fSmall,DT_CENTER);
    else if(e.type==TANK)
        txt(dc,"TANK",x-r-6,y+r+19,r*2+12,20,fSmall,DT_CENTER);
    else if(e.type==BOSS)
        txt(dc,"BOSS",x-r-10,y+r+19,r*2+20,20,fSmall,DT_CENTER);
}

// ============================================================
// DRAW BULLETS
// ============================================================

void drawBullets(HDC dc)
{
    HBRUSH b=CreateSolidBrush(RGB(255,220,55));
    HBRUSH old=(HBRUSH)SelectObject(dc,b);

    for(size_t i=0;i<bullets.size();i++)
    {
        if(!bullets[i].active) continue;

        int x=(int)bullets[i].x;
        int y=(int)bullets[i].y;

        Ellipse(dc,x-4,y-4,x+4,y+4);
    }

    SelectObject(dc,old);
    DeleteObject(b);
}

// ============================================================
// DRAW PARTICLES
// ============================================================

void drawParticles(HDC dc)
{
    HBRUSH b=CreateSolidBrush(RGB(255,155,30));
    HBRUSH old=(HBRUSH)SelectObject(dc,b);

    for(size_t i=0;i<particles.size();i++)
    {
        if(!particles[i].active) continue;

        int x=(int)particles[i].x;
        int y=(int)particles[i].y;

        Ellipse(dc,x-3,y-3,x+3,y+3);
    }

    SelectObject(dc,old);
    DeleteObject(b);
}

// ============================================================
// DRAW POWER UPS
// ============================================================

void drawPowerUps(HDC dc)
{
    for(size_t i=0;i<powerUps.size();i++)
    {
        if(!powerUps[i].active) continue;

        COLORREF c=RGB(70,210,100);

        if(powerUps[i].type==HEALTH) c=RGB(50,210,80);
        if(powerUps[i].type==AMMO) c=RGB(240,190,40);
        if(powerUps[i].type==SHIELD) c=RGB(70,150,240);
        if(powerUps[i].type==RAPID) c=RGB(210,70,230);

        HBRUSH b=CreateSolidBrush(c);
        HBRUSH old=(HBRUSH)SelectObject(dc,b);

        Ellipse(dc,(int)powerUps[i].x-14,(int)powerUps[i].y-14,
                (int)powerUps[i].x+14,(int)powerUps[i].y+14);

        SelectObject(dc,old);
        DeleteObject(b);

        string s="?";

        if(powerUps[i].type==HEALTH) s="+";
        if(powerUps[i].type==AMMO) s="A";
        if(powerUps[i].type==SHIELD) s="S";
        if(powerUps[i].type==RAPID) s="R";

        txt(dc,s,(int)powerUps[i].x-12,(int)powerUps[i].y-10,
            24,22,fButton,DT_CENTER);
    }
}

// ============================================================
// DRAW GUN
// ============================================================

void drawGun(HDC dc)
{
    float gx=W/2.0f;
    float gy=535.0f;

    float dx=mouse.x-gx;
    float dy=mouse.y-gy;

    float angle=atan2(dy,dx);

    float r=recoil;

    gx-=cos(angle)*r;
    gy-=sin(angle)*r;

    float length=80;

    if(gun==RIFLE) length=125;
    if(gun==SHOTGUN) length=100;
    if(gun==SNIPER) length=155;

    float bx=gx+cos(angle)*length;
    float by=gy+sin(angle)*length;

    // barrel
    HPEN barrel=CreatePen(PS_SOLID,18,RGB(38,40,46));
    HPEN old=(HPEN)SelectObject(dc,barrel);

    MoveToEx(dc,(int)gx,(int)gy,NULL);
    LineTo(dc,(int)bx,(int)by);

    SelectObject(dc,old);
    DeleteObject(barrel);

    // top highlight
    HPEN hi=CreatePen(PS_SOLID,4,RGB(105,110,120));
    old=(HPEN)SelectObject(dc,hi);

    MoveToEx(dc,(int)gx,(int)gy-3,NULL);
    LineTo(dc,(int)bx,(int)by-3);

    SelectObject(dc,old);
    DeleteObject(hi);

    // body
    HBRUSH body=CreateSolidBrush(RGB(65,67,74));
    HPEN bodyPen=CreatePen(PS_SOLID,2,RGB(190,190,195));

    HBRUSH ob=(HBRUSH)SelectObject(dc,body);
    old=(HPEN)SelectObject(dc,bodyPen);

    Ellipse(dc,(int)gx-25,(int)gy-18,
            (int)gx+25,(int)gy+18);

    SelectObject(dc,ob);
    SelectObject(dc,old);

    DeleteObject(body);
    DeleteObject(bodyPen);

    // handle
    float ha=angle+1.57f;

    float x1=gx-cos(angle)*10;
    float y1=gy-sin(angle)*10;

    float x2=x1+cos(ha)*45;
    float y2=y1+sin(ha)*45;

    HPEN handle=CreatePen(PS_SOLID,21,RGB(55,35,25));
    old=(HPEN)SelectObject(dc,handle);

    MoveToEx(dc,(int)x1,(int)y1,NULL);
    LineTo(dc,(int)x2,(int)y2);

    SelectObject(dc,old);
    DeleteObject(handle);

    // muzzle flash
    if(muzzle)
    {
        float fl=38;

        HBRUSH flash=CreateSolidBrush(RGB(255,215,35));
        HBRUSH of=(HBRUSH)SelectObject(dc,flash);

        POINT p[6];

        p[0].x=(LONG)bx;
        p[0].y=(LONG)by;

        p[1].x=(LONG)(bx+cos(angle+0.35f)*fl);
        p[1].y=(LONG)(by+sin(angle+0.35f)*fl);

        p[2].x=(LONG)(bx+cos(angle)*fl);
        p[2].y=(LONG)(by+sin(angle)*fl);

        p[3].x=(LONG)(bx+cos(angle-0.35f)*fl);
        p[3].y=(LONG)(by+sin(angle-0.35f)*fl);

        p[4].x=(LONG)(bx+cos(angle+0.12f)*20);
        p[4].y=(LONG)(by+sin(angle+0.12f)*20);

        p[5].x=(LONG)(bx+cos(angle-0.12f)*20);
        p[5].y=(LONG)(by+sin(angle-0.12f)*20);

        Polygon(dc,p,6);

        SelectObject(dc,of);
        DeleteObject(flash);
    }

    if(reloading)
        center(dc,"RELOADING...",425,fMed);
}

// ============================================================
// HUD
// ============================================================

void drawHUD(HDC dc)
{
    txt(dc,"SCORE: "+to_string(score),15,15,170,30,fMed);
    txt(dc,"HIGH: "+to_string(highScore),185,15,160,30,fSmall);
    txt(dc,"LEVEL: "+to_string(level),345,15,120,30,fMed);
    txt(dc,"COMBO x"+to_string(combo),470,15,150,30,fMed);

    txt(dc,"HP",650,15,30,25,fSmall);

    HBRUSH bg=CreateSolidBrush(RGB(75,25,25));
    RECT r={680,18,790,32};
    FillRect(dc,&r,bg);
    DeleteObject(bg);

    HBRUSH hp=CreateSolidBrush(RGB(45,210,75));
    RECT rr={680,18,680+playerHP+10,32};
    FillRect(dc,&rr,hp);
    DeleteObject(hp);

    txt(dc,guns[gun].name,15,475,180,25,fSmall);
    txt(dc,"AMMO "+to_string(guns[gun].ammo)+"/"+
        to_string(guns[gun].magazine),200,475,190,25,fSmall);

    txt(dc,"KILLS "+to_string(kills),390,475,140,25,fSmall);
    txt(dc,"GRENADES "+to_string(grenades),535,475,170,25,fSmall);

    if(shield)
        txt(dc,"SHIELD",715,475,100,25,fSmall);

    if(rapid)
        txt(dc,"RAPID",815,475,80,25,fSmall);
}

// ============================================================
// BOTTOM BUTTONS
// ============================================================

void drawBottom(HDC dc)
{
    button(dc,reloadButton,"RELOAD");
    button(dc,grenadeButton,"GRENADE");
    button(dc,pauseButton,"PAUSE");
    button(dc,weaponButton,"WEAPON");
    button(dc,exitButton,"EXIT");
}

// ============================================================
// SCREENS
// ============================================================

void drawMenu(HDC dc)
{
    drawBackground(dc);

    center(dc,"ENEMY SHOOTING GAME",115,fBig);
    center(dc,"GUN EDITION",170,fMed);

    center(dc,"Mouse = Aim    LEFT CLICK = Fire",225,fSmall);
    center(dc,"1 Pistol   2 Rifle   3 Shotgun   4 Sniper",255,fSmall);
    center(dc,"R Reload    G Grenade    P Pause",280,fSmall);

    button(dc,playButton,"PLAY GAME");
    button(dc,menuExitButton,"EXIT");
}

string levelChallengeName()
{
    if(levelMode == 0) return "WARM-UP WAVE";
    if(levelMode == 1) return "FAST ASSAULT";
    if(levelMode == 2) return "ARMORED ATTACK";
    if(levelMode == 3) return "CHAOS WAVE";
    if(levelMode == 4) return "BOSS & SUPPORT";
    return "RAPID RUSH";
}

void drawCountdown(HDC dc)
{
    drawBackground(dc);

    for(size_t i=0;i<enemies.size();i++)
        drawEnemy(dc,enemies[i]);

    center(dc,"LEVEL "+to_string(level),135,fMed);
    center(dc,levelChallengeName(),180,fMed);
    center(dc,to_string(countdown),235,fBig);
    center(dc,"GET READY!",300,fMed);
}

void drawPlaying(HDC dc)
{
    drawBackground(dc);
    drawHUD(dc);

    for(size_t i=0;i<enemies.size();i++)
        drawEnemy(dc,enemies[i]);

    drawPowerUps(dc);
    drawBullets(dc);
    drawParticles(dc);
    drawGun(dc);
    drawBottom(dc);
}

void drawPause(HDC dc)
{
    drawBackground(dc);

    for(size_t i=0;i<enemies.size();i++)
        drawEnemy(dc,enemies[i]);

    center(dc,"GAME PAUSED",190,fBig);
    center(dc,"Press P to continue",260,fMed);
}

void drawLevelDone(HDC dc)
{
    drawBackground(dc);

    center(dc,"LEVEL COMPLETE!",150,fBig);
    center(dc,"Level "+to_string(level)+" completed",225,fMed);
    center(dc,"Score: "+to_string(score),270,fMed);
    center(dc,"Press ENTER for next level",325,fSmall);
}

void drawGameOver(HDC dc)
{
    drawBackground(dc);

    center(dc,"GAME OVER",145,fBig);
    center(dc,"Score: "+to_string(score),220,fMed);
    center(dc,"Enemies defeated: "+to_string(kills),260,fSmall);
    center(dc,"Best Combo: x"+to_string(bestCombo),290,fSmall);

    button(dc,againButton,"PLAY AGAIN");
    center(dc,"Press ESC to exit",455,fSmall);
}

// ============================================================
// INPUT
// ============================================================

void keyDown(WPARAM key)
{
    if(key==VK_ESCAPE)
    {
        PostQuitMessage(0);
        return;
    }

    if(state==MENU)
    {
        if(key==VK_RETURN) newGame();
        return;
    }

    if(state==GAME_OVER)
    {
        if(key==VK_RETURN) newGame();
        return;
    }

    if(state==LEVEL_DONE)
    {
        if(key==VK_RETURN)
        {
            level++;
            makeLevel();
        }
        return;
    }

    if(state==COUNTDOWN)
        return;

    if(key=='P' || key=='p')
    {
        if(state==PLAYING) state=PAUSED;
        else if(state==PAUSED) state=PLAYING;
        return;
    }

    if(state==PAUSED) return;

    if(key=='1') changeGun(PISTOL);
    if(key=='2') changeGun(RIFLE);
    if(key=='3') changeGun(SHOTGUN);
    if(key=='4') changeGun(SNIPER);

    if(key=='R' || key=='r') reload();
    if(key=='G' || key=='g') grenade();
}

// ============================================================
// WINDOW PROCEDURE
// ============================================================

LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
{
    switch(msg)
    {
        case WM_CREATE:
        {
            SetTimer(hwnd,TIMER_ID,16,NULL);
            loadHighScore();
            setupGuns();
            return 0;
        }

        case WM_MOUSEMOVE:
        {
            mouse.x=LOWORD(lp);
            mouse.y=HIWORD(lp);
            return 0;
        }

        case WM_LBUTTONDOWN:
        {
            int x=LOWORD(lp);
            int y=HIWORD(lp);

            mouse.x=x;
            mouse.y=y;

            if(state==MENU)
            {
                if(inside(playButton,x,y))
                    newGame();
                else if(inside(menuExitButton,x,y))
                    PostQuitMessage(0);

                return 0;
            }

            if(state==GAME_OVER)
            {
                if(inside(againButton,x,y))
                    newGame();

                return 0;
            }

            if(state==LEVEL_DONE)
            {
                level++;
                makeLevel();
                return 0;
            }

            if(state==PLAYING)
            {
                if(inside(reloadButton,x,y))
                {
                    reload();
                    return 0;
                }

                if(inside(grenadeButton,x,y))
                {
                    grenade();
                    return 0;
                }

                if(inside(pauseButton,x,y))
                {
                    state=PAUSED;
                    return 0;
                }

                if(inside(weaponButton,x,y))
                {
                    gun=(WeaponType)((gun+1)%4);
                    beep();
                    return 0;
                }

                if(inside(exitButton,x,y))
                {
                    PostQuitMessage(0);
                    return 0;
                }

                // Fire from anywhere in the play window (except the bottom
                // control bar). This makes aiming/shooting left or right work
                // naturally even when the mouse is below the battlefield.
                if(y>=TOP && y<500)
                    shoot();
            }

            return 0;
        }

        case WM_KEYDOWN:
            keyDown(wp);
            return 0;

        case WM_TIMER:
        {
            DWORD now=GetTickCount();

            if(state==COUNTDOWN)
            {
                if(now-countdownStart>=1000)
                {
                    countdownStart=now;
                    countdown--;

                    if(countdown<=0)
                    {
                        state=PLAYING;
                        levelStart=now;
                    }
                }
            }

            else if(state==PLAYING)
            {
                updateReload();
                updateBullets();
                updateEnemies();
                updateParticles();
                updatePowerUps();

                if(recoil>0)
                {
                    recoil-=1.5f;
                    if(recoil<0) recoil=0;
                }

                if(muzzle && now-muzzleStart>=MUZZLE_TIME)
                    muzzle=false;

                if(rapid && now-rapidStart>=RAPID_TIME)
                    rapid=false;

                // Time limit: lose 10 HP instead of abruptly ending.
                if(now-levelStart>=LEVEL_TIME*1000)
                {
                    levelStart=now;

                    if(!shield)
                        playerHP-=10;

                    combo=0;
                }

                if(playerHP<=0)
                {
                    playerHP=0;
                    saveHighScore();
                    state=GAME_OVER;
                }
                else if(levelClear())
                {
                    score += level*100;
                    saveHighScore();
                    state=LEVEL_DONE;
                }
            }

            InvalidateRect(hwnd,NULL,FALSE);
            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC dc=BeginPaint(hwnd,&ps);

            if(!backDC)
            {
                backDC=CreateCompatibleDC(dc);
                backBmp=CreateCompatibleBitmap(dc,W,H);
                SelectObject(backDC,backBmp);
            }

            HBRUSH clear=CreateSolidBrush(RGB(16,19,27));
            RECT r={0,0,W,H};
            FillRect(backDC,&r,clear);
            DeleteObject(clear);

            if(state==MENU) drawMenu(backDC);
            else if(state==COUNTDOWN) drawCountdown(backDC);
            else if(state==PLAYING) drawPlaying(backDC);
            else if(state==PAUSED) drawPause(backDC);
            else if(state==LEVEL_DONE) drawLevelDone(backDC);
            else if(state==GAME_OVER) drawGameOver(backDC);

            BitBlt(dc,0,0,W,H,backDC,0,0,SRCCOPY);

            EndPaint(hwnd,&ps);
            return 0;
        }

        case WM_DESTROY:
        {
            KillTimer(hwnd,TIMER_ID);
            saveHighScore();

            if(backBmp) DeleteObject(backBmp);
            if(backDC) DeleteDC(backDC);

            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProcA(hwnd,msg,wp,lp);
}

// ============================================================
// WINMAIN
// ============================================================

int WINAPI WinMain(HINSTANCE hInst,HINSTANCE,LPSTR,int show)
{
    srand((unsigned int)time(NULL));

    fBig=CreateFontA(42,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,
                     ANSI_CHARSET,OUT_DEFAULT_PRECIS,
                     CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
                     DEFAULT_PITCH|FF_SWISS,"Arial");

    fMed=CreateFontA(24,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,
                     ANSI_CHARSET,OUT_DEFAULT_PRECIS,
                     CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
                     DEFAULT_PITCH|FF_SWISS,"Arial");

    fSmall=CreateFontA(16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
                       ANSI_CHARSET,OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
                       DEFAULT_PITCH|FF_SWISS,"Arial");

    fButton=CreateFontA(15,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,
                        ANSI_CHARSET,OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
                        DEFAULT_PITCH|FF_SWISS,"Arial");

    WNDCLASSA wc;
    ZeroMemory(&wc,sizeof(wc));

    wc.style=CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc=WndProc;
    wc.hInstance=hInst;
    wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName="GunShootingGame";

    RegisterClassA(&wc);

    HWND hwnd=CreateWindowA(
        "GunShootingGame",
        "Enemy Shooting Game - Gun Edition",
        WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
        CW_USEDEFAULT,CW_USEDEFAULT,
        W+16,H+39,
        NULL,NULL,hInst,NULL
    );

    if(!hwnd) return 0;

    ShowWindow(hwnd,show);
    UpdateWindow(hwnd);

    MSG msg;

    while(GetMessage(&msg,NULL,0,0)>0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DeleteObject(fBig);
    DeleteObject(fMed);
    DeleteObject(fSmall);
    DeleteObject(fButton);

    return (int)msg.wParam;
}

