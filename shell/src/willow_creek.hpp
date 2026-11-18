// Original pixel scenery for the overhead Willow Bridge crossing.
#pragma once
#include <SFML/Graphics.hpp>
#include <cmath>
#include <stdexcept>

namespace trail {
class WillowCreek {
 public:
  WillowCreek() {
    if (!scene_.create(240, 73)) throw std::runtime_error("Could not create creek scenery");
    scene_.clear(sf::Color(60,161,167));
    for (int y=0; y<73; ++y) {
      int bend = int(6 * std::sin(y / 12.0));
      for (int side : {0,1}) {
        int edge = (side ? 194 : 42) + bend;
        strip(side ? edge : 0,y,side ? 240-edge : edge,1,0x526c58);
        strip(side ? edge+3 : 0,y,side ? 240-edge-3 : edge-3,1,0xc39e72);
        strip(side ? edge+5 : 0,y,side ? 240-edge-5 : edge-5,1,0x87b767);
      }
    }
    for (int i=0; i<125; ++i) {
      int x=54+(i*47)%130, y=(i*31)%73;
      strip(x,y,2+i%8,1,i%3 ? 0x88cfbe : 0x37959e);
    }
    for (int i=0; i<55; ++i) {
      int x=(i*17)%33+(i%2 ? 204 : 0), y=(i*27)%72;
      strip(x,y,1,2,0x687f53);
      if (i%3==0) strip(x-1,y-1,3,2,0xffba84);
    }
    for (int x : {7,29,213,235}) {
      int y = (x%3)*3;
      strip(x,y,4,23,0x735f48);
      for (int j=-12;j<=12;++j) {
        int depth=15-std::abs(j)/2 + ((j+12)*7)%5;
        strip(x+j,0,1,y+depth,0x37795e);
        if ((j+12)%4<2) strip(x+j,0,1,y+depth-4,0x78ac62);
      }
    }
    scene_.display();
  }
  void draw(sf::RenderTarget& target) const {
    sf::Sprite sprite(scene_.getTexture());
    sprite.setPosition(0,138);
    sprite.setScale(4,4);
    target.draw(sprite);
  }
 private:
  void strip(int x,int y,int w,int h,unsigned rgb) {
    sf::RectangleShape shape({float(w),float(h)});
    shape.setPosition(float(x),float(y));
    shape.setFillColor(sf::Color((rgb<<8)|255u));
    scene_.draw(shape);
  }
  sf::RenderTexture scene_;
};
}
