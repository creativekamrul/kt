#include "../shell/src/pass_guardian.hpp"
#include "test_framework.hpp"

using trail::PassGuardian;

SDP_TEST(guardians, fox_telegraphs_and_keeps_earned_bells_after_a_miss) {
  PassGuardian fox;
  fox.use();
  fox.x=80;
  fox.tick(1.7f,0); fox.tick(.34f,0);
  fox.use();
  CHECK_EQ(fox.seals,1);
  fox.tick(1.6f,0);
  fox.tick(1.7f,0); fox.tick(.34f,0);
  CHECK(fox.phase==PassGuardian::Phase::Rest);
  CHECK_EQ(fox.seals,1);
}

SDP_TEST(guardians, bellback_requires_the_star_and_stops_after_three_parries) {
  PassGuardian turtle;
  turtle.kind=1;
  turtle.use();
  turtle.tick(1.7f,0); turtle.parry();
  CHECK_EQ(turtle.seals,0);
  for(int i=0;i<3;++i){
    turtle.tick(1.6f,0); turtle.tick(1.7f,0); turtle.tick(.6f,0);
    turtle.parry();
  }
  CHECK(turtle.phase==PassGuardian::Phase::Clear);
  turtle.tick(5,0); turtle.parry();
  CHECK_EQ(turtle.seals,3);
}
