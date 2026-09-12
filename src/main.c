#include <eadk.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#define W 320
#define H 240
#define PLAY_H 222
#define MAX_PLAT 14
#define MAX_ENEMIES 10
#define MAX_BULLETS 12
#define LEVELS 20
#define FPS_MS 20

const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "RUINS SHARDS";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;

typedef struct { int x,y,w,h; } Rect;
typedef struct { float x,y,vx,vy; int w,h; bool alive,grounded; } Actor;
typedef struct { float x,y,vx,vy; bool alive; } Bullet;
typedef struct { int x,y,w,h; } Platform;

static const eadk_color_t BG = (eadk_color_t)0x10C2;
static const eadk_color_t BG2 = (eadk_color_t)0x1965;
static const eadk_color_t GRID = (eadk_color_t)0x224A;
static const eadk_color_t CYAN = (eadk_color_t)0x07FF;
static const eadk_color_t CYAN2 = (eadk_color_t)0x05B9;
static const eadk_color_t BLUE = (eadk_color_t)0x051F;
static const eadk_color_t WHITE = (eadk_color_t)0xFFFF;
static const eadk_color_t RED = (eadk_color_t)0xF146;
static const eadk_color_t YELLOW = (eadk_color_t)0xFFE0;
static const eadk_color_t STEEL = (eadk_color_t)0x52AA;
static const eadk_color_t BLACK = (eadk_color_t)0x0000;

static Platform platforms[MAX_PLAT];
static Actor enemies[MAX_ENEMIES];
static Bullet bullets[MAX_BULLETS];
static int platform_count, enemy_count, bullet_count;
static int level_index = 0;
static int deaths = 0;
static bool level_complete = false;

static bool overlap(float ax,float ay,int aw,int ah,float bx,float by,int bw,int bh) {
  return ax < bx+bw && bx < ax+aw && ay < by+bh && by < ay+ah;
}

static void rect(int x,int y,int w,int h,eadk_color_t c) {
  if (w <= 0 || h <= 0) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x+w > W) w = W-x;
  if (y+h > H) h = H-y;
  if (w > 0 && h > 0) eadk_display_push_rect_uniform((eadk_rect_t){x,y,w,h},c);
}

static void text(const char *s,int x,int y,eadk_color_t fg,eadk_color_t bg) {
  eadk_display_draw_string(s,(eadk_point_t){x,y},false,fg,bg);
}

static void background(void) {
  rect(0,0,W,H,BG);
  for (int y=20;y<PLAY_H;y+=50) rect(0,y,W,1,GRID);
  for (int x=20;x<W;x+=40) rect(x,20,1,PLAY_H-20,GRID);
  for (int i=0;i<32;i++) {
    int x=(i*73+17)%W, y=25+(i*47)%185;
    rect(x,y,1,1,CYAN2);
  }
  rect(3,32,7,1,CYAN2); rect(3,33,1,7,CYAN2);
  rect(310,65,7,1,CYAN2); rect(316,66,1,8,CYAN2);
  rect(3,145,8,1,CYAN2); rect(3,146,1,8,CYAN2);
}

static void draw_platform(Platform p) {
  rect(p.x,p.y,p.w,p.h,STEEL);
  rect(p.x,p.y,p.w,2,CYAN);
  rect(p.x,p.y,2,p.h,CYAN2);
  rect(p.x+p.w-2,p.y,2,p.h,CYAN2);
  if (p.w > 25) rect(p.x+5,p.y+4,p.w-10,1,BLUE);
}

static void draw_player(Actor *a) {
  int x=(int)a->x,y=(int)a->y;
  rect(x+2,y,7,1,CYAN2);
  rect(x,y+2,1,7,CYAN2); rect(x+9,y+2,1,7,CYAN2);
  rect(x+2,y+1,7,8,CYAN2);
  rect(x+3,y+4,5,4,CYAN);
  rect(x+2,y+1,7,3,STEEL);
  rect(x+4,y+2,3,1,WHITE);
  rect(x+5,y+5,2,2,WHITE);
  rect(x+2,y+9,3,1,CYAN2); rect(x+7,y+9,2,1,CYAN2);
}

