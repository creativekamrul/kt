#pragma once
#include "mountain_art.hpp"
#include "willow_bay.hpp"

namespace trail {
class Glasswind {
 public:
  using Point=sdp::TrailPoint;
  enum class Phase { Trail, Tell, Feathers, Shadows, Perch, Open };
  Phase phase=Phase::Trail;
  Point feet{},facing{0,-1},bird{};
  int hearts=3,guard=3;
  bool victory=false;
  Glasswind(){
    if(!ground_.loadFromFile("shell/assets/glasswind.png") ||
       !mask_.loadFromFile("shell/assets/glasswind-walk.png") ||
       !owl_.loadFromFile("shell/assets/moonkite.png"))
      throw std::runtime_error("Cannot load Glasswind Hollow");
  }
  void enter(bool open,bool downhill){
    feet=downhill?Point{192,45}:Point{192,391};
    facing={0,downhill?1.f:-1.f};bird={192,84};phase=open?Phase::Open:Phase::Trail;
    gift_=Gift::None;hello_=0;victory=false;hearts=guard=3;reset();
  }
  bool fighting()const{return phase!=Phase::Trail && phase!=Phase::Open;}
  bool cleared()const{return phase==Phase::Open;}
  bool atExit()const{return cleared() && feet.y<47;}
  bool atEntry()const{return feet.y>380;}
  std::string hint()const{
    if(noteTime_>0)return note_;
    if(phase==Phase::Open)return "Follow the high lantern to the upper mountain.";
    if(phase==Phase::Tell)return "Wings wide! Watch where the bright feather flies.";
    if(phase==Phase::Feathers)return "Parry the central gold feather or slip between the three.";
    if(phase==Phase::Shadows)return "Leave each violet shadow before it falls.";
    if(phase==Phase::Perch)return "The moonkite has landed. Move close and strike!";
    if(near({98,237},21) && gift_==Gift::None)return "Inspect the mirror pool for a ward.";
    if(near({266,253},21) && gift_==Gift::None)return "Ring the hanging bell for a longer opening.";
    if(gift_!=Gift::None)return "Follow the lanterns to the moonkite's roost.";
    return "West: Mirror Pools for shelter. East: Bell Gallery for a longer opening.";
  }
  void dodge(){if(dodgeCd_<=0 && parry_<=0){dash_=facing;dodge_=.23f;dodgeCd_=.62f;}}
  void parry(){if(fighting() && dodge_<=0 && parryCd_<=0){parry_=.31f;parryCd_=.58f;}}
  bool canGreet()const{return cleared() && near(bird,30);}
  void use(){
    if(canGreet()){hello_=1.1f;say("The Moonkite hops closer and chirps hello.",2.5f);return;}
    if(phase==Phase::Trail && gift_==Gift::None){
      if(near({98,237},21)){gift_=Gift::Ward;say("Mirror ward earned. It will catch one hit.");}
      else if(near({266,253},21)){gift_=Gift::Bell;say("The bell answers. The moonkite will perch longer.");}
    }else if(phase==Phase::Perch && near(bird,35) && dodge_<=0){
      facing=direction(feet,bird);swing_=.25f;hit();
    }
  }
  void update(float dt,Point input){
    anim_+=dt;timer_+=dt;hello_=std::max(0.f,hello_-dt);
    for(float* p:{&dodgeCd_,&parryCd_,&safe_,&noteTime_,&swing_})*p=std::max(0.f,*p-dt);
    if(dodge_>0)move(dash_,dt*200);
    else if(std::hypot(input.x,input.y)>.05f){facing=direction({},input);move(input,dt*78);}
    if(phase==Phase::Trail && feet.y<=139){restart();say("The moonkite lifts off.",.85f);}
    else if(phase==Phase::Tell){
      if(timer_<.65f)aim_=direction(bird,feet);
      if(timer_>=1.1f){
        for(int i=-1;i<=1;++i){
          float a=std::atan2(aim_.y,aim_.x)+i*.37f;
          shots_[i+1]={bird,{std::cos(a),std::sin(a)},true};
        }
        change(Phase::Feathers);
      }
    }else if(phase==Phase::Feathers){
      for(int i=0;i<3;++i){
        auto& s=shots_[i];if(!s.live)continue;
        s.pos.x+=s.dir.x*dt*87;s.pos.y+=s.dir.y*dt*87;
        if(near(s.pos,9)){
          s.live=false;
          if(i==1 && parry_>0){hit();break;}
          if(dodge_<=0){damage();if(phase!=Phase::Feathers)break;}
        }
      }
      if(phase==Phase::Feathers && timer_>1.65f)shadows();
    }else if(phase==Phase::Shadows){
      if(timer_<.48f)shadow_=feet;
      if(timer_>=1.f && !fell_){fell_=true;if(near(shadow_,17) && dodge_<=0)damage();}
      if(phase==Phase::Shadows && timer_>=1.3f){
        if(++drops_==3){bird=shadow_;change(Phase::Perch);}
        else{shadow_=feet;timer_=0;fell_=false;}
      }
    }else if(phase==Phase::Perch && timer_>(gift_==Gift::Bell?4.f:2.45f))startVolley();
    dodge_=std::max(0.f,dodge_-dt);parry_=std::max(0.f,parry_-dt);
  }
  void draw(sf::RenderTarget& t,const MountainArt& art,int dir,int frame)const{
    t.draw(sf::Sprite(ground_));
    if(!fighting()){
      for(Point p:{Point{137,335},Point{239,334},Point{98,237},Point{266,253},Point{192,36}})
        if(p.y>50 || cleared())WillowBay::beacon(t,p,anim_,11);
    }
    if(phase==Phase::Tell){
      for(int i=9;i<57;i+=6)dot(t,bird.x+aim_.x*i,bird.y+aim_.y*i,2,2,0xffdf92);
    }
    if(phase==Phase::Shadows){
      int rad=fell_?18:17-int(std::min(timer_,1.f)*7);
      for(int k=0;k<28;++k){float a=k*.2244f;
        dot(t,shadow_.x+rad*std::cos(a),shadow_.y+rad*std::sin(a),3,2,fell_?0xffdaa5:0x735577);}
      if(timer_>.55f)dot(t,shadow_.x-2,shadow_.y-std::max(0.f,1-timer_)*82-5,4,8,0xe6b9ed);
    }
    if(phase==Phase::Feathers)for(int i=0;i<3;++i){
      const auto& s=shots_[i];if(!s.live)continue;
      for(int n=0;n<7;++n)dot(t,s.pos.x-s.dir.x*n-1,s.pos.y-s.dir.y*n-1,4,n<3?5:3,0x695874);
      for(int n=0;n<6;++n)dot(t,s.pos.x-s.dir.x*n,s.pos.y-s.dir.y*n,2,n<3?3:1,i==1?0xffdc72:0xe7a6ea);
    }
    auto drawBird=[&]{
      int f=phase==Phase::Perch || cleared()?3:int(anim_*6)%4;
      sf::Sprite sprite(owl_,{f*48,0,48,38});sprite.setOrigin(24,31);
      sprite.setPosition(std::round(bird.x),std::round(bird.y-5*std::abs(std::sin(hello_*6))+ (fighting() && phase!=Phase::Perch?std::sin(anim_*4)*2:0)));
      t.draw(sprite);
    };
    if(feet.y>=bird.y)drawBird();
    if(safe_<=0 || int(anim_*12)%2==0)art.hiker(t,feet,dir,frame);
    if(feet.y<bird.y)drawBird();
    if(dodge_>0)for(int i=1;i<4;++i)dot(t,feet.x-dash_.x*i*3,feet.y-dash_.y*i*3,3,2,0xc9f0de);
    if(parry_>0 || swing_>0)for(int i=0;i<17;++i){
      float a=i*.3927f;dot(t,feet.x+11*std::cos(a),feet.y-8+9*std::sin(a),2,2,parry_>0?0x80e6d5:0xffdc86);
    }
  }
  bool clear(Point p)const{
    if(p.x<4 || p.x>=380 || p.y<18 || p.y>=414)return false;
    if(fighting()){float x=(p.x-192)/86,y=(p.y-95)/52;if(x*x+y*y>1)return false;}
    if(!cleared() && p.y<43)return false;
    for(int x:{-2,2})if(mask_.getPixel(unsigned(p.x+x),unsigned(p.y)).r<128)return false;
    return true;
  }
 private:
  enum class Gift {None,Ward,Bell};
  struct Shot{Point pos,dir;bool live=false;};
  std::array<Shot,3> shots_{};
  sf::Texture ground_,owl_;sf::Image mask_;
  Point aim_{},shadow_{},dash_{};
  Gift gift_=Gift::None;
  int drops_=0;
  bool fell_=false,ward_=false;
  float hello_=0,anim_=0,timer_=0,dodge_=0,dodgeCd_=0,parry_=0,parryCd_=0,safe_=0,noteTime_=0,swing_=0;
  std::string note_;
  bool near(Point p,float r)const{return std::hypot(p.x-feet.x,p.y-feet.y)<=r;}
  static Point direction(Point a,Point b){float dx=b.x-a.x,dy=b.y-a.y,l=std::hypot(dx,dy);return l>.001f?Point{dx/l,dy/l}:Point{0,1};}
  void say(std::string s,float t=1.65f){note_=std::move(s);noteTime_=t;}
  void change(Phase p){phase=p;timer_=0;}
  void reset(){dodge_=dodgeCd_=parry_=parryCd_=safe_=noteTime_=timer_=swing_=0;shots_={};}
  void startVolley(){bird={guard%2?166.f:218.f,79};aim_=direction(bird,feet);change(Phase::Tell);}
  void shadows(){drops_=0;fell_=false;shadow_=feet;change(Phase::Shadows);}
  void hit(){
    --guard;
    if(guard<=0){change(Phase::Open);victory=true;bird={251,84};say("The moonkite settles. The lantern pass is yours.",3);}
    else if(phase==Phase::Feathers){say("Feather reflected! Watch the shadows.");shadows();}
    else{say("Clean strike!");startVolley();}
  }
  void restart(){hearts=guard=3;ward_=gift_==Gift::Ward;reset();startVolley();}
  void damage(){
    if(safe_>0)return;
    safe_=.7f;
    if(ward_){ward_=false;say("The mirror ward caught it!");return;}
    if(--hearts<=0){feet={192,134};restart();say("Breathe. Watch the feather and try again.");}
  }
  void move(Point d,float n){
    Point x{feet.x+d.x*n,feet.y};if(clear(x))feet=x;
    Point y{feet.x,feet.y+d.y*n};if(clear(y))feet=y;
  }
  static void dot(sf::RenderTarget& t,float x,float y,int w,int h,unsigned c){MountainArt::box(t,std::round(x),std::round(y),w,h,c);}
};
}

