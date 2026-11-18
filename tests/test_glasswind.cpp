#include "glasswind.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using Hollow=trail::Glasswind;
using Point=Hollow::Point;

void require(bool condition,const char* detail){
  if(!condition)throw std::runtime_error(detail);
}
void walk(Hollow& trail,Point goal){
  for(int frame=0;frame<800;++frame){
    float dx=goal.x-trail.feet.x,dy=goal.y-trail.feet.y,length=std::hypot(dx,dy);
    if(length<1)return;
    trail.update(.01f,{dx/length,dy/length});
  }
  throw std::runtime_error("Blocked route on the shipped walking mask");
}
template<class Condition> void waitFor(Hollow& trail,Condition ready){
  for(int frame=0;frame<2000 && !ready();++frame)trail.update(.01f,{});
  require(ready(),"Encounter failed to reach its expected phase");
}

void testRoutes(){
  Hollow trail;
  trail.enter(false,false);
  for(Point p:std::array<Point,5>{{{192,351},{138,335},{90,307},{62,268},{98,237}}})walk(trail,p);
  trail.use();require(trail.hint().find("ward earned")!=std::string::npos,"Pool interaction");
  for(Point p:std::array<Point,4>{{{82,199},{130,174},{158,153},{192,137}}})walk(trail,p);
  require(trail.fighting(),"Western entrance");
  waitFor(trail,[&]{return trail.hint().find("ward caught")!=std::string::npos;});
  require(trail.hearts==3,"Ward absorbs the first contact");

  trail.enter(false,false);
  for(Point p:std::array<Point,6>{{{192,351},{237,333},{284,353},{322,317},{310,282},{266,253}}})walk(trail,p);
  trail.use();require(trail.hint().find("bell answers")!=std::string::npos,"Bell interaction");
  for(Point p:std::array<Point,4>{{{300,219},{274,188},{233,162},{192,137}}})walk(trail,p);
  auto reflect=[&]{
    waitFor(trail,[&]{return trail.phase==Hollow::Phase::Feathers;});
    float distance=std::hypot(trail.bird.x-trail.feet.x,trail.bird.y-trail.feet.y);
    for(int i=0;i<int((distance-20)/.87f);++i)trail.update(.01f,{});
    trail.parry();
    waitFor(trail,[&]{return trail.phase!=Hollow::Phase::Feathers;});
  };
  reflect();require(trail.guard==2 && trail.phase==Hollow::Phase::Shadows,"Reflected feather");
  for(int drop=0;drop<3;++drop){
    for(int i=0;i<70;++i)trail.update(.01f,{});
    trail.update(.01f,{drop%2?1.f:-1.f,0});
    trail.dodge();
    for(int i=0;i<60;++i)trail.update(.01f,{});
  }
  waitFor(trail,[&]{return trail.phase==Hollow::Phase::Perch;});
  require(trail.hearts==3,"Dodge clears the shadow");
  walk(trail,trail.bird);trail.use();require(trail.guard==1,"Grounded counterattack");
  reflect();require(trail.cleared() && trail.victory,"Guardian victory");
  walk(trail,{192,104});walk(trail,{192,43});require(trail.atExit(),"Exit reaches the upper mountain");
  trail.enter(true,true);require(trail.atExit() && !trail.fighting(),"Saved descent");
}
}
int main(){
  try{testRoutes();std::cout<<"Glasswind routes and guardian passed.\n";}
  catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