static void draw_enemy(Actor *a) {
  int x=(int)a->x,y=(int)a->y;
  rect(x+2,y,7,1,(eadk_color_t)0x9008);
  rect(x,y+2,1,7,(eadk_color_t)0x9008);
  rect(x+9,y+2,1,7,(eadk_color_t)0x9008);
  rect(x+2,y+1,7,8,RED);
  rect(x+3,y+4,5,4,(eadk_color_t)0x7808);
  rect(x+3,y+2,2,2,WHITE); rect(x+7,y+2,2,2,WHITE);
  rect(x+5,y+5,1,2,YELLOW);
}

static void draw_bullet(Bullet *b) {
  /* Only the current bullet is drawn. The whole playfield is redrawn
     every frame, so there is intentionally NO trail. */
  int x=(int)b->x,y=(int)b->y;
  rect(x,y,4,3,WHITE);
  rect(x+1,y,2,2,CYAN);
}

static void draw_door(int x,int y) {
  rect(x,y,15,15,BLUE);
  rect(x+3,y+2,9,10,BLACK);
  rect(x+5,y+3,5,2,CYAN);
  rect(x+5,y+10,5,2,CYAN);
  rect(x+3,y+5,2,5,CYAN2);
  rect(x+10,y+5,2,5,CYAN2);
}

/* 20 deliberately different layouts. Each level is generated from a
   compact pattern: start platform, mid platforms, and a goal platform. */
static void make_level(int n, Actor *player, int *door_x, int *door_y) {
  platform_count=0; enemy_count=0; bullet_count=0;
  for (int i=0;i<MAX_ENEMIES;i++) enemies[i].alive=false;
  for (int i=0;i<MAX_BULLETS;i++) bullets[i].alive=false;

  int ground_y = 205;
  platforms[platform_count++] = (Platform){0,ground_y,58,17};

  int patterns[LEVELS][6] = {
    {72,175,70,135,72,95},{60,185,42,150,88,110},
    {80,160,55,115,75,70},{52,180,80,135,45,82},
    {70,150,45,105,100,65},{55,170,70,125,55,75},
    {90,185,40,140,95,90},{45,160,75,105,60,65},
    {75,185,50,125,90,75},{55,150,90,105,50,62},
    {40,175,55,130,100,85},{70,185,80,145,45,70},
    {60,155,65,110,80,60},{85,175,45,125,90,78},
    {50,185,75,140,55,92},{70,160,45,115,105,72},
    {45,180,95,130,55,60},{80,170,55,115,95,85},
    {55,155,80,100,60,68},{75,185,45,135,105,78}
  };
  int *p=patterns[n%LEVELS];

  int x1=p[0], y1=p[1], w1=42+(n%3)*10;
  int x2=p[2], y2=p[3], w2=48+(n%4)*8;
  int x3=p[4], y3=p[5], w3=45+(n%5)*7;

  platforms[platform_count++] = (Platform){x1,y1,w1,9};
  platforms[platform_count++] = (Platform){x2,y2,w2,9};
  platforms[platform_count++] = (Platform){x3,y3,w3,9};

  int gx = 245 - (n%3)*12;
  int gy = 72 + (n%5)*10;
  platforms[platform_count++] = (Platform){gx,gy,60,9};

  /* Extra challenge platforms vary by level. */
  if (n%2==0) platforms[platform_count++] = (Platform){150,185-(n%4)*10,38,8};
  if (n%3==0) platforms[platform_count++] = (Platform){190,125+(n%2)*18,42,8};
  if (n%4==0) platforms[platform_count++] = (Platform){110,75+(n%3)*14,35,8};
  if (n%5==0) platforms[platform_count++] = (Platform){210,185,35,8};

  player->x=8; player->y=ground_y-10; player->vx=0; player->vy=0;
  player->w=10; player->h=10; player->alive=true; player->grounded=true;

  *door_x=gx+22; *door_y=gy-15;

  /* Enemy positions are intentionally varied and some levels have more. */
  int ec=2+(n%4);
  if (n>=12) ec++;
  if (ec>MAX_ENEMIES) ec=MAX_ENEMIES;
  for (int i=0;i<ec;i++) {
    Platform q=platforms[1+(i%(platform_count-1))];
    enemies[i]=(Actor){q.x+8+(i*13)%(q.w-16),q.y-10,0,0,10,10,true,true};
    enemy_count++;
  }
}

