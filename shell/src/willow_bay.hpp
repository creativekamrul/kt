#pragma once
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <fstream>
#include <vector>
#include <array>
#include "sdp/mountain.hpp"

namespace trail {
class WillowBay {
 public:
  using Point = sdp::TrailPoint;
  static constexpr Point plaza{284,210}, bridge{181,168}, pass{233,44}, lodge{389,191};
  WillowBay() {
    if (!ground_.loadFromFile("shell/assets/bay-village.png") ||
        !walk_.loadFromFile("shell/assets/bay-walk.png") ||
        !front_.loadFromFile("shell/assets/bay-front.png") ||
        ground_.getSize()!=walk_.getSize())
      throw std::runtime_error("Cannot load the Willow Bay map and walking mask");
    std::ifstream props("shell/assets/bay-front.csv");
    Prop p;
    while(props >> p.x >> p.y >> p.base >> p.top >> p.w >> p.h) props_.push_back(p);
    if(props_.empty()) throw std::runtime_error("Cannot load Willow Bay landmarks");
  }
  static bool near(Point p,Point q,float distance) {
    return std::hypot(p.x-q.x,p.y-q.y)<=distance;
  }
  Point move(Point p,float dx,float dy,bool repaired) const {
    auto clear=[&](Point q){
      for (int x : {-3,3}) for (int y : {-1,1}) {
        const int xx=int(q.x)+x, yy=int(q.y)+y;
        if(xx<4 || yy<12 || xx>=508 || yy>=339 || walk_.getPixel(xx,yy).r<128) return false;
        if(!repaired && xx>=131 && xx<=158 && yy>=157 && yy<=182)return false;
      }
      return true;
    };
    if(clear({p.x+dx,p.y}))p.x+=dx;
    if(clear({p.x,p.y+dy}))p.y+=dy;
    return p;
  }
  void draw(sf::RenderTarget& target,int stages,float time) const {
    target.draw(sf::Sprite(ground_));
    for(int i=stages;i<3;++i){
      box(target,133+9*i,160,5,17,0x468c8a);
      box(target,133+9*i,159,5,2,0x795b49);
    }
    for(int i=0;i<9;++i){
      int x=22+(i*37)%100,y=258+i*7;
      int phase=(int(time*3)+i)%8;
      if(phase<4)box(target,x+phase,y,phase==2?5:3,1,0xa1dacc);
    }
    // A pair of swallows wheel above the garden.
    for(int i=0;i<2;++i){
      float t=time*.6f+i*.52f;
      int x=int(348+18*std::sin(t)),y=int(226+5*std::cos(t*2)+i*5);
      int lift=(int(time*6)+i)%2;
      box(target,x-2,y-lift,2,1,0x4e5e68);
      box(target,x,y,1,1,0x4e5e68);
      box(target,x+1,y-lift,2,1,0x4e5e68);
    }
  }
  void drawForeground(sf::RenderTarget& target,Point feet) const {
    const sf::IntRect actor(int(feet.x)-12,int(feet.y)-25,24,28);
    for(const auto& p : props_) {
      if(feet.y>=p.base) continue;
      sf::IntRect clip;
      if(!actor.intersects({p.x,p.y,p.w,p.h},clip)) continue;
      sf::Sprite sprite(front_,{clip.left-p.x,p.top+clip.top-p.y,clip.width,clip.height});
      sprite.setPosition(float(clip.left),float(clip.top));target.draw(sprite);
    }
  }
  // Sample the walking mask on a four-pixel grid. The same closed crossing
  // that stops the hiker also stops the guidance trail.
  void guide(sf::RenderTarget& t,Point player,Point goal,bool repaired,float clock) {
    constexpr int cols=128,rows=86;
    auto index=[](Point p){return std::clamp(int(p.y)/4,0,85)*128+std::clamp(int(p.x)/4,0,127);};
    auto center=[](int i){return Point{float((i%128)*4+2),float((i/128)*4+2)};};
    const int dest=index(goal);
    if(dest!=guideGoal_ || repaired!=guideRepaired_){
      guideGoal_=dest;guideRepaired_=repaired;next_.fill(-1);
      std::vector<int> open{dest};next_[dest]=dest;
      for(size_t k=0;k<open.size();++k){
        const int at=open[k];auto p=center(at);
        for(int delta:{1,-1,cols,-cols}){
          int n=at+delta;
          if(n<0 || n>=cols*rows || next_[n]>=0)continue;
          auto q=center(n);if(std::abs(p.x-q.x)>4)continue;
          auto actual=move(p,q.x-p.x,q.y-p.y,repaired);
          if(actual.x!=q.x || actual.y!=q.y)continue;
          next_[n]=at;open.push_back(n);
        }
      }
    }
    int node=index(player);
    // A foot can stand between grid samples at the water's edge.
    if(next_[node]<0){
      float best=12;
      for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx){
        int n=index({player.x+4*dx,player.y+4*dy});
        auto p=center(n);float d=std::hypot(p.x-player.x,p.y-player.y);
        if(next_[n]>=0 && d<best){best=d;node=n;}
      }
    }
    int phase=int(clock*3)%3;
    for(int step=0;node>=0 && node!=dest && step<21;++step){
      int forward=next_[node];if(forward<0)break;
      if(step==5 || step==11 || step==17){
        auto p=center(node),q=center(forward);
        int x=int(p.x),y=int(p.y),dx=int(q.x-p.x)/4,dy=int(q.y-p.y)/4;
        unsigned color=(step-5)/6==phase?0xffe9a1:0xf5c974;
        for(int layer=0;layer<2;++layer)for(int i=-3;i<=3;++i){
          int px=x+dx*(3-std::abs(i))-dy*i,py=y+dy*(3-std::abs(i))+dx*i;
          if(layer==0)box(t,px-1,py-1,4,4,0x855c46);
          else box(t,px,py,2,2,color);
        }
      }
      node=forward;
    }
  }
  static void beacon(sf::RenderTarget& t,Point goal,float time,int above){
    int x=int(goal.x),y=int(goal.y)-above-int(1.8f*std::sin(time*4));
    for(int i=0;i<5;++i){
      box(t,x-5+i,y+i,11-2*i,1,0x885a48);
      if(i<4)box(t,x-4+i,y+i,9-2*i,1,i==0?0xffefab:0xf4b958);
    }
  }
 private:
  static void box(sf::RenderTarget& target,int x,int y,int w,int h,unsigned rgb){
    sf::RectangleShape shape({float(w),float(h)});shape.setPosition(float(x),float(y));
    shape.setFillColor(sf::Color((rgb<<8)|255u));target.draw(shape);
  }
  struct Prop { int x,y,base,top,w,h; };
  std::vector<Prop> props_;
  int guideGoal_=-1;
  bool guideRepaired_=false;
  std::array<int,128*86> next_{};
  sf::Texture ground_,front_;
  sf::Image walk_;
};
}
