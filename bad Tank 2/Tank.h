#pragma once
#include "Hardware.h"
#include "GameAPI.h"
#include "MusicPlayer.h"
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <math.h>
#include <string.h>

namespace Tank {

// ============================================================
// TANK - JakeBoy first-person campaign
// ============================================================
#define TANK_DEBUG_PERF 0

#define screen display

constexpr int SW = 240, SH = 320;
constexpr int VIEW_H = 274;
constexpr int COCKPIT_Y = 274;
constexpr int HORIZON = 104;
constexpr float PI2 = 6.28318530718f;
constexpr float FOV = 1.04719755f; // 60 degrees
constexpr float HALF_FOV = FOV * 0.5f;
constexpr float FOCAL = 207.8f;
constexpr float NEAR_Z = 1.0f;
constexpr float FAR_Z = 115.0f;
constexpr float CAMERA_H = 2.0f;
constexpr float PLAYER_RADIUS = 1.4f;
constexpr float FORWARD_SPEED = 10.0f;
constexpr float REVERSE_SPEED = 7.0f;
constexpr float TURN_RATE = 1.25f;
constexpr float PIVOT_RATE = 1.9f;
// Physics/input response runs at 60 Hz so camera motion does not advance in
// coarse 33 ms jumps. Rendering targets ~30 FPS, matching Deep Vector's
// proven 33 ms display cadence while leaving SPI headroom for the denser world.
constexpr uint32_t SIM_US = 16667UL;
constexpr uint32_t RENDER_MS = 33UL; // ~30 fps target
constexpr uint8_t MAX_STEPS = 3;
constexpr uint32_t SWITCH_DEBOUNCE_MS = 20;
constexpr uint32_t SWITCH_ERROR_CENTER_MS = 120;
constexpr uint32_t CANNON_RELOAD_MS = 1500;
constexpr uint32_t INVULN_MS = 3000;
constexpr uint32_t DEATH_MS = 1100;
constexpr int MAX_HEALTH = 100;
constexpr int START_LIVES = 10;
constexpr int MAX_SOLDIERS = 18;
constexpr int MAX_TANKS = 8;
constexpr int MAX_SHELLS = 8;
constexpr int MAX_EFFECTS = 20;
constexpr int MAX_MISSILES = 3;
constexpr int MAX_VISIBLE = 44;

// RGB565 palette
constexpr uint16_t C_SKY=0x5D9F, C_SKY_SMOKE=0x8410, C_SAND=0xC5EA, C_DIRT=0x8B85;
constexpr uint16_t C_ROAD=0x528A, C_GRASS=0x6506, C_BRICK=0x8A43, C_BRICK_DARK=0x4922;
constexpr uint16_t C_CONCRETE=0x8410, C_DARK=0x2104, C_METAL=0x4208, C_METAL_HI=0x7BEF;
constexpr uint16_t C_OLIVE=0x4B42, C_FIRE=0xFD20, C_SMOKE=0x630C, C_WHITE=0xFFFF;
constexpr uint16_t C_RED=0xF800, C_DARK_RED=0x6000, C_GREEN=0x07E0, C_YELLOW=0xFFE0;
constexpr uint16_t C_WATER=0x041F, C_BLACK=0x0000, C_BROWN=0x6203, C_RUBBLE=0x5AEB;

struct V2 { float x,y; };
struct Rect { float x0,y0,x1,y1; };

enum GameState:uint8_t { INTRO, PLAYING, DEAD, VICTORY, GAMEOVER };
enum Zone:uint8_t { BEACH, DEFENSE, TRENCHES, TOWN, CITY, PURSUIT, ZONE_COUNT };
enum ObjType:uint8_t { O_BUILDING,O_BUNKER,O_WALL,O_WRECK,O_CRATER,O_HEDGEHOG,O_SANDBAG,O_POLE,O_FIRE,O_SMOKE,O_ARTILLERY,O_TREE,O_LANDING,O_RUBBLE };
enum SoldierState:uint8_t { S_IDLE,S_RUN,S_FLEE,S_DEAD };
enum TankKind:uint8_t { T_LIGHT,T_HOLDER,T_MANEUVER,T_HEAVY };
enum TankState:uint8_t { T_GUARD,T_ADVANCE,T_AIM,T_COOLDOWN,T_DEAD };
enum BossPhase:uint8_t { B_INACTIVE,B_WALKING,B_COLLAPSING,B_TANKS,B_CORE,B_DESTROYED };
enum FxType:uint8_t { FX_MG,FX_BLAST,FX_SPARK,FX_DUST,FX_BIGBLAST };

struct DebouncedSwitch { SwitchState raw, previous, stable; uint32_t changed,errorAt; bool error; };
struct StaticObj { int16_t x,y; uint8_t type; uint8_t size; };
struct Spawn { int16_t x,y; uint8_t kind; };
struct Soldier { bool active; V2 p,target; SoldierState state; uint8_t frame; uint32_t nextChange; };
struct EnemyTank { bool active; V2 p,home; float heading; TankKind kind; TankState state; int hp; uint32_t nextFire,stateUntil; };
struct Shell { bool active; V2 p,v; uint32_t dieAt; };
struct Effect { bool active; FxType type; V2 p; uint32_t start,duration; };
struct Missile { bool active; V2 target; uint32_t warnAt,impactAt; float radius; };
struct Player { V2 p; float heading; int hp,lives; uint32_t cannonReady,invulnUntil; float recoil; };
struct BossLeg { int hp; bool dead; };
struct Fortress { BossPhase phase; V2 p; float heading; BossLeg leg[3]; uint32_t nextMissile; uint32_t phaseAt; float collapse; int defenders; bool coreOpen; };
struct Projected { int16_t x,y; float depth,scale; bool visible; };
struct Visible { uint8_t cls,index; float depth; };

// ============================================================
// Authored zone content. Coordinates are local to each zone.
// Campaign runs +Y; each zone is ~95 units long and 42-54 wide.
// ============================================================
#define SO(x,y,t,s) {x,y,t,s}
static const StaticObj beachObjs[] PROGMEM = {
 SO(-15,8,O_LANDING,4),SO(14,10,O_LANDING,3),SO(-10,18,O_HEDGEHOG,2),SO(-3,20,O_HEDGEHOG,2),SO(6,19,O_HEDGEHOG,2),SO(14,23,O_HEDGEHOG,2),
 SO(-17,29,O_CRATER,3),SO(8,31,O_CRATER,2),SO(-8,37,O_WRECK,3),SO(17,40,O_SMOKE,3),SO(-18,49,O_BUNKER,5),SO(17,51,O_BUNKER,5),
 SO(-7,48,O_SANDBAG,3),SO(1,50,O_SANDBAG,3),SO(9,48,O_SANDBAG,3),SO(-13,60,O_FIRE,2),SO(12,62,O_WRECK,3),SO(-4,69,O_CRATER,3),
 SO(15,73,O_SMOKE,4),SO(-16,78,O_RUBBLE,3),SO(0,82,O_WALL,5),SO(13,84,O_ARTILLERY,3)
};
static const StaticObj defenseObjs[] PROGMEM = {
 SO(-18,8,O_WALL,5),SO(-9,10,O_SANDBAG,3),SO(10,10,O_SANDBAG,3),SO(18,9,O_WALL,5),SO(-14,19,O_ARTILLERY,3),SO(13,22,O_BUNKER,5),
 SO(-4,27,O_CRATER,3),SO(8,31,O_WRECK,3),SO(-17,39,O_SMOKE,4),SO(17,43,O_FIRE,2),SO(-8,49,O_HEDGEHOG,2),SO(0,51,O_HEDGEHOG,2),SO(8,49,O_HEDGEHOG,2),
 SO(-15,61,O_BUNKER,5),SO(15,64,O_BUNKER,5),SO(-4,69,O_ARTILLERY,3),SO(7,73,O_RUBBLE,3),SO(-13,82,O_WRECK,3),SO(15,86,O_SMOKE,4)
};
static const StaticObj trenchObjs[] PROGMEM = {
 SO(-18,8,O_WALL,5),SO(17,12,O_WALL,5),SO(-14,18,O_SANDBAG,3),SO(8,21,O_BUNKER,5),SO(-4,28,O_ARTILLERY,3),SO(15,31,O_CRATER,3),
 SO(-17,39,O_WALL,5),SO(-8,42,O_SANDBAG,3),SO(12,45,O_WALL,5),SO(3,50,O_WRECK,3),SO(-14,58,O_SMOKE,3),SO(16,61,O_BUNKER,5),
 SO(-5,68,O_CRATER,3),SO(8,72,O_SANDBAG,3),SO(-17,79,O_ARTILLERY,3),SO(14,82,O_FIRE,2),SO(0,88,O_RUBBLE,4)
};
static const StaticObj townObjs[] PROGMEM = {
 SO(-18,9,O_BUILDING,6),SO(18,11,O_BUILDING,6),SO(-17,27,O_BUILDING,7),SO(17,29,O_BUILDING,6),SO(-4,23,O_WRECK,3),SO(8,35,O_CRATER,2),
 SO(-18,45,O_BUILDING,7),SO(18,47,O_BUILDING,7),SO(-11,51,O_FIRE,2),SO(7,55,O_WRECK,3),SO(-18,66,O_BUILDING,6),SO(18,68,O_BUILDING,8),
 SO(-7,72,O_RUBBLE,3),SO(11,77,O_SMOKE,4),SO(-18,86,O_BUILDING,8),SO(18,87,O_BUILDING,7),SO(0,91,O_POLE,3)
};
static const StaticObj cityObjs[] PROGMEM = {
 SO(-19,8,O_BUILDING,8),SO(19,9,O_BUILDING,9),SO(-18,24,O_BUILDING,10),SO(18,28,O_BUILDING,8),SO(-7,31,O_RUBBLE,4),SO(8,36,O_WRECK,3),
 SO(-19,45,O_BUILDING,9),SO(19,48,O_BUILDING,10),SO(-10,52,O_FIRE,3),SO(12,58,O_SMOKE,5),SO(-18,65,O_BUILDING,11),SO(18,68,O_BUILDING,9),
 SO(-4,70,O_CRATER,3),SO(6,77,O_RUBBLE,4),SO(-19,84,O_BUILDING,10),SO(19,86,O_BUILDING,11),SO(-11,90,O_FIRE,2),SO(10,92,O_WRECK,3)
};
static const StaticObj pursuitObjs[] PROGMEM = {
 SO(-22,10,O_CRATER,4),SO(16,14,O_WRECK,4),SO(-11,22,O_ARTILLERY,3),SO(22,27,O_SMOKE,5),SO(-20,38,O_BUNKER,4),SO(8,42,O_CRATER,4),
 SO(20,51,O_WRECK,4),SO(-14,58,O_FIRE,3),SO(3,64,O_RUBBLE,4),SO(-22,72,O_SMOKE,5),SO(17,78,O_ARTILLERY,3),SO(-7,87,O_CRATER,4)
};

static const Spawn beachSoldiers[] PROGMEM={{-9,25,0},{4,27,0},{12,34,0},{-14,47,0},{6,55,0},{-5,72,0}};
static const Spawn defenseSoldiers[] PROGMEM={{-10,14,0},{7,17,0},{-15,35,0},{11,39,0},{-4,58,0},{12,72,0},{-11,81,0}};
static const Spawn trenchSoldiers[] PROGMEM={{-12,13,0},{10,18,0},{-5,29,0},{14,37,0},{-13,48,0},{5,57,0},{12,69,0},{-7,80,0}};
static const Spawn townSoldiers[] PROGMEM={{-10,18,0},{9,25,0},{-12,39,0},{12,46,0},{-6,59,0},{10,70,0},{-11,82,0}};
static const Spawn citySoldiers[] PROGMEM={{-10,16,0},{11,22,0},{-12,36,0},{10,43,0},{-8,56,0},{12,64,0},{-10,75,0},{7,86,0}};
static const Spawn noSoldiers[] PROGMEM={{0,0,0}};

static const Spawn beachTanks[] PROGMEM={{8,67,T_LIGHT}};
static const Spawn defenseTanks[] PROGMEM={{-9,45,T_HOLDER},{11,79,T_LIGHT}};
static const Spawn trenchTanks[] PROGMEM={{11,55,T_HOLDER},{-10,84,T_MANEUVER}};
static const Spawn townTanks[] PROGMEM={{8,42,T_MANEUVER},{-9,76,T_HOLDER}};
static const Spawn cityTanks[] PROGMEM={{-9,35,T_HEAVY},{10,69,T_MANEUVER},{0,88,T_HOLDER}};
static const Spawn noTanks[] PROGMEM={{0,0,0}};

struct ZoneDef { const StaticObj* objs; uint8_t objCount; const Spawn* soldiers; uint8_t soldierCount; const Spawn* tanks; uint8_t tankCount; float halfWidth,length; uint16_t ground,sky; };
static const ZoneDef zones[] = {
 {beachObjs,(uint8_t)(sizeof(beachObjs)/sizeof(*beachObjs)),beachSoldiers,(uint8_t)(sizeof(beachSoldiers)/sizeof(*beachSoldiers)),beachTanks,1,24,96,C_SAND,C_SKY},
 {defenseObjs,(uint8_t)(sizeof(defenseObjs)/sizeof(*defenseObjs)),defenseSoldiers,(uint8_t)(sizeof(defenseSoldiers)/sizeof(*defenseSoldiers)),defenseTanks,2,23,96,C_DIRT,C_SKY_SMOKE},
 {trenchObjs,(uint8_t)(sizeof(trenchObjs)/sizeof(*trenchObjs)),trenchSoldiers,(uint8_t)(sizeof(trenchSoldiers)/sizeof(*trenchSoldiers)),trenchTanks,2,22,96,C_DIRT,C_SKY_SMOKE},
 {townObjs,(uint8_t)(sizeof(townObjs)/sizeof(*townObjs)),townSoldiers,(uint8_t)(sizeof(townSoldiers)/sizeof(*townSoldiers)),townTanks,2,22,96,C_ROAD,C_SKY_SMOKE},
 {cityObjs,(uint8_t)(sizeof(cityObjs)/sizeof(*cityObjs)),citySoldiers,(uint8_t)(sizeof(citySoldiers)/sizeof(*citySoldiers)),cityTanks,3,22,96,C_ROAD,0x4208},
 {pursuitObjs,(uint8_t)(sizeof(pursuitObjs)/sizeof(*pursuitObjs)),noSoldiers,0,noTanks,0,28,115,C_DIRT,0x528A}
};

// ============================================================
// Runtime state
// ============================================================
GameState gameState=INTRO; Zone zone=BEACH; Player player;
DebouncedSwitch leftSw,rightSw; SwitchState leftState=SWITCH_CENTER,rightState=SWITCH_CENTER;
Soldier soldiers[MAX_SOLDIERS]; EnemyTank tanks[MAX_TANKS]; Shell shells[MAX_SHELLS]; Effect effects[MAX_EFFECTS]; Missile missiles[MAX_MISSILES];
Fortress boss; Visible visible[MAX_VISIBLE]; uint8_t visibleCount=0;
uint32_t stateAt=0,lastUs=0,accumUs=0,lastRender=0,lastPerf=0; uint16_t frames=0;
enum SoundKind:uint8_t { SND_NONE,SND_MG,SND_CANNON,SND_HIT,SND_WARN,SND_BOOM,SND_READY };
struct SoundState { SoundKind kind; uint32_t until; }; SoundState sound={SND_NONE,0};
bool lastLeftButton=false,lastRightButton=false; uint8_t mgShots=0; uint32_t mgNextShot=0,mgNextBurst=0;
int lastHudHp=-1,lastHudLives=-1; bool lastReady=false; int lastCompass=-99; bool cockpitDrawn=false;

// ============================================================
// Utility
// ============================================================
float clampf(float v,float a,float b){return v<a?a:(v>b?b:v);} 
float sqr(float v){return v*v;} float dist2(V2 a,V2 b){return sqr(a.x-b.x)+sqr(a.y-b.y);} 
float wrapAngle(float a){while(a>PI)a-=PI2;while(a<-PI)a+=PI2;return a;}
V2 forward(float h){return {sinf(h),cosf(h)};} 
uint16_t shade(uint16_t c,uint8_t n){ if(n==0)return c; uint16_t r=(c>>11)&31,g=(c>>5)&63,b=c&31; r=(r*(8-n))/8;g=(g*(8-n))/8;b=(b*(8-n))/8;return (r<<11)|(g<<5)|b; }

// ============================================================
// Switch input - adapted from working Ice Cold Beer implementation
// ============================================================
SwitchState rawSwitch(int up,int down){int u=digitalRead(up),d=digitalRead(down);if(u==LOW&&d==HIGH)return SWITCH_UP;if(u==HIGH&&d==HIGH)return SWITCH_CENTER;if(u==HIGH&&d==LOW)return SWITCH_DOWN;return SWITCH_ERROR;}
void initSwitch(DebouncedSwitch& s,int up,int down){uint32_t n=millis();SwitchState v=rawSwitch(up,down);s.raw=s.previous=v;s.changed=s.errorAt=n;s.error=(v==SWITCH_ERROR);s.stable=s.error?SWITCH_CENTER:v;}
void updateSwitch(DebouncedSwitch& s,int up,int down,uint32_t n){SwitchState v=rawSwitch(up,down);s.raw=v;if(v==SWITCH_ERROR){if(!s.error){s.error=true;s.errorAt=n;}if(n-s.errorAt>=SWITCH_ERROR_CENTER_MS)s.stable=SWITCH_CENTER;return;}bool was=s.error;s.error=false;if(was){s.previous=v;s.changed=n;return;}if(v!=s.previous){s.previous=v;s.changed=n;}if(n-s.changed>=SWITCH_DEBOUNCE_MS)s.stable=v;}
// Physical Tank switches are mounted opposite the logical labels used by the
// original readout. Invert once here; all differential-drive math stays sane.
float tread(SwitchState s){return s==SWITCH_UP?-1.0f:(s==SWITCH_DOWN?1.0f:0.0f);} 

// ============================================================
// Collision helpers
// ============================================================
bool objSolid(uint8_t t){return t==O_BUILDING||t==O_BUNKER||t==O_WALL||t==O_WRECK||t==O_LANDING;}
float objRadius(const StaticObj&o){switch(o.type){case O_BUILDING:return 3.5f+o.size*.35f;case O_BUNKER:return 3.2f;case O_WALL:return 2.5f;case O_WRECK:return 2.2f;case O_LANDING:return 4.0f;default:return 1.0f;}}
bool blocked(V2 p){const ZoneDef&z=zones[zone];if(fabsf(p.x)>z.halfWidth-1.2f||p.y<1||p.y>z.length+2)return true;for(uint8_t i=0;i<z.objCount;i++){StaticObj o;memcpy_P(&o,z.objs+i,sizeof(o));if(!objSolid(o.type))continue;float r=objRadius(o)+PLAYER_RADIUS;if(sqr(p.x-o.x)+sqr(p.y-o.y)<r*r)return true;}if(zone==PURSUIT&&boss.phase==B_WALKING){for(int i=0;i<3;i++)if(!boss.leg[i].dead){float a=boss.heading+(i-1)*2.0944f;V2 lp={boss.p.x+sinf(a)*7,boss.p.y+cosf(a)*7};if(dist2(p,lp)<sqr(3.2f))return true;}}return false;}

// ============================================================
// Spawning / zone loading
// ============================================================
void clearActors(){memset(soldiers,0,sizeof(soldiers));memset(tanks,0,sizeof(tanks));memset(shells,0,sizeof(shells));memset(effects,0,sizeof(effects));memset(missiles,0,sizeof(missiles));}
void spawnSoldier(V2 p){for(auto &s:soldiers)if(!s.active){s.active=true;s.p=p;s.target=p;s.state=S_IDLE;s.nextChange=millis()+500+(esp_random()%1200);return;}}
int tankHp(TankKind k){return k==T_LIGHT?1:(k==T_HEAVY?4:2);} 
void spawnTank(V2 p,TankKind k){for(auto&t:tanks)if(!t.active){t.active=true;t.p=t.home=p;t.kind=k;t.state=T_GUARD;t.hp=tankHp(k);t.heading=PI;t.nextFire=millis()+1200+(esp_random()%1000);return;}}
void loadZone(Zone z,bool resetPlayer=true){zone=z;clearActors();const ZoneDef&d=zones[z];for(uint8_t i=0;i<d.soldierCount;i++){Spawn s;memcpy_P(&s,d.soldiers+i,sizeof(s));spawnSoldier({(float)s.x,(float)s.y});}for(uint8_t i=0;i<d.tankCount;i++){Spawn s;memcpy_P(&s,d.tanks+i,sizeof(s));spawnTank({(float)s.x,(float)s.y},(TankKind)s.kind);}if(resetPlayer){player.p={0,4};player.heading=0;}cockpitDrawn=false;lastHudHp=-1;}

// ============================================================
// Nonblocking SFX. Background music owns the buzzers when enabled; SFX
// become active when music is OFF, avoiding the chirp/frequency fight that
// occurs if two schedulers write the same LEDC channels.
// ============================================================
void stopSfx(){ if(Music::enabled)return; ledcWrite(BUZZER_1_PIN,0);ledcWrite(BUZZER_2_PIN,0);sound.kind=SND_NONE;}
void sfx(SoundKind k){ if(Music::enabled)return; uint32_t n=millis();sound.kind=k;uint32_t hz=300,dur=90;switch(k){case SND_MG:hz=760;dur=45;break;case SND_CANNON:hz=120;dur=180;break;case SND_HIT:hz=180;dur=120;break;case SND_WARN:hz=1100;dur=120;break;case SND_BOOM:hz=85;dur=260;break;case SND_READY:hz=900;dur=55;break;default:break;}ledcWriteTone(BUZZER_1_PIN,hz);ledcWrite(BUZZER_1_PIN,300);if(k==SND_CANNON||k==SND_BOOM){ledcWriteTone(BUZZER_2_PIN,hz+35);ledcWrite(BUZZER_2_PIN,220);}sound.until=n+dur;}
void updateSfx(uint32_t n){if(!Music::enabled&&sound.kind!=SND_NONE&&(int32_t)(n-sound.until)>=0)stopSfx();}

// ============================================================
// Effects / weapons
// ============================================================
void effect(FxType t,V2 p,uint32_t dur=350){for(auto&e:effects)if(!e.active){e.active=true;e.type=t;e.p=p;e.start=millis();e.duration=dur;return;}}
void killSoldier(Soldier&s){s.state=S_DEAD;s.nextChange=millis()+700;effect(FX_MG,s.p,180);}
void killTank(EnemyTank&t){t.state=T_DEAD;t.nextFire=millis()+900;effect(FX_BIGBLAST,t.p,800);}

bool segmentCircle(V2 a,V2 dir,V2 c,float r,float &outT){V2 q={c.x-a.x,c.y-a.y};float t=q.x*dir.x+q.y*dir.y;if(t<0||t>FAR_Z)return false;float px=a.x+dir.x*t,py=a.y+dir.y*t;if(sqr(px-c.x)+sqr(py-c.y)<=r*r){outT=t;return true;}return false;}
bool rayBlockedBefore(V2 a,V2 dir,float maxT){const ZoneDef&z=zones[zone];for(uint8_t i=0;i<z.objCount;i++){StaticObj o;memcpy_P(&o,z.objs+i,sizeof(o));if(!objSolid(o.type))continue;float t;if(segmentCircle(a,dir,{(float)o.x,(float)o.y},objRadius(o),t)&&t<maxT)return true;}return false;}

void cannonFire(){uint32_t n=millis();if(n<player.cannonReady||gameState!=PLAYING)return;player.cannonReady=n+CANNON_RELOAD_MS;player.recoil=1;sfx(SND_CANNON);V2 d=forward(player.heading);float best=FAR_Z;int hitType=0,hitIdx=-1;
 for(int i=0;i<MAX_TANKS;i++)if(tanks[i].active&&tanks[i].state!=T_DEAD){float t;if(segmentCircle(player.p,d,tanks[i].p,2.0f,t)&&t<best){best=t;hitType=1;hitIdx=i;}}
 if(zone==PURSUIT&&boss.phase==B_WALKING){for(int i=0;i<3;i++)if(!boss.leg[i].dead){float a=boss.heading+(i-1)*2.0944f;V2 lp={boss.p.x+sinf(a)*7,boss.p.y+cosf(a)*7};float t;if(segmentCircle(player.p,d,lp,2.8f,t)&&t<best){best=t;hitType=2;hitIdx=i;}}}
 if(zone==PURSUIT&&boss.phase==B_CORE&&boss.coreOpen){float t;if(segmentCircle(player.p,d,boss.p,2.5f,t)&&t<best){best=t;hitType=3;hitIdx=0;}}
 if(rayBlockedBefore(player.p,d,best)){effect(FX_BLAST,{player.p.x+d.x*(best*.7f),player.p.y+d.y*(best*.7f)},350);return;}
 V2 hp={player.p.x+d.x*best,player.p.y+d.y*best};effect(FX_BLAST,hp,450);
 if(hitType==1){if(--tanks[hitIdx].hp<=0)killTank(tanks[hitIdx]);}
 else if(hitType==2){BossLeg&l=boss.leg[hitIdx];l.hp--;effect(FX_BIGBLAST,hp,650);if(l.hp<=0){l.dead=true;bool all=true;for(auto &x:boss.leg)if(!x.dead)all=false;if(all){boss.phase=B_COLLAPSING;boss.phaseAt=n;boss.collapse=0;memset(missiles,0,sizeof(missiles));}}}
 else if(hitType==3){boss.phase=B_DESTROYED;boss.phaseAt=n;effect(FX_BIGBLAST,boss.p,1800);gameState=VICTORY;stateAt=n;}
}

void mgBullet(){sfx(SND_MG);V2 d=forward(player.heading+(int32_t(esp_random()%101)-50)*0.0008f);float best=70;Soldier*hs=nullptr;EnemyTank*ht=nullptr;for(auto&s:soldiers)if(s.active&&s.state!=S_DEAD){float t;if(segmentCircle(player.p,d,s.p,.8f,t)&&t<best){best=t;hs=&s;ht=nullptr;}}for(auto&t:tanks)if(t.active&&t.state!=T_DEAD){float q;if(segmentCircle(player.p,d,t.p,1.7f,q)&&q<best){best=q;ht=&t;hs=nullptr;}}if(rayBlockedBefore(player.p,d,best))return;V2 p={player.p.x+d.x*best,player.p.y+d.y*best};effect(FX_MG,p,120);if(hs)killSoldier(*hs);else if(ht&&ht->kind==T_LIGHT&&(--ht->hp<=0))killTank(*ht);}
void updateMG(bool held,bool pressed,uint32_t n){if(pressed&&n>=mgNextBurst){mgShots=5;mgNextShot=n;}if(held&&mgShots==0&&n>=mgNextBurst){mgShots=5;mgNextShot=n;}if(mgShots&&n>=mgNextShot){mgBullet();mgShots--;mgNextShot=n+70;if(!mgShots)mgNextBurst=n+300;}}

// ============================================================
// Damage / respawn
// ============================================================
void damagePlayer(int d){uint32_t n=millis();if(gameState!=PLAYING||n<player.invulnUntil)return;player.hp-=d;sfx(SND_HIT);if(player.hp<=0){player.hp=0;player.lives--;gameState=DEAD;stateAt=n;effect(FX_BIGBLAST,player.p,900);}}
void respawn(){if(player.lives<=0){gameState=GAMEOVER;stateAt=millis();return;}player.hp=MAX_HEALTH;player.invulnUntil=millis()+INVULN_MS;player.p.y=max(3.0f,player.p.y-7);player.p.x=0;for(int k=0;k<8&&blocked(player.p);k++)player.p.y=max(3.0f,player.p.y-3);gameState=PLAYING;}

// ============================================================
// Simulation
// ============================================================
void movePlayer(float dt){float l=tread(leftState),r=tread(rightState);float linear=(l+r)*.5f;float diff=r-l;float speed=linear>=0?FORWARD_SPEED:REVERSE_SPEED;float turn=(l*r<0?PIVOT_RATE:TURN_RATE)*diff*.5f;player.heading=wrapAngle(player.heading+turn*dt);V2 f=forward(player.heading);V2 old=player.p;V2 np={old.x+f.x*linear*speed*dt,old.y+f.y*linear*speed*dt};V2 tx={np.x,old.y};if(!blocked(tx))old.x=tx.x;V2 ty={old.x,np.y};if(!blocked(ty))old.y=ty.y;player.p=old;
 const ZoneDef&z=zones[zone];if(player.p.y>z.length-1&&zone<CITY){loadZone((Zone)(zone+1));}else if(player.p.y>z.length-1&&zone==CITY){loadZone(PURSUIT);boss.phase=B_WALKING;boss.p={0,76};boss.heading=0;boss.nextMissile=millis()+3500;for(auto &l:boss.leg){l.hp=3;l.dead=false;}}}

void updateSoldiers(float dt,uint32_t n){for(auto&s:soldiers)if(s.active){if(s.state==S_DEAD){if(n>=s.nextChange)s.active=false;continue;}float pd=dist2(s.p,player.p);if(pd<100&&s.state!=S_FLEE){s.state=S_FLEE;V2 away={s.p.x-player.p.x,s.p.y-player.p.y};float m=sqrtf(max(.01f,away.x*away.x+away.y*away.y));s.target={clampf(s.p.x+away.x/m*7,-20,20),clampf(s.p.y+away.y/m*7,2,92)};}else if(n>=s.nextChange&&s.state!=S_FLEE){s.state=S_RUN;s.target={clampf(s.p.x+(int(esp_random()%13)-6),-20,20),clampf(s.p.y+(int(esp_random()%15)-7),3,92)};s.nextChange=n+1400+(esp_random()%1600);}if(s.state==S_RUN||s.state==S_FLEE){V2 v={s.target.x-s.p.x,s.target.y-s.p.y};float m=sqrtf(v.x*v.x+v.y*v.y);if(m<.4f){s.state=S_IDLE;s.nextChange=n+700+(esp_random()%1000);}else{s.p.x+=v.x/m*3.0f*dt;s.p.y+=v.y/m*3.0f*dt;}}}}

void spawnShell(V2 from,V2 to){for(auto&s:shells)if(!s.active){V2 d={to.x-from.x,to.y-from.y};float m=sqrtf(d.x*d.x+d.y*d.y);if(m<.1f)return;s.active=true;s.p=from;s.v={d.x/m*15,d.y/m*15};s.dieAt=millis()+4000;return;}}
void updateTanks(float dt,uint32_t n){for(auto&t:tanks)if(t.active){if(t.state==T_DEAD){if(n>=t.nextFire)t.active=false;continue;}V2 q={player.p.x-t.p.x,player.p.y-t.p.y};float d=sqrtf(q.x*q.x+q.y*q.y);t.heading=atan2f(q.x,q.y);if(d<38&&d>11&&(t.kind==T_MANEUVER||t.kind==T_LIGHT)){float sp=t.kind==T_LIGHT?3.2f:2.1f;t.p.x+=q.x/d*sp*dt;t.p.y+=q.y/d*sp*dt;}if(d<46&&n>=t.nextFire&&!rayBlockedBefore(t.p,{q.x/d,q.y/d},d)){spawnShell(t.p,player.p);t.nextFire=n+(t.kind==T_HEAVY?2400:1800)+(esp_random()%800);}}}
void updateShells(float dt,uint32_t n){for(auto&s:shells)if(s.active){s.p.x+=s.v.x*dt;s.p.y+=s.v.y*dt;if(n>=s.dieAt){s.active=false;continue;}if(dist2(s.p,player.p)<3.0f){s.active=false;effect(FX_BLAST,s.p,400);damagePlayer(25);}}}
void updateEffects(uint32_t n){for(auto&e:effects)if(e.active&&n-e.start>=e.duration)e.active=false;}

void missileStrike(uint32_t n){for(auto&m:missiles)if(!m.active){V2 f=forward(player.heading);m.active=true;m.target={clampf(player.p.x+f.x*5+(int(esp_random()%11)-5),-23,23),clampf(player.p.y+f.y*5,5,108)};m.warnAt=n;m.impactAt=n+2600;m.radius=4.5f;return;}}
void updateBoss(float dt,uint32_t n){if(zone!=PURSUIT)return;if(boss.phase==B_WALKING){boss.p.y+=2.3f*dt;if(boss.p.y>105)boss.p.y=105;if(n>=boss.nextMissile){missileStrike(n);sfx(SND_WARN);boss.nextMissile=n+4200;}for(auto&m:missiles)if(m.active&&n>=m.impactAt){m.active=false;effect(FX_BIGBLAST,m.target,700);sfx(SND_BOOM);if(dist2(m.target,player.p)<sqr(m.radius))damagePlayer(50);}}
 else if(boss.phase==B_COLLAPSING){boss.collapse=clampf((n-boss.phaseAt)/3000.0f,0,1);if(boss.collapse>=1){boss.phase=B_TANKS;boss.phaseAt=n;boss.defenders=6;spawnTank({-12,boss.p.y-8},T_LIGHT);spawnTank({12,boss.p.y-6},T_MANEUVER);spawnTank({-7,boss.p.y+3},T_HEAVY);spawnTank({8,boss.p.y+5},T_HOLDER);spawnTank({-15,boss.p.y+10},T_MANEUVER);spawnTank({14,boss.p.y+12},T_LIGHT);}}
 else if(boss.phase==B_TANKS){int alive=0;for(auto&t:tanks)if(t.active&&t.state!=T_DEAD)alive++;boss.defenders=alive;if(alive==0){boss.phase=B_CORE;boss.phaseAt=n;boss.coreOpen=true;}}
}

void simulate(float dt,uint32_t n){if(gameState==PLAYING){movePlayer(dt);updateSoldiers(dt,n);updateTanks(dt,n);updateShells(dt,n);updateBoss(dt,n);if(player.recoil>0)player.recoil=max(0.0f,player.recoil-dt*5);}else if(gameState==DEAD&&n-stateAt>=DEATH_MS)respawn();updateEffects(n);}

// ============================================================
// Projection and drawing
// ============================================================
Projected project(V2 p,float z=0){float dx=p.x-player.p.x,dy=p.y-player.p.y;float s=sinf(player.heading),c=cosf(player.heading);float cx=dx*c-dy*s;float depth=dx*s+dy*c;Projected r={0,0,depth,0,false};if(depth<NEAR_Z||depth>FAR_Z)return r;float sc=FOCAL/depth;r.x=120+(int)(cx*sc);r.y=HORIZON+(int)((CAMERA_H-z)*sc);r.scale=sc;r.visible=r.x>-80&&r.x<320&&r.y>-120&&r.y<320;return r;}
void ground(){const ZoneDef&z=zones[zone];screen.fillRect(0,0,SW,HORIZON,z.sky);screen.fillRect(0,HORIZON,SW,VIEW_H-HORIZON,z.ground); // perspective guide / road
 uint16_t road=zone>=TOWN?shade(C_ROAD,0):shade(z.ground,1);screen.fillTriangle(107,HORIZON,133,HORIZON,196,VIEW_H,road);screen.fillTriangle(107,HORIZON,196,VIEW_H,44,VIEW_H,road);
 // motion/optic-flow dashes
 for(int i=0;i<7;i++){float wy=player.p.y+5+i*i*2.2f;Projected a=project({-1.1f,wy}),b=project({1.1f,wy});if(a.visible&&b.visible)screen.drawFastHLine(a.x,a.y,b.x-a.x,shade(road,2));}
 if(zone==BEACH){screen.fillRect(0,HORIZON-7,SW,7,C_WATER);}
}

void drawObj(const StaticObj&o){Projected p=project({(float)o.x,(float)o.y});if(!p.visible)return;int w=max(2,(int)(o.size*p.scale*.65f)),h=w;uint16_t c=C_RUBBLE;switch(o.type){case O_BUILDING:c=C_BRICK;h=w*2;break;case O_BUNKER:c=C_CONCRETE;h=w/2+4;break;case O_WALL:c=C_CONCRETE;w*=2;h=max(4,w/3);break;case O_WRECK:c=C_METAL;w*=2;h=max(3,w/2);break;case O_CRATER:c=shade(C_DIRT,3);h=max(2,w/3);break;case O_HEDGEHOG:c=C_METAL_HI;break;case O_SANDBAG:c=C_BROWN;w*=2;h=max(3,w/3);break;case O_POLE:c=C_DARK;w=max(2,w/4);h*=3;break;case O_FIRE:c=C_FIRE;h*=2;break;case O_SMOKE:c=C_SMOKE;h*=3;break;case O_ARTILLERY:c=C_OLIVE;w*=2;break;case O_TREE:c=C_BROWN;h*=3;break;case O_LANDING:c=C_METAL_HI;w*=2;break;default:break;}w=min(w,120);h=min(h,170);int y=p.y-h;screen.fillRect(p.x-w/2,y,w,h,c);
 // Detail is expensive over SPI. Keep silhouettes at distance and spend extra
 // primitives only on nearby scenery where the player can actually read them.
 if(p.depth>42.0f)return;
 if(o.type==O_BUILDING){screen.fillRect(p.x-w/2,y,w,3,shade(c,2));if(p.depth<24.0f){for(int yy=y+8;yy<y+h-4;yy+=14)for(int xx=p.x-w/2+5;xx<p.x+w/2-3;xx+=14)screen.fillRect(xx,yy,4,5,C_DARK);}}else if(o.type==O_HEDGEHOG){screen.drawLine(p.x-w/2,p.y,p.x+w/2,y,C_METAL_HI);screen.drawLine(p.x-w/2,y,p.x+w/2,p.y,C_METAL_HI);}else if(o.type==O_SMOKE){screen.fillCircle(p.x,y+h/3,max(2,w/2),C_SMOKE);if(p.depth<22.0f)screen.fillCircle(p.x+3,y,max(2,w/3),shade(C_SMOKE,1));}else if(o.type==O_FIRE){screen.fillTriangle(p.x-w/2,p.y,p.x+w/2,p.y,p.x,y,C_FIRE);if(p.depth<25.0f)screen.fillTriangle(p.x-w/3,p.y,p.x+w/3,p.y,p.x,y+h/3,C_YELLOW);}}

void soldierSprite(const Soldier&s){Projected p=project(s.p,1.6f);if(!p.visible)return;int h=clampf(p.scale*1.7f,3,24),w=max(2,h/3);int y=p.y-h;uint16_t c=0x4A65;screen.fillRect(p.x-w/2,y+h/4,w,h/2,c);screen.fillCircle(p.x,y+h/7,max(1,w/2),0x9C8C);if(h>8){screen.drawLine(p.x-w/2,y+h/2,p.x-w,y+h*3/4,c);screen.drawLine(p.x+w/2,y+h/2,p.x+w,y+h*3/4,c);screen.drawLine(p.x-1,y+h*3/4,p.x-w/2,p.y,c);screen.drawLine(p.x+1,y+h*3/4,p.x+w/2,p.y,c);screen.drawLine(p.x,y+h/2,p.x+w+2,y+h/2-2,C_DARK);}}
void tankSprite(const EnemyTank&t){Projected p=project(t.p,1.4f);if(!p.visible)return;int w=clampf(p.scale*3.4f,5,64),h=max(4,w/2);int y=p.y-h;uint16_t c=t.kind==T_HEAVY?0x52A4:C_OLIVE;screen.fillRect(p.x-w/2,y+h/3,w,h*2/3,c);screen.fillRect(p.x-w/4,y,w/2,h/2,shade(c,1));screen.drawFastHLine(p.x,y+h/4,w/2,C_DARK);screen.fillRect(p.x-w/2-1,p.y-3,w+2,3,C_DARK);}
void shellSprite(const Shell&s){Projected p=project(s.p,.6f);if(p.visible){screen.fillCircle(p.x,p.y,max(1,(int)(p.scale*.15f)),C_YELLOW);}}
void effectSprite(const Effect&e,uint32_t n){Projected p=project(e.p,1.0f);if(!p.visible)return;float q=(n-e.start)/(float)e.duration;int r=max(2,(int)(p.scale*(e.type==FX_BIGBLAST?2.5f:1.2f)*(1-q*.4f)));uint16_t c=q<.45?C_YELLOW:(q<.75?C_FIRE:C_SMOKE);screen.fillCircle(p.x,p.y-r/2,r,c);}
void targetMarker(V2 wp,uint32_t n){Projected p=project(wp,2.5f);if(!p.visible)return;float a=(n%3000)*PI2/3000.0f;int r=10;int16_t x[3],y[3];for(int i=0;i<3;i++){float q=a+i*2.0944f;x[i]=p.x+cosf(q)*r;y[i]=p.y+sinf(q)*r;}for(int i=0;i<3;i++){int j=(i+1)%3;int ax=x[i]+(x[j]-x[i])*.18f,ay=y[i]+(y[j]-y[i])*.18f,bx=x[i]+(x[j]-x[i])*.82f,by=y[i]+(y[j]-y[i])*.82f;screen.drawLine(ax,ay,bx,by,C_RED);}}

void fortressDraw(uint32_t n){if(zone!=PURSUIT||boss.phase==B_INACTIVE||boss.phase==B_DESTROYED)return;Projected body=project(boss.p,13);if(!body.visible&&boss.p.y-player.p.y<0)return;float collapse=boss.phase==B_COLLAPSING?boss.collapse:(boss.phase>=B_TANKS?1:0);int bw=clampf(body.scale*15,30,210),bh=clampf(body.scale*6,12,90);int bx=body.x+(int)(collapse*55),by=body.y-bh;screen.fillRect(bx-bw/2,by,bw,bh,C_METAL);screen.fillRect(bx-bw/3,by-bh/3,bw*2/3,bh/3,C_METAL_HI);
 if(boss.phase==B_WALKING||boss.phase==B_COLLAPSING){for(int i=0;i<3;i++){float a=boss.heading+(i-1)*2.0944f;V2 lp={boss.p.x+sinf(a)*7,boss.p.y+cosf(a)*7};Projected p=project(lp,0);if(!p.visible)continue;int w=clampf(p.scale*3.8f,7,80);int top=max(0,(int)body.y);screen.fillRect(p.x-w/2,top,w,max(4,p.y-top),boss.leg[i].dead?shade(C_METAL,3):C_METAL);if(!boss.leg[i].dead)targetMarker(lp,n);}}
 if(boss.phase>=B_TANKS){ // fallen hull
   screen.fillRect(max(0,bx-bw/2),min(VIEW_H-35,by+bh/2),min(SW,bw),28,C_METAL);
 }
 if(boss.phase==B_CORE&&boss.coreOpen){screen.fillCircle(body.x,min(VIEW_H-30,(int)body.y),8,C_RED);targetMarker(boss.p,n);}}

void missileDraw(const Missile&m,uint32_t n){if(!m.active)return;Projected p=project(m.target,0);if(!p.visible)return;float remain=(m.impactAt-n)/2600.0f;int r=clampf(p.scale*m.radius,5,55);uint16_t c=((n/180)&1)?C_RED:C_YELLOW;screen.drawCircle(p.x,p.y,r,c);screen.drawCircle(p.x,p.y,max(2,r-3),c);}

// ============================================================
// Cockpit / HUD
// ============================================================
void drawCockpitStatic(){
  // Compact armored instrument shelf: only the bottom 46 px belongs to HUD.
  screen.fillRect(0,COCKPIT_Y,SW,SH-COCKPIT_Y,C_DARK);
  screen.drawFastHLine(0,COCKPIT_Y,SW,C_METAL_HI);
  screen.drawFastHLine(0,COCKPIT_Y+2,SW,C_METAL);
  // angled armor cheeks leave the center visually connected to the cannon
  screen.fillTriangle(0,SH,0,COCKPIT_Y+3,46,SH,C_METAL);
  screen.fillTriangle(SW-1,SH,SW-1,COCKPIT_Y+3,SW-47,SH,C_METAL);
  screen.setTextSize(1);
  screen.setTextColor(C_METAL_HI);
  screen.setCursor(7,281); screen.print("ARM");
  screen.setCursor(94,281); screen.print("HDG");
  screen.setCursor(165,281); screen.print("L");
  cockpitDrawn=true;
  lastHudHp=-1; lastHudLives=-1; lastCompass=-99; lastReady=!lastReady;
}

void cannonDraw(){
  // Cannon emerges from the tank nose into the world instead of consuming HUD space.
  int recoil=(int)(player.recoil*8);
  screen.fillTriangle(91,319,149,319,132,289,C_METAL);
  screen.fillTriangle(91,319,108,289,132,289,C_METAL);
  screen.fillRect(115,250+recoil,10,45-recoil,C_METAL_HI);
  screen.fillRect(112,247+recoil,16,7,C_METAL);
  screen.drawFastVLine(116,251+recoil,38-recoil,C_WHITE);
}

void hud(bool force=false){
  if(!cockpitDrawn) drawCockpitStatic();
  if(force||player.hp!=lastHudHp){
    screen.fillRect(6,292,72,18,C_DARK);
    int seg=max(0,(player.hp+12)/13);
    for(int i=0;i<8;i++){
      uint16_t col=i<seg?(seg<=2?C_RED:(seg<=4?C_YELLOW:C_GREEN)):0x3186;
      screen.fillRect(7+i*9,295,7,10,col);
    }
    lastHudHp=player.hp;
  }
  if(force||player.lives!=lastHudLives){
    screen.fillRect(162,292,31,18,C_DARK);
    screen.setTextColor(C_WHITE); screen.setTextSize(2); screen.setCursor(164,294);
    screen.print(player.lives);
    lastHudLives=player.lives;
  }
  bool ready=millis()>=player.cannonReady;
  if(force||ready!=lastReady){
    screen.fillRect(198,280,40,36,C_DARK);
    screen.fillCircle(218,296,9,ready?C_RED:C_DARK_RED);
    screen.drawCircle(218,296,10,C_METAL_HI);
    screen.setTextSize(1); screen.setTextColor(ready?C_WHITE:C_METAL_HI);
    screen.setCursor(202,309); screen.print(ready?"FIRE":"LOAD");
    lastReady=ready;
  }
  int compass=((int)lroundf(player.heading*4/PI2)%8+8)%8;
  if(force||compass!=lastCompass){
    static const char*names[]={"N","NE","E","SE","S","SW","W","NW"};
    screen.fillRect(88,292,64,18,C_DARK);
    screen.drawRect(88,292,64,18,C_METAL);
    screen.setTextSize(2); screen.setTextColor(C_WHITE);
    int x=120-(int)strlen(names[compass])*6;
    screen.setCursor(x,294); screen.print(names[compass]);
    lastCompass=compass;
  }
}

void crosshair(){screen.drawFastHLine(114,112,13,C_WHITE);screen.drawFastVLine(120,106,13,C_WHITE);screen.drawPixel(120,112,C_RED);}
void warningPanel(uint32_t n){bool danger=false;for(auto&m:missiles)if(m.active&&dist2(m.target,player.p)<sqr(m.radius+2))danger=true;if(danger&&((n/160)&1)){screen.fillRect(89,4,62,13,C_RED);screen.setTextColor(C_WHITE);screen.setTextSize(1);screen.setCursor(95,7);screen.print("MISSILE!");}}

// ============================================================
// Rendering
// ============================================================
void render(uint32_t n){ground();const ZoneDef&z=zones[zone]; // painter-ish: static authored props far to near
 for(int pass=0;pass<2;pass++)for(int i=z.objCount-1;i>=0;i--){StaticObj o;memcpy_P(&o,z.objs+i,sizeof(o));Projected p=project({(float)o.x,(float)o.y});if(!p.visible)continue;if((pass==0&&p.depth<28)||(pass==1&&p.depth>=28))continue;drawObj(o);}for(auto&s:soldiers)if(s.active&&s.state!=S_DEAD)soldierSprite(s);for(auto&t:tanks)if(t.active&&t.state!=T_DEAD){tankSprite(t);targetMarker(t.p,n);}for(auto&s:shells)if(s.active)shellSprite(s);fortressDraw(n);for(auto&m:missiles)missileDraw(m,n);for(auto&e:effects)if(e.active)effectSprite(e,n);crosshair();hud();cannonDraw();warningPanel(n);
 if(gameState==INTRO){screen.fillRect(26,52,188,34,C_DARK);screen.drawRect(26,52,188,34,C_WHITE);screen.setTextSize(2);screen.setTextColor(C_WHITE);screen.setCursor(34,62);screen.print("INVADE THE NORTH");}
 if(gameState==DEAD){screen.fillRect(62,90,116,28,C_DARK_RED);screen.setTextSize(2);screen.setTextColor(C_WHITE);screen.setCursor(76,97);screen.print("TANK LOST");}
 if(gameState==VICTORY){screen.fillRect(20,62,200,66,C_BLACK);screen.drawRect(20,62,200,66,C_YELLOW);screen.setTextSize(2);screen.setTextColor(C_YELLOW);screen.setCursor(31,76);screen.print("FORTRESS");screen.setCursor(39,96);screen.print("DESTROYED");screen.setTextSize(1);screen.setTextColor(C_WHITE);screen.setCursor(72,116);screen.print("MISSION COMPLETE");}
 if(gameState==GAMEOVER){screen.fillRect(52,80,136,40,C_BLACK);screen.drawRect(52,80,136,40,C_RED);screen.setTextSize(2);screen.setTextColor(C_RED);screen.setCursor(68,94);screen.print("GAME OVER");}
 frames++;}

// ============================================================
// Lifecycle
// ============================================================
void resetRun(){player={{0,4},0,MAX_HEALTH,START_LIVES,0,0,0};boss={};boss.phase=B_INACTIVE;mgShots=0;loadZone(BEACH,false);gameState=INTRO;stateAt=millis();}
void enter(){pinMode(LEFT_UP_PIN,INPUT_PULLUP);pinMode(LEFT_DOWN_PIN,INPUT_PULLUP);pinMode(RIGHT_UP_PIN,INPUT_PULLUP);pinMode(RIGHT_DOWN_PIN,INPUT_PULLUP);initSwitch(leftSw,LEFT_UP_PIN,LEFT_DOWN_PIN);initSwitch(rightSw,RIGHT_UP_PIN,RIGHT_DOWN_PIN);screen.setRotation(0);screen.setTextWrap(false);screen.fillScreen(C_BLACK);resetRun();drawCockpitStatic();lastUs=micros();accumUs=0;lastRender=0;lastPerf=millis();}
void tick(const GameInput&input){uint32_t n=millis(),us=micros();uint32_t elapsed=us-lastUs;lastUs=us;if(elapsed>200000)elapsed=200000;accumUs+=elapsed;updateSwitch(leftSw,LEFT_UP_PIN,LEFT_DOWN_PIN,n);updateSwitch(rightSw,RIGHT_UP_PIN,RIGHT_DOWN_PIN,n);leftState=leftSw.stable;rightState=rightSw.stable;
 if(gameState==INTRO&&(n-stateAt>1100||input.leftPressed||input.rightPressed)){gameState=PLAYING;stateAt=n;}
 bool exitChord=input.leftButton&&input.rightButton; if(gameState==PLAYING&&!exitChord){if(input.rightPressed||(input.rightButton&&n>=player.cannonReady))cannonFire();updateMG(input.leftButton,input.leftPressed,n);}else if(exitChord){mgShots=0;}
 if((gameState==VICTORY||gameState==GAMEOVER)&&n-stateAt>1500&&(input.leftPressed||input.rightPressed))resetRun();
 uint8_t steps=0;while(accumUs>=SIM_US&&steps<MAX_STEPS){simulate(SIM_US/1000000.0f,n);accumUs-=SIM_US;steps++;}if(steps==MAX_STEPS&&accumUs>=SIM_US)accumUs=0;updateSfx(n);if(n-lastRender>=RENDER_MS){lastRender=n;render(n);} 
#if TANK_DEBUG_PERF
 if(n-lastPerf>=1000){Serial.print("Tank FPS ");Serial.println(frames);frames=0;lastPerf=n;}
#endif
}

} // namespace Tank