static void draw_scene(Actor *player,int dx,int dy) {
  background();
  for (int i=0;i<platform_count;i++) draw_platform(platforms[i]);
  draw_door(dx,dy);
  for (int i=0;i<enemy_count;i++) if (enemies[i].alive) draw_enemy(&enemies[i]);
  for (int i=0;i<bullet_count;i++) if (bullets[i].alive) draw_bullet(&bullets[i]);
  if (player->alive) draw_player(player);
}

static void hud(int level) {
  char s[32];
  text("RUINS SHARDS",5,4,CYAN,BG);
  snprintf(s,sizeof(s),"LV %02d",level+1);
  text(s,115,4,WHITE,BG);
  snprintf(s,sizeof(s),"DEATHS %d",deaths);
  text(s,230,4,RED,BG);
}

static bool key(eadk_keyboard_state_t k, eadk_key_t code) {
  return eadk_keyboard_key_down(k,code);
}

static void title(void) {
  background();
  text("SYSTEM // RUIN",10,28,CYAN,BG);
  text("SECURITY LEVEL: UNKNOWN",10,48,CYAN2,BG);
  text("RUINS SHARDS",92,90,WHITE,BG);
  rect(95,108,130,2,CYAN);
  text("BY ASHER",126,126,CYAN,BG);
  text("OK = START",122,176,WHITE,BG);
  while (true) {
    eadk_keyboard_state_t k=eadk_keyboard_scan();
    if (key(k,eadk_key_ok)) break;
    eadk_timing_msleep(40);
  }
  while (key(eadk_keyboard_scan(),eadk_key_ok)) eadk_timing_msleep(20);
}

static void transition(void) {
  rect(0,0,W,H,BG);
  for (int i=0;i<8;i++) {
    rect(0,110-i*8,W,2,CYAN2);
    rect(0,110+i*8,W,2,BLUE);
    eadk_timing_msleep(12);
  }
}

static void death_screen(void) {
  background();
  rect(50,62,220,112,BG2);
  rect(50,62,220,2,RED);
  text("SYSTEM FAILURE",94,82,RED,BG2);
  text("YOU DIED",119,108,WHITE,BG2);
  char s[32]; snprintf(s,sizeof(s),"DEATHS: %d",deaths);
  text(s,105,134,RED,BG2);
  eadk_timing_msleep(600);
}

static void victory(void) {
  background();
  text("MISSION COMPLETE",84,62,CYAN,BG);
  rect(78,78,164,2,CYAN);
  text("20 LEVELS CLEARED",91,100,WHITE,BG);
  char s[32]; snprintf(s,sizeof(s),"DEATHS %d",deaths);
  text(s,110,124,RED,BG);
  text("PRESS OK",119,178,WHITE,BG);
  while (!key(eadk_keyboard_scan(),eadk_key_ok)) eadk_timing_msleep(30);
  while (key(eadk_keyboard_scan(),eadk_key_ok)) eadk_timing_msleep(20);
}

