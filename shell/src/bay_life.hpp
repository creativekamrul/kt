#pragma once

#include "willow_bay.hpp"
#include "sdp/store.hpp"
#include <array>

namespace trail {

class BayLife {
 public:
  using Point = WillowBay::Point;
  enum class Target { None, Sol, Nia, Kit, Flowers, Sign, Bridge, WestBank, Ridge, Pass, Lodge };
  static constexpr Point flowers{239,188}, sign{258,160};
  struct Resident {
    const char* name;
    std::array<Point,3> stops;
    Point feet;
    float rest, stride=0;
    int next=1, facing=1, frame=0;
  };
  std::array<Resident,3> folk{{
    {"Sol", {{{191,183},{212,177},{199,192}}}, {191,183},2},
    {"Nia", {{{273,194},{272,211},{251,200}}}, {273,194},4},
    {"Kit", {{{296,182},{313,192},{329,187}}}, {296,182},3}
  }};
  BayLife() {
    if(!cast_.loadFromFile("shell/assets/bay-villagers.png") ||
       !moth_.loadFromFile("shell/assets/wind-moth.png"))
      throw std::runtime_error("Cannot load Willow Bay villagers");
  }
  void load(sdp::IDataStore& store,int child) {
    store_=&store; child_=child;
    for(int i=0;i<Count;++i) done_[i]=store.hasMilestone(child,keys_[i]);
  }
  Point point(Target t) const {
    switch(t){
      case Target::Sol:return folk[0].feet;
      case Target::Nia:return folk[1].feet;
      case Target::Kit:return folk[2].feet;
      case Target::Flowers:return flowers;
      case Target::Sign:return sign;
      case Target::Bridge:return WillowBay::bridge;
      case Target::WestBank:return {118,168};
      case Target::Ridge:return {91,162};
      case Target::Pass:return WillowBay::pass;
      case Target::Lodge:return WillowBay::lodge;
      default:return {};
    }
  }
  Target nearby(Point p,bool repaired=false) const {
    Target best=Target::None; float limit=20;
    for(auto t:{Target::Bridge,Target::WestBank,Target::Pass,Target::Lodge,Target::Sol,
                Target::Nia,Target::Kit,Target::Flowers,Target::Sign,Target::Ridge}){
      if(t==Target::Ridge && !repaired)continue;
      if(t==Target::Flowers && (!done_[Basket] || done_[Blooms]))continue;
      if(t==Target::Sign && (!done_[Ribbon] || done_[Raised]))continue;
      auto q=point(t); float dist=std::hypot(p.x-q.x,p.y-q.y);
      if(dist<=limit){best=t;limit=dist;}
    }
    return best;
  }
  Target objective(bool repaired) const {
    if(done_[Basket] && !done_[Blooms])return Target::Flowers;
    if(done_[Blooms] && !done_[Delivered])return Target::Sol;
    if(done_[Ribbon] && !done_[Raised])return Target::Sign;
    return repaired ? Target::Ridge : Target::Bridge;
  }
  const char* label(Target t) const {
    switch(t){
      case Target::Sol:return "Talk to Sol, the carpenter";
      case Target::Nia:return "Talk to Nia, the florist";
      case Target::Kit:return "Talk to Kit, the courier";
      case Target::Flowers:return "Gather creek bells";
      case Target::Sign:return "Tie the trail pennant";
      case Target::Ridge:return "Climb Bracken Rise";
      case Target::Pass:return "Take the mountain pass";
      case Target::Lodge:return "Read the lodge journal";
      case Target::Bridge:case Target::WestBank:return "Willow Bridge";
      default:return "";
    }
  }
  bool inDialogue() const {return !line.empty();}
  bool sparring() const {return spar_;}
  bool ready() const {return beat_>=1.15f && beat_<2.15f;}
  void dismiss(){line.clear();offer_=false;}
  void cancel(){dismiss();spar_=false;}
  void confirm() {
    bool start=offer_;dismiss();
    if(start){spar_=true;hits_=0;beat_=0;burst_=0;}
  }
  void interact(Target t){
    switch(t){
      case Target::Nia:
        speaker="Nia";portrait_=1;
        if(!done_[Basket]){mark(Basket);line="Sol has worked since dawn. Pick some pink creek bells for his bench?";}
        else if(!done_[Blooms])line="The creek bells grow beside the west path. Leave the roots for next spring.";
        else if(!done_[Delivered])line="What a bunch! Sol is down by the bridge.";
        else line="Sol put the bells right by his tools. You made his day.";
        break;
      case Target::Flowers:mark(Blooms);speaker="Creek bells";portrait_=-1;
        line="You tuck a small bunch into your pack. Find Sol by the bridge.";break;
      case Target::Sol:
        speaker="Sol";portrait_=0;
        if(done_[Blooms] && !done_[Delivered]){
          mark(Delivered);line="For my bench? My old mum grew these. Give Nia my thanks.";
        }else if(!done_[Ribbon]){
          mark(Ribbon);line="Take this red pennant. Tie it at the fork so folks can find the pass.";
        }else if(!done_[Raised])line="Look for the bare post by the fork. Two knots should hold.";
        else line="Saw that pennant catch the breeze. Now, about these bridge planks...";
        break;
      case Target::Sign:mark(Raised);speaker="Trail fork";portrait_=-1;
        line="Two knots and a tug. The red pennant unfurls toward the pass.";break;
      case Target::Kit:
        speaker="Kit";portrait_=2;offer_=true;
        line=done_[Sparred] ? "Another round? My wind moth has been practicing!" :
          "Meet Flick, my wind moth! Parry three gusts when its wings flash gold.";
        break;
      default:break;
    }
  }
  void swing() {
    if(!spar_ || burst_>0)return;
    bool hit=ready();beat_=0;burst_=.48f;parried_=hit;
    if(hit && ++hits_==3){spar_=false;mark(Sparred);speaker="Kit";portrait_=2;
      line="Three clean parries! Flick folds its wings and lands on your pack.";}
  }
  void update(float dt,Point player){
    if(inDialogue())return;
    if(spar_){burst_=std::max(0.f,burst_-dt);if(burst_<=0)beat_=std::fmod(beat_+dt,2.7f);return;}
    for(auto& r:folk){
      if(WillowBay::near(r.feet,player,13)){
        r.frame=0;r.facing=heading(player.x-r.feet.x,player.y-r.feet.y);continue;
      }
      if(r.rest>0){r.rest-=dt;r.frame=0;continue;}
      auto q=r.stops[r.next];float dx=q.x-r.feet.x,dy=q.y-r.feet.y;
      float length=std::hypot(dx,dy),step=dt*11;
      if(length<=step){r.feet=q;r.next=(r.next+1)%3;r.rest=2.4f;r.frame=0;continue;}
      r.facing=heading(dx,dy);r.feet.x+=step*dx/length;r.feet.y+=step*dy/length;
      r.stride+=dt;r.frame=int(r.stride*7)%4;
    }
  }
  void drawPerson(sf::RenderTarget& t,int i,Point feet,bool watching) const {
    const auto& r=folk[i];sf::Sprite s(cast_,{r.frame*24,i*112+(watching?2:r.facing)*28,24,28});
    s.setOrigin(12,25);s.setPosition(std::round(feet.x),std::round(feet.y));t.draw(s);
  }
  void portrait(sf::RenderTarget& t,int x,int y) const {
    if(portrait_<0)return;
    sf::Sprite s(cast_,{0,portrait_*112+28,24,23});s.setScale(2,2);s.setPosition(float(x),float(y));t.draw(s);
  }
  void drawProps(sf::RenderTarget& t,float time) const {
    int x=int(flowers.x),y=int(flowers.y);
    for(int i=0;i<5;++i){
      int cx=x-6+i*3,cy=y-4-(i*7)%4;
      pixel(t,cx,cy,1,5,0x47765b);
      if(!done_[Blooms] || i==0 || i==4){
        pixel(t,cx-2,cy-1,4,2,0xc5517a);pixel(t,cx-1,cy-2,2,2,0xffb9b3);
      }
    }
    x=int(sign.x);y=int(sign.y);pixel(t,x-1,y-15,2,16,0x88664f);
    if(done_[Raised]){
      int lift=int(time*3)%2;pixel(t,x+1,y-15,5,5,0xe56553);
      pixel(t,x+6,y-15+lift,4,3,0xff9670);
    }else pixel(t,x-2,y-16,4,2,0xffcc86);
    if(done_[Delivered]){
      pixel(t,190,176,4,5,0x428d92);pixel(t,188,174,4,2,0xf58bae);pixel(t,192,173,4,3,0xffb1a9);
    }
  }
  void drawMoth(sf::RenderTarget& t,Point p,float time) const {
    float lunge=ready()? std::max(0.f,1.f-std::abs(beat_-1.7f)*2):0;
    int x=int(p.x+24-lunge*12),y=int(p.y-21+2*std::sin(time*6));
    sf::Sprite moth(moth_,{(int(time*6)%2)*32,ready()?26:0,32,26});
    moth.setOrigin(16,14);moth.setPosition(float(x),float(y));t.draw(moth);
    int handX=int(p.x)+4,handY=int(p.y)-11-(burst_>0?3:0);
    pixel(t,handX,handY,5,4,0x365961);pixel(t,handX+2,handY,3,2,0x7cbbae);
    pixel(t,handX+5,handY-1,3,3,0xd29567);pixel(t,handX+6,handY-2,2,2,0xffca91);
    // The gust travels from Flick to the hiker during the gold parry window.
    if(beat_>.45f){
      float u=std::clamp((beat_-.45f)/1.7f,0.f,1.f);
      int gx=int(p.x+27-u*20),gy=int(p.y-17+3*std::sin(u*3.14f));
      for(int j=-1;j<=1;++j)
        pixel(t,gx+std::abs(j)*2,gy+j*2,4,1,ready()?0xffed99:0xd2f5db);
    }
    if(burst_>0){
      int frame=int((.48f-burst_)*10);
      // A sweeping guard, with three sparks breaking away on contact.
      for(int j=0;j<5;++j){
        int sx=int(p.x)+6+int(6*std::sin((j+frame)*.4f));
        pixel(t,sx,int(p.y)-19+j*2+frame,2,3,parried_?0xffeeb0:0xa3dacc);
      }
      if(parried_)for(int j=-1;j<=1;++j)
        pixel(t,int(p.x)+14+frame*2,int(p.y)-15+j*(3+frame),2,2,0xffbe64);
    }
    if(burst_>0 || ready()){
      unsigned c=burst_>0 && !parried_?0xc7b9cb:0xffdb73;
      int bx=int(p.x)+9,by=int(p.y)-11;
      for(int j=-2;j<=2;++j)pixel(t,bx+4-std::abs(j)*2,by+j*2,2,3,c);
    }
  }
  std::string sparText() const {
    return "Flick   "+std::to_string(hits_)+" / 3     "+
      (burst_>0?(parried_?"Parried!":"Watch for the gold flash."):
       ready()?"PARRY NOW":"Watch Flick spread its wings...");
  }
  bool offer()const{return offer_;}
  std::string line,speaker;
 private:
  enum Fact { Basket,Blooms,Delivered,Ribbon,Raised,Sparred,Count };
  static constexpr std::array<const char*,Count> keys_={"bay.bells.accepted","bay.bells.picked",
    "bay.bells.delivered","bay.pennant.accepted","bay.pennant.raised","bay.flick.parried"};
  std::array<bool,Count> done_{};
  sdp::IDataStore* store_=nullptr;int child_=0;
  sf::Texture cast_,moth_;
  bool spar_=false,offer_=false,parried_=false;
  int hits_=0,portrait_=0;
  float beat_=0,burst_=0;
  void mark(Fact f){if(!done_[f]){store_->awardMilestone(child_,keys_[f]);done_[f]=true;}}
  static int heading(float x,float y){return std::abs(x)>std::abs(y)?(x>0?2:3):(y>0?1:0);}
  static void pixel(sf::RenderTarget& t,int x,int y,int w,int h,unsigned rgb){
    sf::RectangleShape shape({float(w),float(h)});shape.setPosition(float(x),float(y));
    shape.setFillColor(sf::Color((rgb<<8)|255u));t.draw(shape);
  }
};
}
