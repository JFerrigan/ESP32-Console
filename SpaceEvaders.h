#pragma once
#include <Arduino.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "GameAPI.h"
#include "GameRenderMemory.h"
#include "Hardware.h"
#if defined(__has_include)
#if __has_include("SharedGameAudio.h")
#include "SharedGameAudio.h"
#endif
#endif

// Optional shared audio is enabled by its launcher integration capability macro.
namespace SpaceEvaders {
// Avoid ESP32/Xtensa register macros such as EPS.
constexpr float kCollisionTimeEpsilon = 0.000001f;
constexpr float kSimulationStepSeconds = 1.0f / 120.0f;
constexpr uint16_t rgb(unsigned r,unsigned g,unsigned b) { return ((r&248)<<8)|((g&252)<<3)|(b>>3); }
constexpr uint16_t BG=rgb(4,7,13), RIM=rgb(23,35,50), WHITE=rgb(244,255,255), GOLD=rgb(255,213,107), RED=rgb(255,82,105), MUTED=rgb(121,136,153);
constexpr uint16_t PRIMARY[2]={rgb(53,217,244),rgb(255,149,82)};
constexpr uint16_t SHADOW[2]={rgb(20,92,147),rgb(207,68,127)};
constexpr uint16_t COVER[2]={rgb(36,120,135),rgb(133,73,99)};
enum Phase { FightSplash, Playing, DeathBurst, Fatality, MatchOver };
enum Sound { Fire, Impact, Intercept, Fight, Death, Victory, Ready };
struct Rect { int x,y,w,h; };
struct Box { float x,y,hx,hy; };
struct Gate { bool armed, releasing; uint32_t since; };
struct Ship { float y,vy; int8_t move; uint8_t score,pending; bool ready; uint32_t muzzle; bool flashing; };
struct Shot { float x,y; bool active; };
struct Spark { int x,y; uint32_t born; bool active; };
struct Contact { float t; uint8_t kind,a,b,row,col; };
struct Snapshot { Rect rect; uint32_t appearance; bool visible; };
static Ship ships[2];
static Shot shots[4];
static Gate gates[2];
static uint8_t bunkers[6][12];
static Spark sparks[8];
static Phase phase;
static uint8_t hitMask;
static uint32_t attempt,phaseTime,lastUs,accum,lastRender,nowMs;
static bool presentationReady,rebuilding,unlockAfterDraw;
static int rebuildY;
static_assert(2048u * sizeof(uint16_t) <= GameRenderMemory::CAPACITY, "pixel buffer exceeds shared memory");
static uint16_t *const pixels = reinterpret_cast<uint16_t *>(GameRenderMemory::bytes);
static Rect canvas,dirty[64];
static uint8_t dirtyCount;
static Snapshot previous[14];
static uint32_t solverFailures,droppedTicks;
static int minI(int a,int b){return a<b?a:b;}
static int maxI(int a,int b){return a>b?a:b;}
static float shipX(int p){return p?216.0f:23.0f;}
static float boltV(int s){return s/2?-250.0f:250.0f;}
static void sound(int p,Sound s){
#ifdef JAKEBOY_SHARED_AUDIO_V1
  SharedGameAudio::playEffect(p,(uint8_t)s,nowMs);
#else
  (void)p; (void)s;
#endif
}
static Rect unite(Rect a,Rect b){int x=minI(a.x,b.x),y=minI(a.y,b.y);return {x,y,maxI(a.x+a.w,b.x+b.w)-x,maxI(a.y+a.h,b.y+b.h)-y};}
static bool overlaps(Rect a,Rect b){return a.x<b.x+b.w&&b.x<a.x+a.w&&a.y<b.y+b.h&&b.y<a.y+a.h;}
static void markDirty(Rect r){
  int x=maxI(0,r.x),y=maxI(0,r.y);r={x,y,minI(240,r.x+r.w)-x,minI(320,r.y+r.h)-y};
  if(r.w<=0||r.h<=0)return;
  for(int i=0;i<dirtyCount;){Rect u=unite(r,dirty[i]);if(overlaps(r,dirty[i])&&u.w*u.h<= (r.w*r.h+dirty[i].w*dirty[i].h)*5/4){r=u;dirty[i]=dirty[--dirtyCount];i=0;}else ++i;}
  if(dirtyCount==64){int best=0,cost=100000;for(int i=0;i<64;++i){Rect u=unite(r,dirty[i]);int c=u.w*u.h-dirty[i].w*dirty[i].h;if(c<cost){cost=c;best=i;}}dirty[best]=unite(r,dirty[best]);}else dirty[dirtyCount++]=r;
}
static void resetGates(){memset(gates,0,sizeof gates);ships[0].pending=ships[1].pending=0;}
static bool pollGate(Gate &g,bool held,uint32_t ms){
  if(held){g.releasing=false;if(g.armed){g.armed=false;return true;}}
  else {if(!g.releasing){g.releasing=true;g.since=ms;}if(uint32_t(ms-g.since)>=15)g.armed=true;}return false;
}
static int readMove(int up,int down){bool u=digitalRead(up)==LOW,d=digitalRead(down)==LOW;return u==d?0:u?-1:1;}
static void rebuild(){rebuilding=true;rebuildY=0;presentationReady=false;dirtyCount=0;memset(previous,0,sizeof previous);}
static void changePhase(Phase next){phase=next;phaseTime=nowMs;resetGates();accum=0;lastUs=micros();
  if(next==Playing){presentationReady=true;return;}
  if(next==DeathBurst){presentationReady=true;for(int p=0;p<2;++p)if(hitMask&(1<<p))sound(p,Death);return;}
  if(next==Fatality)for(auto &s:shots)s.active=false;
  if(next==MatchOver){ships[0].ready=ships[1].ready=false;sound(ships[1].score==3?1:0,Victory);}
  rebuild();
}
static void prepareRound(){
  memset(shots,0,sizeof shots);memset(sparks,0,sizeof sparks);hitMask=0;
  for(auto &s:ships){s.y=159.5f;s.vy=0;s.flashing=false;s.pending=0;s.ready=false;}
  for(auto &b:bunkers)for(int r=0;r<12;++r)b[r]=(r==0||r==11)?6:15;
  unlockAfterDraw=false;changePhase(FightSplash);
}
static void resetMatch(){memset(ships,0,sizeof ships);attempt=1;prepareRound();}
static void spark(int x,int y){for(auto &s:sparks)if(!s.active){s={x,y,nowMs,true};return;}}
static Box cellBox(int b,int r,int c){return {float((b<3?49:175)+c*4+2),float(40+(b%3)*96+r*4+2),2,2};}
static bool swept(Box a,float vx,float vy,Box b,float bx,float by,float limit,float &t){
  float entry=0,leave=limit;float d[2]={a.x-b.x,a.y-b.y},v[2]={vx-bx,vy-by},h[2]={a.hx+b.hx,a.hy+b.hy};
  for(int k=0;k<2;++k){if(fabsf(v[k])<kCollisionTimeEpsilon){if(fabsf(d[k])>h[k])return false;}else{float t0=(-h[k]-d[k])/v[k],t1=(h[k]-d[k])/v[k];if(t0>t1){float z=t0;t0=t1;t1=z;}entry=fmaxf(entry,t0);leave=fminf(leave,t1);if(entry>leave+kCollisionTimeEpsilon)return false;}}
  if(entry>limit+kCollisionTimeEpsilon||leave<0)return false;
  t=fmaxf(0,entry);return true;
}
static int gather(Contact *out,float remaining){
  int n=0;
  for(int s=0;s<4;++s)if(shots[s].active){Box a={shots[s].x,shots[s].y,2.5f,1};float t;Contact best={remaining+1,0,(uint8_t)s,0,0,0};float bestDY=1e9;
    for(int b=0;b<6;++b)for(int r=0;r<12;++r)for(int c=0;c<4;++c)if(bunkers[b][r]&(1<<c)){
      Box box=cellBox(b,r,c);if(swept(a,boltV(s),0,box,0,0,remaining,t)){float dy=fabsf(box.y-a.y);
        if(t<best.t-kCollisionTimeEpsilon||(fabsf(t-best.t)<=kCollisionTimeEpsilon&&dy<bestDY-kCollisionTimeEpsilon)){best={t,0,(uint8_t)s,(uint8_t)b,(uint8_t)r,(uint8_t)c};bestDY=dy;}}}
    if(best.t<=remaining+kCollisionTimeEpsilon)out[n++]=best;
    int p=1-s/2;Box target={shipX(p),ships[p].y,6,7};if(swept(a,boltV(s),0,target,0,ships[p].vy,remaining,t))out[n++]={t,2,(uint8_t)s,(uint8_t)p,0,0};
    t=((s/2?-2.5f:242.5f)-a.x)/boltV(s);if(t>=-kCollisionTimeEpsilon&&t<=remaining+kCollisionTimeEpsilon)out[n++]={fmaxf(0,t),3,(uint8_t)s,0,0,0};
  }
  for(int a=0;a<2;++a)for(int b=2;b<4;++b)if(shots[a].active&&shots[b].active){float t;if(swept({shots[a].x,shots[a].y,2.5f,1},250,0,{shots[b].x,shots[b].y,2.5f,1},-250,0,remaining,t))out[n++]={t,1,(uint8_t)a,(uint8_t)b,0,0};}
  for(int p=0;p<2;++p)if(ships[p].vy!=0){float t=((ships[p].vy<0?18:301)-ships[p].y)/ships[p].vy;if(t>=-kCollisionTimeEpsilon&&t<=remaining+kCollisionTimeEpsilon)out[n++]={fmaxf(0,t),4,(uint8_t)p,0,0,0};}
  return n; // <= 4*(cover+ship+exit) + 4 pairs + 2 boundaries = 18
}
static void advance(float t){for(int p=0;p<2;++p)ships[p].y+=ships[p].vy*t;for(int s=0;s<4;++s)if(shots[s].active)shots[s].x+=boltV(s)*t;}
static void tryFire(int p){for(int i=p*2;i<p*2+2;++i)if(!shots[i].active){shots[i]={p?203.0f:36.0f,ships[p].y,true};ships[p].muzzle=nowMs;ships[p].flashing=true;sound(p,Fire);return;}}
static void stepSimulation(){
  for(int p=0;p<2;++p){ships[p].vy=ships[p].move*170.0f;while(ships[p].pending){--ships[p].pending;tryFire(p);}}
  float remaining=kSimulationStepSeconds;uint8_t hits=0;int iterations=0;
  while(remaining>kCollisionTimeEpsilon&&iterations++<12){Contact events[24];int n=gather(events,remaining);float first=remaining+1;for(int i=0;i<n;++i)first=fminf(first,events[i].t);
    if(first>remaining+kCollisionTimeEpsilon){advance(remaining);remaining=0;break;}advance(first);remaining=fmaxf(0,remaining-first);
    bool consumed[4]={};
    for(int kind=0;kind<5;++kind){bool stage[4]={};
      for(int i=0;i<n;++i){Contact e=events[i];if(e.kind!=kind||fabsf(e.t-first)>kCollisionTimeEpsilon)continue;
        if(kind==4){ships[e.a].y=ships[e.a].vy<0?18:301;ships[e.a].vy=0;continue;}
        if(consumed[e.a]||(kind==1&&consumed[e.b]))continue;
        stage[e.a]=true;
        if(kind==0){if(bunkers[e.b][e.row]&(1<<e.col)){bunkers[e.b][e.row]&=~(1<<e.col);Box b=cellBox(e.b,e.row,e.col);markDirty({int(b.x)-6,int(b.y)-6,12,12});spark(int(b.x),int(b.y));sound(e.b/3,Impact);}}
        if(kind==1){stage[e.b]=true;spark(int((shots[e.a].x+shots[e.b].x)/2),int(shots[e.a].y));sound(0,Intercept);sound(1,Intercept);}
        if(kind==2)hits|=1<<e.b;
      }
      for(int s=0;s<4;++s)consumed[s]|=stage[s];
    }
    for(int s=0;s<4;++s)if(consumed[s])shots[s].active=false;
  }
  if(remaining>kCollisionTimeEpsilon)++solverFailures;
  if(hits){hitMask=hits;if(hits!=3){int winner=hits==1?1:0;++ships[winner].score;markDirty({0,138,12,44});markDirty({228,138,12,44});}changePhase(DeathBurst);}
}
// Small opaque region rasterizer. No per-pixel TFT calls.
static void fill(Rect r,uint16_t color){int x=maxI(r.x,canvas.x),y=maxI(r.y,canvas.y),right=minI(r.x+r.w,canvas.x+canvas.w),bottom=minI(r.y+r.h,canvas.y+canvas.h);for(int yy=y;yy<bottom;++yy)for(int xx=x;xx<right;++xx)pixels[(yy-canvas.y)*canvas.w+xx-canvas.x]=color;}
static void localRect(int p,int u,int v,int w,int h,uint16_t color){fill(p?Rect{v,320-u-w,h,w}:Rect{240-v-h,u,h,w},color);}
// Original compact 5x7 glyph rows. Only these glyphs are needed by the UI.
static const uint8_t font[39][7]={
{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},{7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},{30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},{17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14},{4,4,4,4,4,0,4},{0,0,0,31,0,0,0},{0,0,0,0,0,0,0}};
static void text(int p,const char *str,int v,int scale,uint16_t color){int len=strlen(str),u=(320-(len*6-1)*scale)/2;for(int i=0;i<len;++i){char ch=str[i];int k=ch>='A'&&ch<='Z'?ch-'A':ch>='0'&&ch<='9'?ch-'0'+26:ch=='!'?36:ch=='-'?37:38;for(int y=0;y<7;++y)for(int x=0;x<5;++x)if(font[k][y]&(16>>x))localRect(p,u+i*6*scale+x*scale,v+y*scale,scale,scale,color);}}
static void drawShip(int p){int x=int(shipX(p)),y=int(lroundf(ships[p].y));
  for(int dy=-10;dy<=10;++dy){int a=abs(dy);int lo=a>=8?-6:a>=4?-8:-5;int hi=a>=8?-2:a>=4?1: a>=2?5:9;
    for(int dx=lo;dx<=hi;++dx){uint16_t c=dx<lo+2?SHADOW[p]:PRIMARY[p];if(abs(dy)<=1&&dx>=-2&&dx<=2)c=WHITE;fill({x+(p?-dx:dx),y+dy,1,1},c);}}
  fill({x+(p?7:-8),y-2,2,5},ships[p].move?PRIMARY[p]:SHADOW[p]);
  if(ships[p].flashing)fill({x+(p?-13:10),y-2,4,5},WHITE);
}
static void compose(Rect r){canvas=r;for(int i=0;i<r.w*r.h;++i)pixels[i]=BG;
  for(int i=0;i<12;++i){int x=36+(i*37)%78,y=12+(i*71)%296;fill({x,y,1,1},RIM);fill({239-x,319-y,1,1},RIM);}
  for(int b=0;b<6;++b)for(int row=0;row<12;++row)for(int c=0;c<4;++c)if(bunkers[b][row]&(1<<c)){Box q=cellBox(b,row,c);Rect z={int(q.x)-2,int(q.y)-2,4,4};fill(z,COVER[b/3]);int next=c+(b<3?1:-1);if(next<0||next>3||!(bunkers[b][row]&(1<<next)))fill({z.x+(b<3?3:0),z.y,1,4},SHADOW[b/3]);}
  for(int p=0;p<2;++p){for(int i=0;i<3;++i){int x=p?230:3,y=144+i*12;fill({x,y,7,7},RIM);fill({x+1,y+1,5,5},i<ships[p].score?PRIMARY[p]:BG);}if(!(hitMask&(1<<p)))drawShip(p);}
  for(int i=0;i<4;++i)if(shots[i].active){int x=lroundf(shots[i].x),y=lroundf(shots[i].y);fill({x-3,y-1,7,3},PRIMARY[i/2]);fill({x-2+(i/2?0:1),y,4,1},WHITE);}
  for(auto &s:sparks)if(s.active){fill({s.x-3,s.y,7,1},GOLD);fill({s.x,s.y-3,1,7},WHITE);}
  if(phase==DeathBurst){int age=nowMs-phaseTime;for(int p=0;p<2;++p)if(hitMask&(1<<p)){int x=shipX(p),y=lroundf(ships[p].y);if(age<45)fill({x-7,y-7,15,15},WHITE);else {int d=5+(age-45)/10;for(int a=-1;a<=1;++a)for(int b=-1;b<=1;++b)if(a||b)fill({x+a*d-1,y+b*d-1,3,3},PRIMARY[p]);}}}
  if(phase==FightSplash||phase==Fatality||phase==MatchOver){for(int p=0;p<2;++p){localRect(p,20,136,280,77,BG);localRect(p,52,207,216,1,SHADOW[p]);localRect(p,40,153,2,8,PRIMARY[p]);localRect(p,278,153,2,8,PRIMARY[p]);
    if(phase==FightSplash){char label[32];snprintf(label,sizeof label,"ROUND %lu",(unsigned long)attempt);text(p,label,141,1,MUTED);text(p,"FIGHT!",166,3,SHADOW[p]);text(p,"FIGHT!",165,3,GOLD);}
    else if(phase==Fatality){text(p,"FATALITY",158,3,RED);text(p,hitMask==3?"DOUBLE KO":(hitMask&(1<<p))?"ROUND LOST":"ROUND WON",190,1,MUTED);}
    else {bool win=ships[p].score==3;text(p,win?"YOU WIN":"YOU LOSE",152,3,win?GOLD:MUTED);text(p,ships[p].ready?"READY":"PRESS FIRE",190,2,ships[p].ready?PRIMARY[p]:MUTED);if(win){localRect(p,154,138,12,2,GOLD);localRect(p,159,133,2,12,GOLD);}}
  }}
}
static void transfer(Rect r){for(int y=r.y;y<r.y+r.h;){int h=minI(r.y+r.h-y,2048/r.w);Rect part={r.x,y,r.w,h};compose(part);display.drawRGBBitmap(part.x,part.y,pixels,part.w,part.h);y+=h;}}
static void snapshot(int index,Rect r,bool visible,uint32_t appearance){Snapshot &s=previous[index];if(s.visible!=visible||s.appearance!=appearance||memcmp(&s.rect,&r,sizeof r)){if(s.visible)markDirty(s.rect);if(visible)markDirty(r);}s={r,appearance,visible};}
static void renderFrame(){
  for(int p=0;p<2;++p)snapshot(p,{int(shipX(p))-14,int(lroundf(ships[p].y))-11,29,23},!(hitMask&(1<<p)),ships[p].flashing*2+(ships[p].move!=0));
  for(int i=0;i<4;++i)snapshot(2+i,{int(lroundf(shots[i].x))-3,int(lroundf(shots[i].y))-1,7,3},shots[i].active,0);
  for(int i=0;i<8;++i)snapshot(6+i,{sparks[i].x-3,sparks[i].y-3,7,7},sparks[i].active,0);
  if(phase==DeathBurst)for(int p=0;p<2;++p)if(hitMask&(1<<p))markDirty({int(shipX(p))-24,int(ships[p].y)-24,49,49});
  for(int i=0;i<dirtyCount;++i)transfer(dirty[i]);
  dirtyCount=0;
  if(unlockAfterDraw){unlockAfterDraw=false;resetGates();accum=0;lastUs=micros();}
}
static void renderSlice(){for(int i=0;i<4&&rebuildY<320;++i){transfer({0,rebuildY,240,minI(8,320-rebuildY)});rebuildY+=8;}if(rebuildY>=320){rebuilding=false;presentationReady=true;phaseTime=nowMs;if(phase==FightSplash){sound(0,Fight);sound(1,Fight);}}}
static void pollInput(const GameInput &in){ships[0].move=readMove(LEFT_UP_PIN,LEFT_DOWN_PIN);ships[1].move=readMove(RIGHT_UP_PIN,RIGHT_DOWN_PIN);
  for(int p=0;p<2;++p)if(pollGate(gates[p],p?in.rightButton:in.leftButton,nowMs)){
    if(phase==Playing&&!unlockAfterDraw){int count=ships[p].pending;for(int i=p*2;i<p*2+2;++i)count+=shots[i].active;if(count<2)++ships[p].pending;}
    else if(phase==MatchOver&&presentationReady&&uint32_t(nowMs-phaseTime)>=450&&!ships[p].ready){ships[p].ready=true;sound(p,Ready);markDirty(p?Rect{184,30,24,260}:Rect{32,30,24,260});}
  }
}
inline void enter(){nowMs=millis();lastUs=micros();lastRender=lastUs;accum=0;solverFailures=droppedTicks=0;display.setRotation(0);display.setTextWrap(false);resetMatch();}
inline void update(const GameInput &in){nowMs=millis();uint32_t us=micros(),elapsed=us-lastUs;lastUs=us;pollInput(in);
  for(auto &s:sparks)if(s.active&&uint32_t(nowMs-s.born)>=70)s.active=false;
  for(auto &s:ships)if(s.flashing&&uint32_t(nowMs-s.muzzle)>=45)s.flashing=false;
  if(!rebuilding&&presentationReady){uint32_t age=nowMs-phaseTime;
    if(phase==FightSplash&&age>=650){phase=Playing;unlockAfterDraw=true;resetGates();markDirty({27,20,78,280});markDirty({136,20,78,280});}
    else if(phase==DeathBurst&&age>=180)changePhase(Fatality);
    else if(phase==Fatality&&age>=700){if(ships[0].score==3||ships[1].score==3)changePhase(MatchOver);else {++attempt;prepareRound();}}
    else if(phase==MatchOver&&ships[0].ready&&ships[1].ready)resetMatch();
  }
  if(phase==Playing&&!unlockAfterDraw){if(elapsed>33334){++droppedTicks;elapsed=33334;}accum+=elapsed*120;int ticks=0;while(accum>=1000000&&ticks++<4&&phase==Playing){accum-=1000000;stepSimulation();}if(accum>=1000000){++droppedTicks;accum%=1000000;}}
  else accum=0;
  if(rebuilding)renderSlice();else if(uint32_t(us-lastRender)>=16667){lastRender=us;renderFrame();}
}
} // namespace SpaceEvaders
