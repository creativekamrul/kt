#pragma once
#include "mountain_art.hpp"
#include "willow_bay.hpp"

namespace trail {
class BrackenRise {
 public:
  using Point=sdp::TrailPoint;
  enum class Phase { Quiet, Aim, Rush, Stamp, Wave, Rest, Stunned, Clear };
  enum class Path { Fork, Fern, Cairn };
  static constexpr Point entry{192,407}, exit{192,34}, clearing{192,111};
  Point feet=entry, ram{192,98}, facing{0,-1};
  Phase phase=Phase::Quiet;
  Path path=Path::Fork;
  int hearts=3,guard=3;
  float clock=0,anim=0;
  bool victory=false;
  BrackenRise(){
    if(!land_.loadFromFile("shell/assets/bracken-rise.png") ||
       !mask_.loadFromFile("shell/assets/bracken-walk.png") ||
       !ramArt_.loadFromFile("shell/assets/copperhorn.png"))
      throw std::runtime_error("Cannot load Bracken Rise");
  }
  static float distance(Point a,Point b){return std::hypot(a.x-b.x,a.y-b.y);}
  bool fighting()const{return phase!=Phase::Quiet && phase!=Phase::Clear;}
  bool cleared()const{return phase==Phase::Clear;}
  void enter(bool complete,bool downhill){
    phase=complete?Phase::Clear:Phase::Quiet;path=Path::Fork;
    feet=downhill?Point{192,48}:entry;ram=complete?Point{249,102}:Point{192,98};facing={0,downhill?1.f:-1.f};
    victory=false;resetActions();message_.clear();messageTime_=0;
  }
  void dodge(){
    if(dodgeDelay_>0 || parry_>0)return;
    dash_=facing;dodge_=.22f;dodgeDelay_=.65f;
  }
  void parry(){
    if(!fighting() || parryDelay_>0 || dodge_>0)return;
    parry_=.30f;parryDelay_=.58f;
  }
  void strike(){
    if(!fighting() || strikeDelay_>0 || dodge_>0)return;
    strike_=.22f;strikeDelay_=.47f;facing=unit(feet,ram);
    if(distance(feet,ram)>35){say("Move closer to strike.");return;}
    if(phase!=Phase::Stunned && phase!=Phase::Rest){say("Horns up! Wait for an opening.");return;}
    --guard;
    if(guard==0){phase=Phase::Clear;victory=true;clock=0;say("The copperhorn bows. The upper pass is open!",4);return;}
    say("Clean strike!");nextAttack();
  }
  std::string objective()const {
    if(messageTime_>0)return message_;
    switch(phase){
      case Phase::Aim:return "Watch the coral charge line.";
      case Phase::Rush:return "Dodge aside or parry the charge!";
      case Phase::Stamp:return "Hoof raised! Watch for the ripple.";
      case Phase::Wave:return "Dodge through the ripple or parry its edge.";
      case Phase::Stunned:return "Parried! Close in and strike.";
      case Phase::Rest:return "An opening! Move close and strike.";
      default:break;
    }
    if(cleared())return "Follow the gold pennants uphill to the upper trail.";
    if(feet.y<185)return "Enter the copperhorn's clearing.";
    if(path==Path::Fern)return "Fern Switchback  /  Follow the teal flags. Gain one heart.";
    if(path==Path::Cairn)return "Cairn Steps  /  Follow the orange flags. Parries stun longer.";
    return "Choose a climb: teal fern switchbacks or orange cairn steps.";
  }
  void update(float dt,Point move){
    anim+=dt;clock+=dt;
    for(float* p:{&parryDelay_,&strikeDelay_,&dodgeDelay_,&messageTime_,&invulnerable_,&strike_})
      *p=std::max(0.f,*p-dt);
    if(dodge_>0)slide(dash_,205*dt);
    else if(std::hypot(move.x,move.y)>.05f){facing=unit({},move);slide(move,78*dt);}
    if(cleared()){
      Point rest{249,102};
      if(distance(ram,rest)>1){auto d=unit(ram,rest);ram.x+=d.x*dt*28;ram.y+=d.y*dt*28;}
    }else if(!fighting()){
      if(feet.y>300 && feet.y<347){
        if(feet.x<166)path=Path::Fern;
        if(feet.x>218)path=Path::Cairn;
      }
      if(feet.y<=145){
        hearts=path==Path::Fern?4:3;guard=3;charges_=0;ram={192,95};
        resetActions();startAim();say("Copperhorn guards the pass.",1);
      }
    } else if(fighting())enemy(dt);
    dodge_=std::max(0.f,dodge_-dt);parry_=std::max(0.f,parry_-dt);
  }
  void draw(sf::RenderTarget& t,const MountainArt& art,int dir,int frame)const{
    t.draw(sf::Sprite(land_));
    if(!fighting()){
      for(Point p:{Point{132,340},Point{251,328},Point{160,158},Point{225,156},exit}){
        if(!cleared() && p.y<55)continue;
        WillowBay::beacon(t,p,anim,8);
      }
    }
    if(phase==Phase::Aim || phase==Phase::Rush){
      Point start=ram;
      for(int k=6;k<170;k+=6){
        Point p{start.x+aim_.x*k,start.y+aim_.y*k};if(!arena(p))break;
        dot(t,p.x-1,p.y-1,3,2,clock>.55f?0xffdd79:0xdc7763);
      }
    }
    if(phase==Phase::Wave || phase==Phase::Stamp){
      float radius=phase==Phase::Wave?waveRadius():9+2*std::sin(clock*13);
      for(int i=0;i<100;++i){
        float a=float(i)*.06283f;Point p{ram.x+radius*std::cos(a),ram.y+radius*std::sin(a)};
        if(arena(p))dot(t,p.x,p.y,3,2,i%3?0xf8d987:0xcf7962);
      }
    }
    auto hero=[&]{
      if(dodge_>0)for(int i=1;i<=3;++i)dot(t,feet.x-dash_.x*i*4,feet.y-dash_.y*i*4,4-i,2,0xffdb99);
      if(invulnerable_<=0 || int(anim*12)%2==0)art.hiker(t,feet,dir,frame);
      if(parry_>0 || strike_>0){
        float angle=std::atan2(facing.y,facing.x);
        for(int i=-7;i<=7;++i){
          float a=angle+i*.13f,r=parry_>0?12.f:16.f;
          dot(t,feet.x+r*std::cos(a),feet.y-8+r*std::sin(a),2,3,parry_>0?0x83dfd3:0xffe3a1);
        }
      }
    };
    auto creature=[&]{
      int f=cleared()?3:phase==Phase::Aim || phase==Phase::Stamp?1:phase==Phase::Rush?2:0;
      sf::Sprite s(ramArt_,{f*48,0,48,40});s.setOrigin(24,34);
      s.setScale((fighting()?aim_.x:1)<0?-1.f:1.f,1);
      s.setPosition(std::round(ram.x),std::round(ram.y)-(phase==Phase::Stamp?3:0));t.draw(s);
      if(phase==Phase::Stunned || phase==Phase::Rest){
        for(int i=-1;i<=1;++i)dot(t,ram.x+i*7,ram.y-37+2*std::sin(anim*9+i),3,2,0xffda71);
      }
    };

    if(feet.y<ram.y){hero();creature();}else{creature();hero();}
  }
 private:
  sf::Texture land_,ramArt_;sf::Image mask_;
  Point aim_{1,0},dash_{};
  float dodge_=0,dodgeDelay_=0,parry_=0,parryDelay_=0,strike_=0,strikeDelay_=0,invulnerable_=0;
  float messageTime_=0;
  int charges_=0;
  bool contacted_=false;
  std::string message_;
  static Point unit(Point a,Point b){
    float len=distance(a,b);return len>.001f?Point{(b.x-a.x)/len,(b.y-a.y)/len}:Point{1,0};
  }
  static bool arena(Point p){
    float x=(p.x-192)/79.f,y=(p.y-105)/50.f;return x*x+y*y<=1;
  }
  bool clear(Point p)const{
    if(p.x<3 || p.x>=381 || p.y<8 || p.y>=441)return false;
    if(fighting() && !arena(p))return false;
    if(!cleared() && p.y<56)return false;
    for(int off:{-2,2})if(mask_.getPixel(unsigned(p.x+off),unsigned(p.y)).r<128)return false;
    return true;
  }
  void slide(Point d,float step){
    Point x{feet.x+d.x*step,feet.y};if(clear(x))feet=x;
    Point y{feet.x,feet.y+d.y*step};if(clear(y))feet=y;
  }
  void say(std::string msg,float seconds=1.15f){message_=std::move(msg);messageTime_=seconds;}
  void resetActions(){dodge_=dodgeDelay_=parry_=parryDelay_=strike_=strikeDelay_=invulnerable_=0;}
  void startAim(){phase=Phase::Aim;clock=0;aim_=unit(ram,feet);contacted_=false;}
  void nextAttack(){
    if(++charges_%2){phase=Phase::Stamp;clock=0;contacted_=false;}
    else startAim();
  }
  float waveRadius()const{return 5+clock*82;}
  void contact(){
    if(contacted_)return;
    contacted_=true;
    if(parry_>0){phase=Phase::Stunned;clock=0;say("Parried! Strike the copperhorn.");return;}
    if(dodge_>0 || invulnerable_>0)return;
    invulnerable_=1;--hearts;
    if(hearts==0){
      // Retry on this shelf with the same route bonus and altitude.
      hearts=path==Path::Fern?4:3;guard=3;charges_=0;ram={192,95};feet={192,141};
      resetActions();startAim();say("Catch your breath. Try the clearing again.",1.4f);
    }else say("Grazed! Move sideways before the charge.");
  }
  void enemy(float dt){
    switch(phase){
      case Phase::Aim:
        if(clock<.48f)aim_=unit(ram,feet);
        if(clock>=1.05f){phase=Phase::Rush;clock=0;contacted_=false;}
        break;
      case Phase::Rush:{
        Point q{ram.x+aim_.x*140*dt,ram.y+aim_.y*140*dt};
        if(arena(q))ram=q;
        if(distance(ram,feet)<15){contact();if(phase!=Phase::Rush)return;}
        if(!arena(q) || clock>.86f){phase=Phase::Rest;clock=0;}
        break;
      }
      case Phase::Stunned:case Phase::Rest:
        if(clock>(phase==Phase::Rest?1.4f:path==Path::Cairn?2.8f:2.0f)){
          nextAttack();
        }
        break;
      case Phase::Stamp:if(clock>=.85f){phase=Phase::Wave;clock=0;}break;
      case Phase::Wave:
        if(std::abs(distance(ram,feet)-waveRadius())<6){contact();if(phase!=Phase::Wave)return;}
        if(waveRadius()>160){phase=Phase::Rest;clock=0;}break;
      default:break;
    }
  }
  static void dot(sf::RenderTarget& t,float x,float y,int w,int h,unsigned rgb){
    MountainArt::box(t,std::round(x),std::round(y),w,h,rgb);
  }
};
}