static void play_level(int n) {
  Actor player;
  int door_x,door_y;
  make_level(n,&player,&door_x,&door_y);
  level_complete = false;
  int cooldown=0;

  while (true) {
    eadk_keyboard_state_t k=eadk_keyboard_scan();
    if (key(k,eadk_key_back)) return;

    if (key(k,eadk_key_left)) player.vx-=0.22f;
    if (key(k,eadk_key_right)) player.vx+=0.22f;
    if (player.vx>2.2f) player.vx=2.2f;
    if (player.vx<-2.2f) player.vx=-2.2f;

    if (key(k,eadk_key_ok) && player.grounded) {
      player.vy=-4.7f; player.grounded=false;
    }

    if (cooldown>0) cooldown--;
    if (key(k,eadk_key_shift) && cooldown==0 && bullet_count<MAX_BULLETS) {
      Bullet *b=&bullets[bullet_count++];
      b->x=player.x+10; b->y=player.y+4;
      b->vx=6.0f+player.vx; b->vy=(key(k,eadk_key_up)?-1.5f:(key(k,eadk_key_down)?1.5f:0));
      b->alive=true;
      cooldown=9;
    }

    player.vy+=0.20f;
    if (player.vy>4.2f) player.vy=4.2f;
    player.x+=player.vx; player.y+=player.vy;
    player.vx*=0.82f;
    if (player.x<0) player.x=0;
    if (player.x>W-player.w) player.x=W-player.w;

    player.grounded=false;
    for (int i=0;i<platform_count;i++) {
      Platform *p=&platforms[i];
      if (player.vy>=0 && player.x+player.w>p->x && player.x<p->x+p->w &&
          player.y+player.h>=p->y && player.y+player.h<=p->y+10) {
        player.y=p->y-player.h; player.vy=0; player.grounded=true;
      }
    }

    for (int i=0;i<enemy_count;i++) if (enemies[i].alive) {
      Actor *e=&enemies[i];
      /* Simple platform patrol. */
      if ((int)(eadk_random()%80)==0) e->vx=(eadk_random()%2)?0.65f:-0.65f;
      e->x+=e->vx;
      if (e->x<0) { e->x=0; e->vx=0.65f; }
      if (e->x>W-e->w) { e->x=W-e->w; e->vx=-0.65f; }
      if (overlap(player.x,player.y,player.w,player.h,e->x,e->y,e->w,e->h)) {
        deaths++; death_screen(); return;
      }
    }

    for (int i=0;i<bullet_count;i++) if (bullets[i].alive) {
      Bullet *b=&bullets[i];
      b->x+=b->vx; b->y+=b->vy;
      if (b->x<-10 || b->x>W+10 || b->y<-10 || b->y>PLAY_H+10) {
        b->alive=false; continue;
      }
      for (int p=0;p<platform_count;p++) {
        if (overlap(b->x,b->y,4,3,platforms[p].x,platforms[p].y,platforms[p].w,platforms[p].h)) {
          b->alive=false; break;
        }
      }
      if (!b->alive) continue;
      for (int e=0;e<enemy_count;e++) if (enemies[e].alive) {
        if (overlap(b->x,b->y,4,3,enemies[e].x,enemies[e].y,10,10)) {
          enemies[e].alive=false; b->alive=false; break;
        }
      }
    }

    /* Compact dead bullets without leaving visual trails. */
    int nb=0;
    for (int i=0;i<bullet_count;i++) if (bullets[i].alive) bullets[nb++]=bullets[i];
    bullet_count=nb;

    if (overlap(player.x,player.y,player.w,player.h,door_x,door_y,15,15)) { level_complete=true; return; }

    if (player.y>PLAY_H+10) { deaths++; death_screen(); return; }

    draw_scene(&player,door_x,door_y);
    hud(n);
    eadk_timing_msleep(FPS_MS);
  }
}

int main(int argc,char *argv[]) {
  (void)argc; (void)argv;
  title();
  transition();
  while (true) {
    if (level_index>=LEVELS) {
      victory();
      level_index=0;
      deaths=0;
      transition();
    }
    play_level(level_index);
    if (level_complete) {
      level_index++;
      transition();
    }
  }
}
