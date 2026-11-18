#pragma once
#include "mountain_art.hpp"
#include <algorithm>
#include <array>
#include <vector>

namespace trail {
class TrailLights {
 public:
  using Point=sdp::TrailPoint;
  void reset(){goal_=-1;}
  template<class Step>
  void draw(sf::RenderTarget& t,Point player,Point goal,float clock,bool bright,Step canStep){
    constexpr int cols=128,rows=128;
    auto index=[](Point p){return std::clamp(int(p.y)/4,0,127)*128+std::clamp(int(p.x)/4,0,127);};
    auto center=[](int i){return Point{float((i%128)*4+2),float((i/128)*4+2)};};
    const int dest=index(goal);
    if(dest!=goal_){
      goal_=dest;next_.fill(-1);
      std::vector<int> open{dest};next_[dest]=dest;
      for(size_t k=0;k<open.size();++k){
        const int at=open[k];auto p=center(at);
        for(int delta:{1,-1,cols,-cols}){
          int n=at+delta;
          if(n<0 || n>=cols*rows || next_[n]>=0)continue;
          auto q=center(n);if(std::abs(p.x-q.x)>4)continue;
          if(!canStep(q,p))continue;
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
    for(int step=0;node>=0 && node!=dest && step<(bright?60:21);++step){
      int forward=next_[node];if(forward<0)break;
      if(bright ? step%4==2 : (step==5 || step==11 || step==17)){
        auto p=center(node),q=center(forward);
        int x=int(p.x),y=int(p.y),dx=int(q.x-p.x)/4,dy=int(q.y-p.y)/4;
        unsigned color=bright?((step/4+phase)%3?0xffee91:0xfffbdb):(step-5)/6==phase?0xffe9a1:0xf5c974;
        for(int layer=0;layer<2;++layer)for(int i=-3;i<=3;++i){
          int px=x+dx*(3-std::abs(i))-dy*i,py=y+dy*(3-std::abs(i))+dx*i;
          if(layer==0)MountainArt::box(t,px-1,py-1,4,4,0x855c46);
          else MountainArt::box(t,px,py,2,2,color);
        }
      }
      node=forward;
    }
  }
 private:
  int goal_=-1;
  std::array<int,128*128> next_{};
};
}
