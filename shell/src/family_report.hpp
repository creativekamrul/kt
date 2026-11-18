#pragma once
#include "sdp/campaign.hpp"
#include "sdp/content.hpp"
#include <algorithm>
#include <map>

namespace trail {
struct FamilyReport {
  struct Skill { std::string name; int correct=0,attempts=0; };
  struct Event { std::string at,label; };
  int altitude=0,seconds=0,correct=0,attempts=0;
  std::vector<sdp::AreaPlay> areas;
  std::vector<Skill> skills;
  std::vector<Event> events;

  static FamilyReport read(sdp::IDataStore& store,int child,const sdp::StringTable& strings){
    FamilyReport r;
    if(auto progress=store.progression(child))r.altitude=progress->altitude;
    r.areas=store.areaPlay(child);
    for(const auto& row:r.areas){
      r.seconds+=row.seconds;
      r.events.push_back({row.last_played,"Played in "+row.area});
    }
    std::sort(r.areas.begin(),r.areas.end(),[](const auto& a,const auto& b){return a.seconds>b.seconds;});
    std::map<std::string,Skill> counts;
    const auto history=store.history(child);
    for(const auto& a:history){
      std::string name=strings.get("skill."+a.skill.id,a.skill.id);
      auto& skill=counts[a.skill.id];skill.name=name;
      ++skill.attempts;++r.attempts;
      if(a.correct){++skill.correct;++r.correct;}
      r.events.push_back({a.created_at,name+(a.correct?"  /  solved":"  /  tried")});
    }
    for(const auto& entry:counts)r.skills.push_back(entry.second);
    static const std::map<std::string,std::string> named{
      {"bay.bells.accepted","Started Nia's creek bells errand"},{"bay.bells.picked","Picked creek bells"},
      {"bay.bells.delivered","Delivered creek bells to Sol"},{"bay.pennant.accepted","Collected Sol's pennant"},
      {"bay.pennant.raised","Raised the trail pennant"},{"bay.flick.parried","Finished wind moth practice"},
      {"mountain.bracken.copperhorn","Cleared the copperhorn's pass"},
      {"mountain.glasswind.moonkite","Cleared the moonkite's roost"},
      {"guardian.emberfox","Rang three bells at Ember Lookout"},{"guardian.bellback","Met the Bellback challenge"},
      {"mountain.summit.homecoming","Raised the summit flag"}};
    for(const auto& milestone:store.milestones(child)){
      auto found=named.find(milestone.name);
      if(found!=named.end())r.events.push_back({milestone.earned_at,found->second});
      for(const auto& level:sdp::expeditions())
        for(int stage=1;stage<=int(level.tasks.size());++stage)
          if(milestone.name=="expedition."+std::string(level.id)+"."+std::to_string(stage))
            r.events.push_back({milestone.earned_at,std::string(level.title)+"  /  stage "+std::to_string(stage)+" complete"});
    }
    std::reverse(r.events.begin(),r.events.end());
    std::stable_sort(r.events.begin(),r.events.end(),[](const auto& a,const auto& b){return a.at>b.at;});
    return r;
  }
};
}
