#pragma once
#include <algorithm>
#include <cmath>
#include <string>

namespace trail {

struct PassGuardian {
  enum class Phase { Meet, Warning, Incoming, Opening, Rest, Clear };
  int kind=0, seals=0, target=1;
  float x=160, clock=0, dash=0, facing=1;
  Phase phase=Phase::Meet;
  std::string feedback;

  const char* name() const { return kind==0?"Ember Fox":"Bellback Tortoise"; }
  const char* camp() const { return kind==0?"Ember Lookout":"Bellflower Terrace"; }
  const char* milestone() const { return kind==0?"guardian.emberfox":"guardian.bellback"; }
  int lane() const { return std::clamp(int((x-70)/60),0,2); }
  void windup(){
    target=lane(); clock=0; phase=Phase::Warning; dash=0;
  }
  void winSeal(){
    ++seals;clock=0;
    phase=seals==3?Phase::Clear:Phase::Rest;
    feedback=seals==3?"Three bells! The guardian bows.":"That bell rang out!";
  }
  void use(){
    if(phase==Phase::Meet)windup();
    else if(kind==0 && phase==Phase::Opening)winSeal();
  }
  void parry(){
    if(kind!=1 || phase!=Phase::Incoming)return;
    if(clock>=.50f && clock<=1.18f)winSeal();
    else {phase=Phase::Rest;clock=0;feedback="A little early. Wait for the big star.";}
  }
  void dodge(){if(kind==0 && phase==Phase::Warning && dash<=0)dash=.21f;}
  void tick(float dt,float side){
    if(phase==Phase::Meet || phase==Phase::Clear)return;
    clock+=dt;
    if(std::abs(side)>.12f)facing=side>0?1.f:-1.f;
    x=std::clamp(x+dt*(side*83+(dash>0?facing*155:0)),76.f,244.f);
    dash=std::max(0.f,dash-dt);
    if(phase==Phase::Warning && clock>=1.65f){
      phase=Phase::Incoming;clock=0;
    }else if(phase==Phase::Incoming && kind==0 && clock>=.32f){
      bool safe=lane()!=target;
      phase=safe?Phase::Opening:Phase::Rest;clock=0;
      feedback=safe?"The fox missed. Ring the bell!":"See the paw prints? Step onto another path.";
    }else if(phase==Phase::Incoming && kind==1 && clock>1.18f){
      phase=Phase::Rest;clock=0;feedback="Try again when the big star appears.";
    }else if(phase==Phase::Opening && clock>2.4f){
      phase=Phase::Rest;clock=0;feedback="Ring your bell after the fox runs past.";
    }else if(phase==Phase::Rest && clock>1.5f)windup();
  }
};
}
