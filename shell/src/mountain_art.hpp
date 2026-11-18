#pragma once

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <string>
#include "sdp/mountain.hpp"

namespace trail {
class MountainArt {
 public:
  MountainArt() {
    load(fir_,"fir"); load(willow_,"willow"); load(spruce_,"spruce"); load(lodge_,"lodge");
    load(hero_,"climber"); load(vista_,"willow-bay"); load(summit_,"summit");
    if (!map_.create(480,500)) throw std::runtime_error("Cannot create mountain map");
  }
  void vista(sf::RenderTarget& target, sf::Vector2f size,bool summit=false) const {
    sf::Sprite s(summit ? summit_ : vista_);
    const float scale=std::max(size.x/480,size.y/270);
    s.setScale(scale,scale); s.setPosition((size.x-480*scale)/2,(size.y-270*scale)/2);
    target.draw(s);
  }
  void generate(const sdp::MountainTrail& t,int zone) {
    sdp::Rng rng(t.seed);
    const bool snow=zone>=2;
    const sf::Color grasses[]={sf::Color(124,182,106),sf::Color(213,165,101),
                               sf::Color(179,211,198),sf::Color(222,235,235)};
    const sf::Color shades[]={sf::Color(111,172,100),sf::Color(199,153,100),
                              sf::Color(158,196,189),sf::Color(203,221,229)};
    const sf::Color grass=grasses[std::clamp(zone,0,3)],shade=shades[std::clamp(zone,0,3)];
    sf::Image ground; ground.create(480,500,grass);
    for (int y=0;y<500;++y) for (int x=0;x<480;++x) {
      float noise=std::sin(x*.047+std::sin(y*.039)*2)+std::cos(y*.06+x*.03);
      if (noise>1) ground.setPixel(x,y,shade);
      const int bank=23+int(9*std::sin(y*.035+t.seed%17));
      if (x<bank) ground.setPixel(x,y, x>bank-3 ? sf::Color(206,181,130) : sf::Color(56,157,164));
      else if (noise<-.4 && (x+y*3)%47==0) ground.setPixel(x,y,snow ? sf::Color::White : sf::Color(200,190,115));
    }
    sf::Texture base;base.loadFromImage(ground);map_.draw(sf::Sprite(base));
    for(int i=0;i<1650;++i){
      int x=rng.nextInt(0,479),y=rng.nextInt(0,499);
      if (x<16) box(map_,x,y,rng.nextInt(2,8),1,0x97c0b0);
      else if(x>36){
        box(map_,x,y,1,2,snow ? 0x9bb3b1 : 0x768b57);
        if(i%6==0) box(map_,x-1,y-1,3,2,snow ? 0xf2eee0 : (i%2 ? 0xe6bd7a : 0xdcd29a));
      }
    }
    const sf::Color dirt=snow ? sf::Color(180,185,181) : sf::Color(232,195,139);
    auto path=[&](sdp::TrailPoint a,sdp::TrailPoint b,int radius) {
      const int n=int(std::hypot(b.x-a.x,b.y-a.y)/2)+1;
      sf::CircleShape dot(float(radius),14); dot.setOrigin(float(radius),float(radius));dot.setFillColor(dirt);
      for(int i=0;i<=n;++i){float f=float(i)/n;
        dot.setPosition(std::round(a.x+(b.x-a.x)*f+5*std::sin(f*6.283f)),std::round(a.y+(b.y-a.y)*f));
        map_.draw(dot);}
    };
    const sdp::TrailPoint fork{float(rng.nextInt(205,275)),326};
    path({t.start.x,499},t.start,15);path(t.start,fork,15);
    for(auto m:t.markers){path(fork,m,10);path(m,{t.rope.x,174},8);}
    for(int bx:t.crossings) {
      path(t.camp,{float(bx),float(t.creekY(bx))},8);
      path({float(bx),float(t.creekY(bx))},fork,8);
    }
    for(int x=0;x<480;++x){
      int y=t.creekY(float(x));
      box(map_,x,y-10,1,21,0x9b946e); box(map_,x,y-8,1,16,0x399ca6);
      box(map_,x,y-6,1,6,0x62b9b5);
      if((x/6)%4==0)box(map_,x,y+3,1,1,0xb7e2ca);
    }
    for(int bx:t.crossings){
      int by=t.creekY(float(bx));
      for(int j=-2;j<=2;++j){
        box(map_,bx-13,by+j*5-2,27,5,0x695849);
        box(map_,bx-13,by+j*5-2,27,3,0xc99e66);
        box(map_,bx-11,by+j*5-2,8,1,0xe0ba83);
      }
      for(int side:{-13,13})box(map_,bx+side,by-14,2,27,0x917350);
    }
    // Layered rock terraces and a climbable rope ladder.
    for(int x=0;x<480;++x){
      const int lip=137+int(4*std::sin(x*.062));
      box(map_,x,lip,1,167-lip,0x736775);
      box(map_,x,lip+3,1,13+(x/6)%6,snow ? 0xa2afb1 : 0xb89a84);
      box(map_,x,lip-3,1,6,snow ? 0xe5ece1 : 0xabc477);
    }
    for(int y=102;y<172;y+=6) box(map_,int(t.rope.x)-4,y,10,2,0xe5bd77);
    for(int x:{-6,5})box(map_,int(t.rope.x)+x,99,2,75,0x87684c);
    box(map_,int(t.rope.x)+9,83,2,20,0x665845);
    box(map_,int(t.rope.x)+11,83,10,6,0xcf754d);
    // Group trees in sheltered pockets and expose more rock as altitude rises.
    int groves=zone==0 ? 12 : zone==1 ? 7 : zone==2 ? 5 : 1;
    for(int i=0;i<groves;++i){
      int edge=i%2 ? 458 : 21, baseY=rng.nextInt(74,486);
      for(int j=0;j<rng.nextInt(2,4);++j){
        int x=edge+rng.nextInt(-12,12),y=baseY+9*j;
        sprite(map_,snow ? spruce_ : i%3==0 ? willow_ : fir_,x,y,.66f+rng.nextInt(0,4)*.1f);
      }
    }
    for(int i=0;i<8+zone*5;++i){
      int x=i%2 ? rng.nextInt(425,470) : rng.nextInt(9,49);
      int y=rng.nextInt(56,484),w=rng.nextInt(6,15);
      sf::ConvexShape stone(6);
      const sf::Vector2f points[]={{0.f,-2.f},{3.f,float(-w/2)},{float(w-3),float(-w/2-2)},
                                   {float(w+1),-3.f},{float(w),1.f},{3.f,2.f}};
      for(int k=0;k<6;++k)stone.setPoint(k,points[k]);
      stone.setPosition(float(x),float(y));
      stone.setFillColor(snow ? sf::Color(139,154,174) : sf::Color(156,127,132));map_.draw(stone);
      box(map_,x+3,y-w/2,w-5,2,snow ? 0xe9f1ed : 0xd5b9a1);
    }
    sprite(map_,lodge_,t.start.x-61,444,.83f);
    for(int i=0;i<42;++i){
      int x=rng.nextInt(45,434),y=rng.nextInt(176,476);
      if(std::abs(y-t.creekY(x))<17 || t.nearest({float(x),float(y)},28)>=0)continue;
      for(int n=0;n<4;++n){
        int xx=x+rng.nextInt(-4,4), yy=y+rng.nextInt(-3,3);
        box(map_,xx,yy,1,4,0x678658);box(map_,xx-1,yy-2,3,2,snow?0xecebd5:0xdca992);
      }
    }
    map_.display();
  }
  void environment(sf::RenderTarget& target) const { target.draw(sf::Sprite(map_.getTexture())); }
  void hiker(sf::RenderTarget& target,sdp::TrailPoint p,int direction,int frame) const {
    sf::Sprite s(hero_,sf::IntRect(frame*24,direction*28,24,28));s.setOrigin(12,25);
    s.setPosition(std::round(p.x),std::round(p.y)); target.draw(s);
  }
  static void box(sf::RenderTarget& t,float x,float y,float w,float h,unsigned color){
    sf::RectangleShape r({w,h});r.setPosition(x,y);r.setFillColor(sf::Color((color<<8)|255u));t.draw(r);
  }
 private:
  static void load(sf::Texture& tex,const std::string& name){
    if(!tex.loadFromFile("shell/assets/"+name+".png"))throw std::runtime_error("Missing KT art: "+name);
    tex.setSmooth(false);
  }
  static void sprite(sf::RenderTarget& t,const sf::Texture& tex,float x,float y,float scale){
    sf::Sprite s(tex);s.setOrigin(tex.getSize().x/2.f,float(tex.getSize().y));s.setPosition(x,y);s.setScale(scale,scale);t.draw(s);
  }
  sf::Texture fir_,willow_,spruce_,lodge_,hero_,vista_,summit_;
  sf::RenderTexture map_;
};
}
