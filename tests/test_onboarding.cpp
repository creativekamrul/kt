#include "bay_life.hpp"
#include "bracken_rise.hpp"
#include <iostream>

namespace {
void require(bool condition,const char* why){
  if(!condition)throw std::runtime_error(why);
}
void escort(){
  using Target=trail::BayLife::Target;
  auto db=sdp::openMemoryStore();
  int child=db->createChild("Tester",0);
  trail::BayLife bay;
  bay.load(*db,child);
  bay.interact(Target::Nia);bay.confirm();
  require(bay.followingGuide(),"Nia accepted the walk");
  auto start=bay.point(Target::Nia);
  for(int i=0;i<120;++i)bay.update(.016f,{410,200});
  require(trail::WillowBay::near(start,bay.point(Target::Nia),.01f),"Guide waits");
  bay.load(*db,child);
  require(bay.followingGuide(),"Unfinished walk resumes");
  trail::WillowBay map;
  for(int i=0;i<500 && !bay.inDialogue();++i){
    auto old=bay.point(Target::Nia);
    bay.update(.016f,{old.x,old.y+8});
    auto pos=bay.point(Target::Nia);
    auto legal=map.move(old,pos.x-old.x,pos.y-old.y,false);
    require(map.near(pos,legal,.001f),"Escort stays on the walking mask");
  }
  require(db->hasMilestone(child,"tutorial.bridge.arrived"),"Walk reaches the bridge");
  bay.confirm();bay.load(*db,child);
  bay.interact(Target::Nia);
  require(db->hasMilestone(child,"bay.bells.accepted"),"Flower errand remains available");
}
void homecoming(){
  using Target=trail::BayLife::Target;
  auto db=sdp::openMemoryStore();
  int child=db->createChild("Visitor",0), sibling=db->createChild("Sibling",0);
  db->awardMilestone(child,"guardian.emberfox");
  trail::BayLife bay;bay.load(*db,child,false);
  require(bay.nearby(bay.point(Target::Fox))==Target::Fox,"Cleared fox visits the plaza");
  require(bay.nearby(bay.point(Target::Tortoise))!=Target::Tortoise,"Bellback waits for its own victory");
  bay.solRemembers("You met the Ember Fox!");
  bay.confirm();require(db->hasMilestone(child,"bay.pennant.accepted"),"Sol can still offer the pennant");
  bay.load(*db,sibling,false);
  require(bay.nearby(bay.point(Target::Fox))!=Target::Fox,"Visitors belong to the climber");
}
void practice(){
  using Rise=trail::BrackenRise;
  Rise r;r.enter(false,false,false);r.feet={192,145};
  r.update(.016f,{});
  require(r.lesson==Rise::Lesson::Dodge && !r.fighting(),"Practice before the guardian");
  r.dodge();
  require(r.lesson==Rise::Lesson::Dodge,"Early dodge can be retried");
  for(int i=0;i<140;++i)r.update(.01f,{});
  r.dodge();require(r.lesson==Rise::Lesson::Parry,"Timed dodge advances practice");
  for(int i=0;i<140;++i)r.update(.01f,{});
  r.parry();require(r.lesson==Rise::Lesson::Ready && r.hearts==3,"Safe practice parry");
  require(r.finishLesson(),"Climber chooses when to continue");
  r.enter(false,false,true);r.feet={192,145};r.update(.01f,{});
  require(r.fighting(),"Learned climbers can enter the fight");
  r.enter(true,true,false);r.feet=r.ram;r.greet();
  require(r.objective().find("bows")!=std::string::npos,"Cleared Copperhorn greets the climber");
  require(r.cleared() && !r.practicing(),"Cleared saves stay clear");
}
}
int main(){
  try{escort();practice();homecoming();}
  catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
