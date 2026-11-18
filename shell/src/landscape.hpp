// KT's trail map is drawn on a small pixel canvas, then enlarged without filtering.
#pragma once

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace trail {

inline int ground(int x) {
  return 67 - x / 12 + static_cast<int>(2 * std::sin(x * .07));
}

class Landscape {
 public:
  Landscape() {
    if (!canvas_.create(240, 80)) throw std::runtime_error("Could not create trail map");
    canvas_.setSmooth(false);
  }

  void draw(sf::RenderTarget& target, double progress, const std::string& zone) {
    const bool alpine = zone == "snowline" || zone == "summit";
    const int x = 14 + static_cast<int>(195 * std::clamp(progress, 0., 1.));
    if (x != last_x_ || alpine != last_alpine_) {
      paint(x, alpine);
      last_x_ = x;
      last_alpine_ = alpine;
    }
    sf::Sprite sprite(canvas_.getTexture());
    sprite.setScale(4.f, 4.f);
    target.draw(sprite);
  }

 private:
  void paint(int hiker_x, bool alpine) {
    canvas_.clear(sf::Color(168, 204, 198));
    box(0, 23, 240, 20, 0xcbd8b8);
    box(0, 43, 240, 37, 0xf0d7a4);
    for (int y = -8; y <= 8; ++y) {
      const int span = static_cast<int>(std::sqrt(72 - y * y));
      box(171 - span, 17 + y, 2 * span + 1, 1, 0xffe1a2);
    }
    for (int x = 0; x < 240; ++x) {
      const int far = 13 + std::abs((x + 6) % 74 - 37) * 2 / 3;
      const int near = 29 + std::abs((x + 44) % 95 - 47) / 2;
      box(x, far, 1, 80 - far, 0x879f9f);
      if (far < 21 + x % 4) box(x, far, 1, 21 + x % 4 - far, 0xe5e6ce);
      box(x, near, 1, 80 - near, 0x617f7c);
    }
    for (int x = -2; x < 244; x += 4) pine(x, 55 - x / 27, 6 + (x + 8) % 9, 0x42685f);
    for (int x = 0; x < 240; ++x) {
      const int y = ground(x);
      box(x, y - 5, 1, 84 - y, alpine ? 0xc5d8cb : 0x849860);
      box(x, y, 1, 3, 0xeac48b);
      box(x, y + 3, 1, 1, 0xb59566);
      if (x % 9 == 0) box(x, y + 7 + x % 5, 2, 1, 0xe3ce8a);
    }
    for (int x : {6, 18, 33, 69, 76, 115, 147, 185, 233})
      pine(x, ground(x) - 4, 9 + x % 12, 0x315d50);
    // Canvas tent, entrance and pennant mark the top of this stretch.
    const int camp = ground(214);
    for (int i = 0; i < 11; ++i) {
      box(213 - i, camp - 12 + i, 2 * i + 1, 1, 0xd08b54);
      box(213, camp - 12 + i, i + 1, 1, 0xedb36c);
      if (i > 4) box(213 - (i - 4) / 2, camp - 12 + i, i - 4, 1, 0x604e43);
    }
    box(224, camp - 17, 1, 16, 0x715844);
    box(225, camp - 17, 5, 3, 0xb45a43);

    const int x = hiker_x;
    const int y = ground(x);
    box(x - 4, y + 1, 9, 2, 0xa78b64);
    box(x - 2, y - 3, 2, 5, 0x343f41);
    box(x + 1, y - 3, 2, 4, 0x343f41);
    box(x - 2, y - 8, 5, 6, 0xbc6240);
    box(x - 4, y - 8, 3, 5, 0x694e3c);
    box(x - 2, y - 12, 5, 4, 0xf3cb96);
    box(x - 3, y - 13, 7, 2, 0x644d40);
    box(x - 2, y - 15, 4, 2, 0xc79254);
    canvas_.display();
  }

  void box(int x, int y, int w, int h, unsigned rgb) {
    sf::RectangleShape shape({static_cast<float>(w), static_cast<float>(h)});
    shape.setPosition(static_cast<float>(x), static_cast<float>(y));
    shape.setFillColor(sf::Color((rgb << 8) | 255u));
    canvas_.draw(shape);
  }

  void pine(int x, int y, int height, unsigned rgb) {
    box(x, y - height, 2, height, 0x4f6050);
    for (int row = 0; row < height - 2; ++row) {
      const int width = row / 4 + (row % 5) / 3;
      box(x - width, y - height + row, 2 * width + 1, 1, rgb);
    }
  }

  int last_x_ = -1;
  bool last_alpine_ = false;
  sf::RenderTexture canvas_;
};

}  // namespace trail
