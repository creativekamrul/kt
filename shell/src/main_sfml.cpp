#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

#include "mountain_art.hpp"
#include "willow_creek.hpp"
#include "bay_life.hpp"
#include "bracken_rise.hpp"
#include "glasswind.hpp"
#include "sdp/campaign.hpp"
#include "sdp/content.hpp"
#include "sdp/engine.hpp"
#include "sdp/progression.hpp"
#include "sdp/session.hpp"
#include "sdp/store.hpp"
#include "sdp/view.hpp"

void trailSound(const char* name);
void trailMusic(bool enabled);

namespace {
using namespace sdp;

const sf::Color ink(39, 59, 51), muted(83, 99, 82), paper(249, 244, 223);
const sf::Color green(48, 90, 69), gold(237, 197, 130);
const Rect action{52, 605, 410, 62}, skip{487, 605, 421, 62};


sf::VideoMode windowSize() {
  const auto screen = sf::VideoMode::getDesktopMode();
  const float scale = std::min({1.f, screen.width / 960.f, (screen.height - 80.f) / 700.f});
  return sf::VideoMode(static_cast<unsigned>(960 * scale), static_cast<unsigned>(700 * scale));
}

enum class Page { Bay, Ridge, Hollow, Summit, Climb, Expeditions, Badges, Task, Reward, Bridge, Pause };

class Shell {
 public:
  explicit Shell(const std::string& db, bool fullscreen)
      : window_(fullscreen ? sf::VideoMode::getDesktopMode() : windowSize(), "KT", fullscreen ? sf::Style::None : sf::Style::Default),
        store_(openSqliteStore(db)),
        curriculum_(ContentLoader::loadContentDirectory(ContentLoader::defaultContentDir(), &generator_)),
        strings_(ContentLoader::loadStrings(ContentLoader::defaultContentDir() + "/strings.json")),
        engine_(EngineConfig{}, curriculum_), progression_(curriculum_),
        runner_(engine_, generator_, progression_, *store_, {curriculum_.sessionLength(), true, true}, &strings_) {
    fullscreen_ = fullscreen;
    if(fullscreen_)window_.setPosition({0,0});
    viewport();
    setWindowIcon();
    window_.setFramerateLimit(60);
    window_.setKeyRepeatEnabled(false);
    if (!font_.loadFromFile("C:/Windows/Fonts/segoeui.ttf") &&
        !font_.loadFromFile("assets/font.ttf"))
      throw std::runtime_error("KT needs a readable interface font.");
    int player = 0;
    for (const auto& child : store_->children())
      if (child.name == "Player") player = child.id;
    if (!player) player = store_->createChild("Player", 8);
    machine_.selectProfile(player);
    runner_.startSession(player);
    campaign_ = std::make_unique<TrailCampaign>(*store_, player);
    journal_ = campaign_->journal(runner_.history());
    life_.load(*store_,player);
    owlCleared_=store_->hasMilestone(player,"mountain.glasswind.moonkite");
    homecoming_=store_->hasMilestone(player,"mountain.summit.homecoming");
    if(!peakArt_.loadFromFile("shell/assets/summit-walk.png"))throw std::runtime_error("Missing summit landscape");
    pad_ = connectedPad();
    padMode_=pad_>=0;
    trailMusic(true);
  }

  void run() {
    while (window_.isOpen()) {
      viewport();
      sf::Event e{};
      while (window_.pollEvent(e)) {
        if (e.type == sf::Event::Closed) { window_.close(); return; }
        if (e.type == sf::Event::Resized) viewport();
        if (e.type == sf::Event::KeyPressed) {
          padMode_ = false;
          if(e.key.code>=0 && e.key.code<sf::Keyboard::KeyCount) keys_[e.key.code]=true;
          if (e.key.code == sf::Keyboard::F11) toggleFullscreen();
          else if (window_.hasFocus()) key(e.key.code);
        }
        if (e.type == sf::Event::JoystickConnected || e.type == sf::Event::JoystickDisconnected) {
          pad_ = connectedPad(); padLatch_=false; padMode_=pad_>=0;
        }
        if (e.type == sf::Event::JoystickButtonPressed && window_.hasFocus() && int(e.joystickButton.joystickId)==pad_) {
          padMode_ = true; gamepadButton(e.joystickButton.button);
        }
        if (e.type == sf::Event::KeyReleased && e.key.code>=0 && e.key.code<sf::Keyboard::KeyCount)
          keys_[e.key.code]=false;
        if (e.type == sf::Event::LostFocus) { held_ = -1; keys_.fill(false); }
        if (page_ == Page::Bridge && held_ >= 0 && e.type == sf::Event::MouseMoved)
          pointer_ = window_.mapPixelToCoords({e.mouseMove.x, e.mouseMove.y});
        if (page_ == Page::Bridge && held_ >= 0 && e.type == sf::Event::MouseButtonReleased &&
            e.mouseButton.button == sf::Mouse::Left) {
          const auto pos = window_.mapPixelToCoords({e.mouseButton.x, e.mouseButton.y});
          const int piece = held_;
          held_ = -1;
          for (int g = 0; g < int(span_->gaps.size()); ++g)
            if (deckSlot(span_->gaps[g]).contains(int(pos.x), int(pos.y))) {picked_=-1;place(piece, g);}
        }
        if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
          auto pos = window_.mapPixelToCoords({e.mouseButton.x, e.mouseButton.y});
          padMode_ = focusKeys_ = false;
          click(static_cast<int>(pos.x), static_cast<int>(pos.y));
        }
      }
      if (!window_.isOpen()) break;
      tick(std::min(.05f, frameClock_.restart().asSeconds()));
      viewport();
      draw();
      if ((padMode_ || focusKeys_) && !walking()) showFocus();
      window_.display();
    }
  }

 private:
  const ZoneDef* zone() const { return curriculum_.zone(runner_.progressionState().zone_id); }

  void fetch() {
    puzzle_ = runner_.nextPuzzle();
    encounterTime_=sf::Time::Zero;
    startMap(puzzle_);
  }
  void answer(int index) {
    if (!machine_.canAnswer() || index < 0 || index >= static_cast<int>(puzzle_.options.size())) return;
    const int elapsed=encounterTime_.asMilliseconds();
    result_ = puzzle_.route.empty() ? runner_.submitAnswer(puzzle_,index,elapsed) : runner_.submitRoute(visited_,elapsed);
    machine_.onResult(result_, runner_.complete());
    notice_.clear();
    if (result_.correct) {
      auto updated = campaign_->journal(runner_.history());
      if (journal_.rank != updated.rank) notice_ = "New rank: " + updated.rank;
      for (const auto& level : expeditions())
        if (journal_.altitude < level.altitude && updated.altitude >= level.altitude)
          notice_ += "   Unlocked: " + std::string(level.title);
      for (std::size_t i = 0; i < updated.badges.size(); ++i)
        if (updated.badges[i].earned() && !journal_.badges[i].earned())
          notice_ += "   Badge: " + updated.badges[i].title;
      journal_ = std::move(updated);
    }
  }
  void advance(bool retry = true) {
    switch (machine_.scene()) {
      case Scene::Mountain:
        runner_.startSession(machine_.childId());
        machine_.startClimbing(curriculum_.sessionLength());
        break;
      case Scene::TrailFeedback: machine_.continueTrail(runner_.complete()); break;
      case Scene::RetryPrompt:
        if (!retry) runner_.cancelRetry();
        machine_.onRetryDecision(retry, runner_.complete());
        break;
      case Scene::CampSummary: machine_.leaveSummary(); return;
      default: return;
    }
    if (machine_.canAnswer()) fetch();
  }
  bool atCampOrTrail() const {
    return machine_.scene() == Scene::Mountain || machine_.atSummary();
  }
  void rememberTrail() {
    savedVisited_=visited_; savedPos_=position_; savedCarry_=carrying_; savedClimb_=ropeOpen_;
    fromTrail_=true;
  }
  void restoreTrail() {
    startMap(puzzle_);
    visited_=savedVisited_; position_=savedPos_; carrying_=savedCarry_; ropeOpen_=savedClimb_;
    fromTrail_=false;
  }
  void enterBay() {
    if(page_==Page::Climb && !atCampOrTrail()) rememberTrail();
    page_=Page::Bay;
    bayPosition_={234,61}; direction_=1; moving_=false; flashTime_=0;
  }
  void enterRise(bool fromBridge,bool downhill=false) {
    ridgeFromBridge_=fromBridge;
    if(downhill && page_==Page::Climb && !atCampOrTrail())rememberTrail();
    rise_.enter(store_->hasMilestone(runner_.childId(),"mountain.bracken.copperhorn"),downhill);
    direction_=downhill?1:0; moving_=false;page_=Page::Ridge;
  }
  void enterHollow(bool downhill=false){
    if(page_==Page::Climb && !atCampOrTrail())rememberTrail();
    hollow_.enter(owlCleared_,downhill);
    direction_=downhill?1:0;moving_=false;page_=Page::Hollow;
  }
  void saveOwl(){
    if(!hollow_.victory)return;
    store_->awardMilestone(runner_.childId(),"mountain.glasswind.moonkite");
    owlCleared_=true;hollow_.victory=false;chime("camp");
  }
  void enterSummit(){
    peakProgress_=0;credits_=false;moving_=false;fromTrail_=false;page_=Page::Summit;
    machine_.selectProfile(runner_.childId());
  }
  void takePass() {
    if(runner_.progressionState().altitude>=90){enterSummit();return;}
    page_=Page::Climb;
    if (fromTrail_) restoreTrail();
    else {
      if(machine_.atSummary())machine_.leaveSummary();
      advance();
    }
  }
  void leaveBridge() {
    held_=picked_=-1;
    if(bridgeFromBay_) {
      bridgeFromBay_=false; page_=Page::Bay;
      journal_=campaign_->journal(runner_.history());
      moving_=false;
    } else openJournal();
  }
  void openJournal() {
    journal_ = campaign_->journal(runner_.history());
    if(page_==Page::Climb || page_==Page::Bay || page_==Page::Ridge || page_==Page::Hollow) {
      journalOrigin_=page_;
      if(page_==Page::Climb && !atCampOrTrail())rememberTrail();
    }
    page_ = Page::Expeditions;
  }
  void openLevel(int level) {
    if (level == 0) {
      bridgeFromBay_=page_==Page::Bay;
      replay_ = journal_.stages[0] == int(expeditions()[0].tasks.size());
      openSpan(replay_ ? 0 : journal_.stages[0]);
      return;
    }
    round_ = campaign_->next(level);
    if (!round_) return;
    hint_.clear();
    page_ = Page::Task;
    startMap(round_->puzzle);
  }
  void fieldAnswer(int index) {
    const auto outcome = campaign_->answer(round_->level, round_->stage, index);
    switch (outcome) {
      case ExpeditionOutcome::TryAgain: hint_ = round_->puzzle.hint; return;
      case ExpeditionOutcome::Placed:
      case ExpeditionOutcome::Unavailable: openJournal(); return;
      case ExpeditionOutcome::Advanced:
      case ExpeditionOutcome::Completed:
        journal_ = campaign_->journal(runner_.history());
        page_ = Page::Reward;
        return;
    }
  }
  void afterTask() {
    const int i = round_->level;
    if (journal_.stages[i] == int(expeditions()[i].tasks.size())) openJournal();
    else openLevel(i);
  }
  void back() {
    if (page_ == Page::Expeditions) {
      page_=journalOrigin_;
      if(page_==Page::Climb && fromTrail_)restoreTrail();
    } else if(page_==Page::Bridge) leaveBridge();
    else openJournal();
  }
  Rect levelButton(int i) const { return {52, 192 + i * 86, 856, 68}; }
  void fieldKey(sf::Keyboard::Key k) {
    if (page_ == Page::Bridge) {
      if (k == sf::Keyboard::Escape) { held_ = -1; back(); }
      else if (span_->complete() && k == sf::Keyboard::Enter) nextSpan();
      return;
    }
    if (k == sf::Keyboard::Escape) { back(); return; }
    if (page_ == Page::Expeditions && k == sf::Keyboard::B) page_ = Page::Badges;
    else if (page_ == Page::Reward && k == sf::Keyboard::Enter) afterTask();
    else if (k >= sf::Keyboard::Num1 && k <= sf::Keyboard::Num4) {
      const int index = k - sf::Keyboard::Num1;
      if (page_ == Page::Expeditions) openLevel(index);

    }
  }
  void fieldClick(int x, int y) {
    if (skip.contains(x,y)) { held_ = -1; back(); return; }
    if (page_ == Page::Bridge) {
      if (span_->complete()) {
        if (action.contains(x,y)) nextSpan();
      } else {
        if(picked_>=0) for(int g=0;g<int(span_->gaps.size());++g){
          if(deckSlot(span_->gaps[g]).contains(x,y)){
            int piece=picked_;picked_=-1;place(piece,g);return;
          }
        }
        for (int i = 0; i < int(span_->rack.size()); ++i) {
          const auto r = rackSlot(i);
          if (onRack(i) && r.contains(x,y)) {
            picked_ = held_ = i;
            pointer_ = {float(x),float(y)};
            offset_ = {x-r.x,y-r.y};
            break;
          }
        }
      }
      return;
    }
    switch (page_) {
      case Page::Expeditions:
        if (action.contains(x,y)) { page_ = Page::Badges; return; }
        for (int i = 0; i < int(expeditions().size()); ++i)
          if (levelButton(i).contains(x,y)) { openLevel(i); return; }
        return;
      case Page::Reward: if (action.contains(x,y)) afterTask(); return;
      default: return;
    }
  }
  void key(sf::Keyboard::Key k) {
    if(!walking() && (k==sf::Keyboard::Down || k==sf::Keyboard::Right ||
                      k==sf::Keyboard::Up || k==sf::Keyboard::Left)) {
      const int count=int(controls().size());
      menuIndex_=(menuIndex_+count+(k==sf::Keyboard::Down || k==sf::Keyboard::Right ? 1:-1))%count;
      focusKeys_=true;return;
    }
    if(!walking() && focusKeys_ && k==sf::Keyboard::Enter) {gamepadButton(0);return;}
    if (page_==Page::Pause) {
      if(k==sf::Keyboard::Escape || k==sf::Keyboard::Enter) page_=previous_;
      return;
    }
    if (walking()) {
      if(page_==Page::Hollow && k==sf::Keyboard::Space){hollow_.dodge();return;}
      if(page_==Page::Hollow && k==sf::Keyboard::Q){hollow_.parry();return;}
      if(page_==Page::Ridge && k==sf::Keyboard::Space){rise_.dodge();return;}
      if(page_==Page::Ridge && k==sf::Keyboard::Q){rise_.parry();return;}
      if(page_==Page::Bay && (life_.inDialogue() || life_.sparring())){
        if(k==sf::Keyboard::Escape)life_.cancel();
        else if(k==sf::Keyboard::E || k==sf::Keyboard::Space || k==sf::Keyboard::Enter)interact();
        return;
      }
      if (k==sf::Keyboard::Escape) { if(page_==Page::Task) back(); else pause(); return; }
      if (k==sf::Keyboard::E || k==sf::Keyboard::Enter || k==sf::Keyboard::Space) interact();
      if (k==sf::Keyboard::J && (page_==Page::Climb || page_==Page::Bay || (page_==Page::Ridge && !rise_.fighting()) || (page_==Page::Hollow && !hollow_.fighting()))) openJournal();
      return;
    }
    if (page_ != Page::Climb) { fieldKey(k); return; }
    if (k==sf::Keyboard::Escape) pause();
    else if (k==sf::Keyboard::J) openJournal();
    
    else if (k==sf::Keyboard::Enter || k==sf::Keyboard::Space) click(homeButton(0).x+8,homeButton(0).y+8);
  }
  void click(int x, int y) {
    if (page_ == Page::Pause) {
      if (pauseResume().contains(x,y)) page_=previous_;
      else if (pauseQuit().contains(x,y)) window_.close();
      else if (pauseSound().contains(x,y)) {sound_=!sound_;trailMusic(sound_);}
      return;
    }
    if(page_==Page::Bay){
      if(life_.inDialogue() || life_.sparring()){interact();return;}
      auto ui=window_.getView();
      auto screen=window_.mapCoordsToPixel({float(x),float(y)},ui);
      auto world=window_.mapPixelToCoords(screen,bayView());
      auto target=life_.nearby({world.x,world.y+8},journal_.stages[0]==3);
      if(target!=BayTarget::None && trail::WillowBay::near(bayPosition_,life_.point(target),20))
        bayAction(target);
      return;
    }
    if (walking()) return;
    if (page_ != Page::Climb) { fieldClick(x,y); return; }
    if (!atCampOrTrail()) return;
    if (homeButton(0).contains(x,y)) enterBay();
    else if (homeButton(1).contains(x,y)) takePass();
    else if (homeButton(2).contains(x,y)) openJournal();
  }

  void rect(Rect r, sf::Color color) {
    sf::RectangleShape box({float(r.width), float(r.height)});
    box.setPosition(float(r.x), float(r.y));
    box.setFillColor(color);
    window_.draw(box);
  }
  void text(const std::string& value, int x, int y, unsigned size, sf::Color color,
            int width = 856, bool bold = false) {
    sf::Text t(sf::String::fromUtf8(value.begin(), value.end()), font_, size);
    if (bold) t.setStyle(sf::Text::Bold);
    while (size > 12 && t.getLocalBounds().width > width) t.setCharacterSize(--size);
    t.setFillColor(color);
    t.setPosition(float(x), float(y));
    window_.draw(t);
  }
  void button(Rect r, const std::string& label, bool primary, const std::string& number = "") {
    auto mouse = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
    const bool hover = r.contains(int(mouse.x), int(mouse.y));
    rect({r.x, r.y + 4, r.width, r.height}, sf::Color(175, 163, 132));
    rect(r, primary ? (hover ? muted : green) : (hover ? gold : sf::Color(233, 222, 193)));
    const int y = r.y + (r.height - 32) / 2;
    if (!number.empty()) {
      rect({r.x + 15, y + 1, 32, 32}, paper);
      text(number, r.x + 24, y + 3, 19, muted);
    }
    const int pad = number.empty() ? 20 : 62;
    text(label, r.x + pad, y - 1, 24, primary ? paper : ink, r.width - pad - 15, true);
  }

  void openSpan(int stage) {
    span_ = campaign_->bridgeSpan(stage,replay_);
    if (!span_) { leaveBridge(); return; }
    held_ = -1; picked_ = -1;
    hint_.clear();
    page_ = Page::Bridge;
  }
  void nextSpan() {
    if (span_->stage+1 == int(expeditions()[0].tasks.size())) leaveBridge();
    else openSpan(span_->stage+1);
  }
  static Rect deckSlot(int index) { return {156+108*index, 243, 104, 94}; }
  static Rect rackSlot(int index) { return {240+188*index, 484, 104, 94}; }
  bool onRack(int piece) const {
    for (int g=0; g<int(span_->gaps.size()); ++g)
      if (span_->filled[g] && span_->rack[piece] == span_->deck[span_->gaps[g]]) return false;
    return true;
  }
  void place(int piece, int gap) {
    if (span_->filled[gap]) return;
    const auto outcome = campaign_->placePlank(*span_,gap,span_->rack[piece],replay_);
    if (outcome == ExpeditionOutcome::Unavailable) { leaveBridge(); return; }
    if (outcome == ExpeditionOutcome::TryAgain) hint_ = "Start at " + std::to_string(span_->deck.front()) + ". " + expeditions()[0].tasks[span_->stage].prompt;
    else hint_.clear();
  }
  void wood(Rect r, int value, bool fresh) {
    const sf::Color edge(104,74,52), grain(175,124,77);
    rect({r.x,r.y+4,r.width,r.height},edge);
    rect(r,fresh ? sf::Color(240,196,121) : sf::Color(206,164,111));
    rect({r.x+4,r.y+3,r.width-8,4},sf::Color(244,209,148));
    rect({r.x+13,r.y+14,23,2},grain);
    rect({r.x+54,r.y+r.height-16,32,2},grain);
    for (int x : {r.x+7,r.right()-12})
      for (int y : {r.y+9,r.bottom()-14}) rect({x,y,5,5},edge);
    const auto digits = std::to_string(value);
    text(digits, r.x+r.width/2-int(digits.size())*10,r.y+21,34,ink,86,true);
  }
  void drawBridge() {
    const bool done = span_->complete();
    const bool final = span_->stage+1 == int(expeditions()[0].tasks.size());
    rect({0,0,960,72},green);
    text("Willow Bridge",48,12,34,paper,500,true);
    text("Span " + std::to_string(span_->stage+1) + " of 3",765,20,21,paper);
    text(expeditions()[0].tasks[span_->stage].prompt,48,85,26,ink,860,true);
    creek_.draw(window_);
    for (int y : {254,312}) {
      rect({108,y,744,12},sf::Color(87,68,52));
      rect({108,y,744,3},sf::Color(178,139,88));
    }
    for (int i=0; i<int(span_->deck.size()); ++i) {
      const Rect r = deckSlot(i);
      auto it = std::find(span_->gaps.begin(),span_->gaps.end(),i);
      const bool gap = it != span_->gaps.end();
      if (gap && !span_->filled[it-span_->gaps.begin()]) {
        const bool over = picked_ >= 0 || (held_ >= 0 && r.contains(int(pointer_.x),int(pointer_.y)));
        rect(r,over ? gold : sf::Color(203,221,181));
        rect({r.x+4,r.y+4,r.width-8,r.height-8},sf::Color(58,100,91));
        text("?",r.x+41,r.y+23,30,paper);
      } else wood(r,span_->deck[i],gap);
    }
    if (done) {
      text(final ? "Bridge Builder" : "Span repaired!",48,452,34,ink,860,true);
      text(final ? "The crossing is ready. Your badge is in the achievement book." :
                   "Both planks fit. The next span is ready to repair.",48,520,23,muted);
      button(action,std::string(final ? (bridgeFromBay_ ? "Walk back to the bay" : "View expeditions") : "Next span")+(padMode_ ? "    [A]" : "    [Enter]"),true);
    } else {
      text(hint_.empty() ? (padMode_ ? "Select a plank with A, then select its gap. B cancels." : "Click a plank, then a gap. You can also drag it into place.") : hint_,48,438,23,green,860,true);
      for (int i=0; i<int(span_->rack.size()); ++i)
        if (held_ != i && picked_ != i && onRack(i)) wood(rackSlot(i),span_->rack[i],true);
    }
    button(skip,std::string(bridgeFromBay_ ? "Back to Willow Bay" : "Back to journal")+(padMode_ ? "    [B]" : "    [Esc]"),false);
    if (picked_>=0 && !span_->complete())
      text("Holding plank "+std::to_string(span_->rack[picked_]),52,610,23,green);
    if (held_ >= 0) {
      auto r = rackSlot(held_);
      r.x = int(pointer_.x)-offset_.x;
      r.y = int(pointer_.y)-offset_.y;
      wood(r,span_->rack[held_],true);
    }
  }

  void drawJournal() {
    rect({0, 0, 960, 78}, green);
    text("KT   /   Trail journal", 52, 18, 30, paper, 480, true);
    text(journal_.rank + "   /   Altitude " + std::to_string(journal_.altitude),
         515, 25, 20, paper, 390);
    if (page_ == Page::Expeditions) {
      text("Special expeditions", 52, 89, 35, ink, 850, true);
      text(journal_.next_rank.empty() ? "Highest trail rank reached" :
           std::to_string(journal_.steps_to_rank) + " more steps to " + journal_.next_rank,
           52, 143, 22, muted);
      for (int i = 0; i < int(expeditions().size()); ++i) {
        const auto& level = expeditions()[i];
        const bool locked = journal_.altitude < level.altitude;
        const int stages = journal_.stages[i];
        const bool done = stages == int(level.tasks.size());
        const std::string status = locked ? "Opens at altitude " + std::to_string(level.altitude) :
            done ? std::string(level.badge) + " earned" :
                   std::to_string(stages) + " / " + std::to_string(level.tasks.size()) + " tasks";
        const Rect row = levelButton(i);
        const bool available = !locked && (!done || i == 0);
        if (available) button(row, level.title, true, std::to_string(i+1));
        else {
          rect(row, sf::Color(233,222,193));
          text(level.title, row.x + 62, row.y + 16, 24, ink, 400, true);
        }
        text(i == 0 && done ? "Replay crossing" : status, 587, row.y + 21, 18, available ? paper : muted, 299);
      }
      button(action, padMode_ ? "Achievement book" : "Achievement book    [B]", true);
    } else if (page_ == Page::Badges) {
      text("Achievement book", 52, 90, 35, ink, 850, true);
      for (int i = 0; i < int(journal_.badges.size()); ++i) {
        const auto& badge = journal_.badges[i];
        const int x = 52 + i % 2 * 437, y = 163 + i / 2 * 103;
        rect({x, y, 418, 90}, badge.earned() ? gold : sf::Color(233,222,193));
        text(badge.title, x+14, y+8, 23, ink, 260, true);
        text(badge.earned() ? "Earned" : std::to_string(badge.count) + " / " +
             std::to_string(badge.required), x+318, y+12, 18, muted, 89);
        text(badge.goal, x+14, y+49, 19, muted, 388);
      }
    } else {
      const auto& level = expeditions()[round_->level];
      text(level.title, 52, 105, 38, ink, 850, true);
      {
        const bool done = journal_.stages[round_->level] == int(level.tasks.size());
        text(done ? std::string("Badge earned: ") + level.badge : "Task complete!",
             52, 224, 34, ink, 850, true);
        text("Saved tasks: " + std::to_string(journal_.stages[round_->level]) + " of " +
             std::to_string(level.tasks.size()), 52, 301, 23, muted);
        button(action, std::string(done ? "View expeditions" : "Next task")+(padMode_ ? "    [A]" : "    [Enter]"), true);
      }
    }
    button(skip, std::string(page_ == Page::Expeditions ? (journalOrigin_==Page::Bay ? "Return to Willow Bay" : "Return to climb") : "Back to journal")+(padMode_ ? "    [B]" : "    [Esc]"), false);
  }

  bool walking() const {
    return page_==Page::Bay || page_==Page::Summit || page_==Page::Hollow || page_==Page::Ridge || page_==Page::Task || (page_==Page::Climb && !atCampOrTrail());
  }
  const Puzzle& activePuzzle() const { return page_==Page::Task ? round_->puzzle : puzzle_; }
  void viewport() {
    const auto sz=window_.getSize();
    if (!sz.x || !sz.y) return;
    const float aspect=float(sz.x)/sz.y;
    const bool wide=page_==Page::Hollow || page_==Page::Summit || page_==Page::Ridge || page_==Page::Bay || page_==Page::Climb || page_==Page::Task;
    uiHeight_=wide ? std::clamp(960.f/aspect,410.f,800.f) : 700.f;
    sf::View ui(sf::FloatRect(0,0,960,uiHeight_));
    const float ratio=(960.f/uiHeight_)/aspect;
    if (ratio<1) ui.setViewport({(1-ratio)/2,0,ratio,1});
    else ui.setViewport({0,(1-1/ratio)/2,1,1/ratio});
    window_.setView(ui);
  }
  void setWindowIcon() {
    sf::Image icon;
    if(icon.loadFromFile("shell/assets/kt-icon.png"))
      window_.setIcon(icon.getSize().x,icon.getSize().y,icon.getPixelsPtr());
  }
  void toggleFullscreen() {
    fullscreen_=!fullscreen_;
    window_.create(fullscreen_ ? sf::VideoMode::getDesktopMode() : windowSize(),"KT",
                   fullscreen_ ? sf::Style::None : sf::Style::Default);
    setWindowIcon();
    held_=-1; keys_.fill(false);
    window_.setKeyRepeatEnabled(false); window_.setFramerateLimit(60);
    viewport();
  }
  void chime(const char* cue) { if(sound_) trailSound(cue); }
  void pause() { focusKeys_=true; previous_=page_; page_=Page::Pause; menuIndex_=0; held_=-1; }
  Rect pauseResume() const {return {285,293,390,64};}
  Rect pauseSound() const {return {285,375,390,64};}
  Rect pauseQuit() const {return {285,457,390,64};}
  Rect homeButton(int i) const { return {54,int(uiHeight_)-225+i*53,338,44}; }
  void drawHome() {
    art_.vista(window_,{960,uiHeight_},runner_.progressionState().altitude>=90);
    rect({32,25,413, int(uiHeight_)-50},sf::Color(31,52,51,226));
    const bool camp=machine_.atSummary();
    const int alt=runner_.progressionState().altitude;
    text("KT",54,32,uiHeight_<490 ? 49 : 66,paper,360,true);
    text(alt>=90 ? "Summit camp" : camp ? "Evening at camp" : "Willow Bay",
         57,uiHeight_<490 ? 91 : 120,31,gold,360,true);
    const auto day=runner_.summary();
    const std::string info=camp ? std::to_string(day.solved)+" steps climbed today" :
      alt==0 ? "Mountain trailhead" : "Continue from altitude "+std::to_string(alt);
    text(info,57,uiHeight_<490 ? 136 : 163,20,paper,360);
    button(homeButton(0),"Walk down to Willow Bay",true);
    button(homeButton(1),"Continue climbing",false);
    button(homeButton(2),"Trail journal",false);
    text(padMode_ ? "Stick / D-pad   Move       A   Select" : "Enter   Willow Bay      J   Journal",54,int(uiHeight_)-59,16,paper);
  }
  void drawPause() {
    art_.vista(window_,{960,700});
    rect({250,162,460,410},sf::Color(33,53,51,242));
    text("KT",286,178,48,paper,350,true);
    text("Rest on the trail",289,245,22,gold);
    button(pauseResume(),"Return to the trail",true);
    button(pauseSound(),sound_ ? "Sound   On" : "Sound   Off",false);
    button(pauseQuit(),"Quit to desktop",false);
  }
  void startMap(const Puzzle& p) {
    visited_.clear();
    map_=MountainTrail::generate(p);
    if(page_==Page::Task && round_->level==1) map_.activity=TrailActivity::Supplies;
    art_.generate(map_,runner_.progressionState().zone_id);
    position_=map_.start; carrying_=-1; ropeOpen_=false; moving_=false;
    direction_=0; walkTime_=0; flashTime_=0;
  }
  void tell(std::string message,float seconds=3.f) {
    flash_=std::move(message); flashTime_=seconds;
  }
  void resolveMarker(int i) {
    const bool wasField=page_==Page::Task;
    if(wasField) {
      fieldAnswer(i);
      if(page_==Page::Reward) chime("step");
      else {carrying_=-1; tell(hint_);}
      return;
    }
    answer(i);
    if(result_.correct) {
      ropeOpen_=true; carrying_=-1; chime("step");
      tell(notice_.empty() ? "Trail found! Take the rope up the ledge." : notice_,3.2f);
    } else {
      const auto oldPos=position_;
      if(machine_.scene()==Scene::RetryPrompt) advance(true);
      else advance();
      if(machine_.atSummary()) chime("camp");
      else {position_={oldPos.x,std::max(185.f,oldPos.y)};tell("Try this trail. Your altitude is safe.");}
    }
  }
  using BayTarget=trail::BayLife::Target;
  void bayAction(BayTarget t){
    if(t==BayTarget::Bridge || t==BayTarget::WestBank)openLevel(0);
    else if(t==BayTarget::Pass && owlCleared_)takePass();
    else if(t==BayTarget::Pass && store_->hasMilestone(runner_.childId(),"mountain.bracken.copperhorn"))enterHollow();
    else if(t==BayTarget::Pass || t==BayTarget::Ridge)enterRise(t==BayTarget::Ridge);
    else if(t==BayTarget::Lodge)openJournal();
    else life_.interact(t);
  }
  void interact() {
    if(page_==Page::Bay) {
      if(life_.inDialogue()){
        life_.confirm();
        if(life_.sparring())direction_=2;
      }
      else if(life_.sparring()){
        if(life_.ready())chime("pickup");
        life_.swing();
      }
      else bayAction(life_.nearby(bayPosition_,journal_.stages[0]==3));
      return;
    }
    if(page_==Page::Summit){
      if(credits_){enterBay();bayPosition_=trail::WillowBay::plaza;}
      else if(peakProgress_>=5.85f){
        store_->awardMilestone(runner_.childId(),"mountain.summit.homecoming");
        homecoming_=credits_=true;chime("camp");
      }
      return;
    }
    if(page_==Page::Hollow){
      if(hollow_.atExit())takePass();
      else if(hollow_.atEntry())enterRise(ridgeFromBridge_,true);
      else {hollow_.use();saveOwl();}
      return;
    }
    if(page_==Page::Ridge){
      if(rise_.fighting()){
        rise_.strike();
        if(rise_.victory){
          store_->awardMilestone(runner_.childId(),"mountain.bracken.copperhorn");
          rise_.victory=false;chime("camp");
        }
      }else if(rise_.cleared() && rise_.feet.y<51)enterHollow();
      else if(rise_.feet.y>397){
        enterBay();
        if(ridgeFromBridge_)bayPosition_={91,164};
      }
      return;
    }
    if(page_==Page::Climb && atBayReturn()) {enterHollow(true);return;}
    if(!walking() || ropeOpen_) return;
    if(map_.activity==TrailActivity::Supplies && carrying_>=0 && map_.near(position_,map_.camp,31)) {
      resolveMarker(carrying_); return;
    }
    const int i=map_.nearest(position_);
    if(i>=0) {
      const auto& route=activePuzzle().route;
      if(!route.empty()) {
        if(std::find(visited_.begin(),visited_.end(),i)!=visited_.end()) return;
        visited_.push_back(i);
        if(i!=route[visited_.size()-1] || visited_.size()==route.size()) resolveMarker(i);
        else {chime("pickup");tell("Anchor secured. Follow the next blaze.",1.4f);}
        return;
      }
      if(map_.activity==TrailActivity::Supplies) {carrying_=i;chime("pickup");tell("Supplies packed. Return to the camp crate.");}
      else resolveMarker(i);
    } else if(page_==Page::Climb && map_.near(position_,{map_.start.x-61,444},31)) {
      openJournal();
    }
  }
  static int connectedPad() {
    for(unsigned i=0;i<sf::Joystick::Count;++i) if(sf::Joystick::isConnected(i)) return int(i);
    return -1;
  }
  sf::Vector2f stick() const {
    if(pad_<0) return {};
    auto axis=[&](sf::Joystick::Axis a) {
      if(!sf::Joystick::hasAxis(pad_,a)) return 0.f;
      float v=sf::Joystick::getAxisPosition(pad_,a)/100.f;
      return std::abs(v)<.22f ? 0.f : v;
    };
    float x=axis(sf::Joystick::PovX), y=-axis(sf::Joystick::PovY);
    return {x==0 ? axis(sf::Joystick::X) : x,y==0 ? axis(sf::Joystick::Y) : y};
  }
  std::vector<Rect> controls() const {
    std::vector<Rect> out;
    if(page_==Page::Pause) return {pauseResume(),pauseSound(),pauseQuit()};
    if(page_==Page::Climb && atCampOrTrail()) return {homeButton(0),homeButton(1),homeButton(2)};
    if(page_==Page::Expeditions) {
      for(int i=0;i<int(expeditions().size());++i)
        if(journal_.altitude>=expeditions()[i].altitude && (i==0 || journal_.stages[i]<int(expeditions()[i].tasks.size())))out.push_back(levelButton(i));
      out.push_back(action);
    }
    if(page_==Page::Reward)out.push_back(action);
    if(page_==Page::Bridge) {
      if(span_->complete()) out.push_back(action);
      else if(picked_==-1) {
        for(int i=0;i<3;++i)if(onRack(i))out.push_back(rackSlot(i));
      } else {
        for(int g=0;g<2;++g)if(!span_->filled[g])out.push_back(deckSlot(span_->gaps[g]));
      }
    }
    out.push_back(skip);return out;
  }
  void showFocus() {
    const auto list=controls();
    menuIndex_=std::clamp(menuIndex_,0,int(list.size())-1);
    auto r=list[menuIndex_];
    sf::RectangleShape line({float(r.width+6),float(r.height+6)});
    line.setPosition(float(r.x-3),float(r.y-3)); line.setFillColor(sf::Color::Transparent);
    line.setOutlineColor(sf::Color(250,197,92));line.setOutlineThickness(3);window_.draw(line);
  }
  void gamepadButton(unsigned b) {
    if(page_==Page::Hollow){
      if(b==1){hollow_.dodge();return;}
      if(b==2){hollow_.parry();return;}
    }
    if(page_==Page::Ridge){
      if(b==1){rise_.dodge();return;}
      if(b==2){rise_.parry();return;}
    }
    if(b==7) { if(page_==Page::Pause)page_=previous_; else pause(); return; }
    if(page_==Page::Bay && (life_.inDialogue() || life_.sparring())){
      if(b==1)life_.cancel();
      if(b==0)interact();
      return;
    }
    if(b==1) {
      if(page_==Page::Pause)page_=previous_;
      else if(page_==Page::Bridge && picked_>=0)picked_=-1;
      else if(page_==Page::Climb || page_==Page::Bay || page_==Page::Summit)pause();
      else back();
      menuIndex_=0;return;
    }
    if(b==3 && (page_==Page::Climb || page_==Page::Bay || (page_==Page::Ridge && !rise_.fighting()) || (page_==Page::Hollow && !hollow_.fighting()))) {openJournal();menuIndex_=0;return;}
    if(b!=0)return;
    if(walking()){interact();return;}
    const auto list=controls();
    auto r=list[std::clamp(menuIndex_,0,int(list.size())-1)];
    if(page_==Page::Bridge && !span_->complete() && r.y!=skip.y) {
      if(picked_<0){for(int i=0;i<3;++i)if(r.x==rackSlot(i).x)picked_=i;}
      else {for(int g=0;g<2;++g)if(r.x==deckSlot(span_->gaps[g]).x)place(picked_,g);picked_=-1;}
    } else click(r.x+r.width/2,r.y+r.height/2);
    menuIndex_=0;
  }
  void tick(float dt) {
    if(!window_.hasFocus())return;
    animationTime_+=dt;
    if(page_==Page::Climb && machine_.canAnswer() && !ropeOpen_)
      encounterTime_+=sf::seconds(dt);
    if (musicClock_.getElapsedTime().asSeconds()>1) { musicClock_.restart(); if(sound_)trailMusic(true); }
    if(page_==Page::Bay){
      life_.update(dt,bayPosition_);
      float targetZoom=life_.sparring()?1.f:0.f;
      bayZoom_+=std::clamp(targetZoom-bayZoom_,-dt*2.8f,dt*2.8f);
      if(life_.inDialogue() || life_.sparring()){moving_=false;return;}
    }
    auto v=stick();
    if(!walking()){
      const float nav=std::abs(v.x)>std::abs(v.y) ? v.x : v.y;
      if(std::abs(nav)>.5f && !padLatch_){
        auto count=int(controls().size());menuIndex_=(menuIndex_+count+(nav>0?1:-1))%count;
        padMode_=true;padLatch_=true;
      } else if(std::abs(nav)<.3f)padLatch_=false;
      return;
    }
    if(v.x!=0 || v.y!=0)padMode_=true;
    auto down=[&](sf::Keyboard::Key a,sf::Keyboard::Key b){return keys_[a] || keys_[b];};
    v.x+=int(down(sf::Keyboard::D,sf::Keyboard::Right))-int(down(sf::Keyboard::A,sf::Keyboard::Left));
    v.y+=int(down(sf::Keyboard::S,sf::Keyboard::Down))-int(down(sf::Keyboard::W,sf::Keyboard::Up));
    float length=std::hypot(v.x,v.y);moving_=length>.05f;
    if(length>1){v.x/=length;v.y/=length;}
    if(page_==Page::Summit){
      if(!credits_){
        float along=std::abs(v.x)>std::abs(v.y)?v.x:-v.y;
        peakProgress_=std::clamp(peakProgress_+dt*along*1.85f,0.f,6.f);
        if(moving_){direction_=along>=0?2:3;walkTime_+=dt;}
      }
      return;
    }
    if(page_==Page::Ridge || page_==Page::Hollow){
      if(page_==Page::Ridge)rise_.update(dt,{v.x,v.y});
      else {hollow_.update(dt,{v.x,v.y});saveOwl();}
      auto f=page_==Page::Ridge?rise_.facing:hollow_.facing;
      direction_=std::abs(f.x)>std::abs(f.y)?(f.x>0?2:3):(f.y>0?1:0);
      if(moving_)walkTime_+=dt;
      return;
    }
    if(moving_) {
      direction_=std::abs(v.x)>std::abs(v.y) ? (v.x>0?2:3) : (v.y>0?1:0);
      if(page_==Page::Bay){
        auto next=bay_.move(bayPosition_,v.x*81*dt,v.y*81*dt,journal_.stages[0]==3);
        bool blocked=std::any_of(life_.folk.begin(),life_.folk.end(),[&](const auto& r){
          return trail::WillowBay::near(next,r.feet,7) &&
            std::hypot(next.x-r.feet.x,next.y-r.feet.y)<std::hypot(bayPosition_.x-r.feet.x,bayPosition_.y-r.feet.y);
        });
        if(!blocked)bayPosition_=next;
      }
      else position_=map_.move(position_,v.x*99*dt,v.y*99*dt,ropeOpen_);
      walkTime_+=dt;
    }
    flashTime_=std::max(0.f,flashTime_-dt);
    if(page_==Page::Bay)return;
    if(ropeOpen_ && position_.y<=map_.rope.y+1){
      bool summit=result_.altitude==90;
      if(summit)enterSummit();
      else advance();
      if(summit || machine_.atSummary())chime("camp");
    }
  }
  bool atBayReturn() const {
    return page_==Page::Climb && map_.near(position_,{map_.start.x+26,454},19);
  }
  sf::View bayView() const {
    const float width=410.f-160.f*bayZoom_,height=uiHeight_*width/960.f;
    const float left=std::clamp(bayPosition_.x+10*bayZoom_-width/2,0.f,512-width);
    const float top=std::clamp(bayPosition_.y-height*.56f,0.f,344-height);
    sf::View view(sf::FloatRect(std::floor(left),std::floor(top),width,height));
    view.setViewport(window_.getView().getViewport());return view;
  }
  void drawBay() {
    const auto ui=window_.getView();
    window_.setView(bayView());
    bool repaired=journal_.stages[0]==3;
    auto goal=life_.objective(repaired),near=life_.nearby(bayPosition_,journal_.stages[0]==3);
    if(owlCleared_ && goal==BayTarget::Ridge)goal=BayTarget::Pass;
    bool guide=!life_.inDialogue() && !life_.sparring();
    bay_.draw(window_,journal_.stages[0],animationTime_);
    life_.drawProps(window_,animationTime_);
    if(homecoming_){
      for(int pole:{266,302})rect({pole,197,2,17},ink);
      for(int i=0;i<=36;++i){
        int y=194+int(4*std::sin(i*3.14159f/36));
        rect({266+i,y,1,1},ink);
        if(i%7<5)rect({266+i,y+1,1,4-std::abs(i%7-2)},i%14<7?gold:sf::Color(214,122,118));
      }
    }
    if(guide)bay_.guide(window_,bayPosition_,life_.point(goal),repaired,animationTime_);
    auto feet=[&](int i){
      if(i==3)return bayPosition_;
      auto p=life_.folk[i].feet;
      if(i==2){
        // Kit steps aside to give Flick a clear line to the player's guard.
        p.x+=(bayPosition_.x-23-p.x)*bayZoom_;
        p.y+=(bayPosition_.y-10-p.y)*bayZoom_;
      }
      return p;
    };
    std::array<int,4> order{0,1,2,3};
    std::stable_sort(order.begin(),order.end(),[&](int a,int b){return feet(a).y<feet(b).y;});
    for(int i:order){
      if(i==3)art_.hiker(window_,bayPosition_,direction_,moving_?int(walkTime_*8)%4:0);
      else life_.drawPerson(window_,i,feet(i),i==2 && life_.sparring());
      bay_.drawForeground(window_,feet(i));
    }
    if(guide)trail::WillowBay::beacon(window_,life_.point(goal),animationTime_,goal==BayTarget::Sol?32:19);
    if(life_.sparring())life_.drawMoth(window_,bayPosition_,animationTime_);
    window_.setView(ui);
    rect({16,13,328,57},sf::Color(34,62,63,234));
    text(homecoming_?"KT  /  Willow Bay  /  Summit Flag":"KT  /  Willow Bay",28,15,18,paper,290,true);
    text(life_.sparring()?"Wind moth practice":std::string("Follow gold arrows  /  ")+life_.label(goal),28,41,16,gold,298);
    const std::string use=padMode_?"A":"E";
    int bottom=int(uiHeight_);
    if(life_.inDialogue()){
      rect({22,bottom-105,916,89},sf::Color(42,58,65,248));
      rect({22,bottom-105,5,89},gold);
      life_.portrait(window_,36,bottom-88);
      text(life_.speaker,96,bottom-101,19,gold,500,true);
      text(life_.line,96,bottom-74,21,paper,821);
      text(use+(life_.offer()?"   Spar":"   Continue")+(life_.offer()?(padMode_?"      B   Leave":"      Esc   Leave"):""),96,bottom-45,15,gold);
      return;
    }
    std::string tip;
    if(life_.sparring())tip=life_.sparText()+"      "+use+" / Click   Parry     "+(padMode_?"B":"Esc")+"   Leave";
    else if(near!=BayTarget::None)tip=use+"   "+life_.label(near);
    else tip=(padMode_?"Stick / D-pad":"WASD / arrows")+std::string("   Walk      Follow the gold trail");
    rect({0,bottom-38,960,38},sf::Color(34,62,63,232));
    text(tip,22,bottom-32,18,paper,915);
  }
  void drawSummit(){
    const auto ui=window_.getView();
    float w=std::min(320.f,180.f*960/uiHeight_),h=w*uiHeight_/960;
    sf::View scene(sf::FloatRect((320-w)*.5f,180-h,w,h));scene.setViewport(ui.getViewport());window_.setView(scene);
    window_.draw(sf::Sprite(peakArt_));
    static constexpr std::array<TrailPoint,7> ridge{{{75,174},{95,153},{119,155},{153,129},{191,131},{214,114},{237,95}}};
    int i=std::min(5,int(peakProgress_));float f=peakProgress_-i;
    TrailPoint p{ridge[i].x+(ridge[i+1].x-ridge[i].x)*f,ridge[i].y+(ridge[i+1].y-ridge[i].y)*f};
    rect({241,72,2,24},ink);
    if(credits_ || homecoming_){
      rect({243,72,10,6},sf::Color(218,109,89));rect({253,73+int(animationTime_*3)%2,4,4},gold);
    }else trail::WillowBay::beacon(window_,{242,72},animationTime_,6);
    art_.hiker(window_,p,direction_,moving_ && !credits_?int(walkTime_*8)%4:0);
    window_.setView(ui);
    if(credits_){
      rect({32,28,467,175},sf::Color(38,52,68,240));
      text("KT   /   Journey complete",52,39,30,gold,428,true);
      text("From Willow Bridge to the summit",52,90,23,paper,419);
      text("Far below, the lanterns of Willow Bay answer your flag.",52,132,17,paper,419);
      text("Thank you for climbing.     KT team  /  SDP 100",52,166,16,gold,419);
    }else {rect({16,14,222,42},sf::Color(38,52,68,230));text("KT   /   Summit ridge",27,18,21,paper,202,true);}
    rect({0,int(uiHeight_)-40,960,40},ink);
    std::string use=padMode_?"A":"E";
    text(credits_?use+"   Return home to Willow Bay":peakProgress_>=5.85f?use+"   Raise the summit flag":"Follow the snowy ridge up and right to the flagpole.",23,int(uiHeight_)-34,20,paper,912);
  }
  void drawRise() {
    bool hollow=page_==Page::Hollow;
    auto feet=hollow?hollow_.feet:rise_.feet;
    auto ui=window_.getView();
    float width=320,height=uiHeight_*(width/960.f);
    float left=std::clamp(feet.x-width*.5f,0.f,384-width);
    float top=std::clamp(feet.y-height*.57f,0.f,(hollow?420.f:448.f)-height);
    sf::View view(sf::FloatRect(std::floor(left),std::floor(top),width,height));
    view.setViewport(ui.getViewport());window_.setView(view);
    if(hollow)hollow_.draw(window_,art_,direction_,moving_?int(walkTime_*8)%4:0);
    else rise_.draw(window_,art_,direction_,moving_?int(walkTime_*8)%4:0);
    window_.setView(ui);
    rect({14,12,610,62},sf::Color(38,60,62,237));
    text(hollow?"KT   /   Glasswind Hollow":"KT   /   Bracken Rise",26,15,21,paper,355,true);
    text(hollow?hollow_.hint():rise_.objective(),26,45,17,gold,586);
    if(feet.y>309 && feet.y<398){
      for(int side:{-1,1}){
        int x=int((192+58*side-left)*3),y=int((333-top)*3)-30;
        if(y<86 || y>uiHeight_-72)continue;
        rect({x-65,y,130,24},sf::Color(39,61,63,238));
        text(hollow?(side<0?"Mirror Pools":"Bell Gallery"):(side<0?"Fern Switchback":"Cairn Steps"),x-59,y+1,15,side<0?sf::Color(161,229,195):gold,120,true);
      }
    }
    std::string tip;
    if(hollow?hollow_.fighting():rise_.fighting()){
      rect({637,12,309,62},sf::Color(58,52,64,239));
      text("Hiker",650,16,16,paper,87,true);text(hollow?"Moonkite":"Copperhorn",778,16,16,paper,143,true);
      for(int i=0;i<(hollow?3:4);++i)rect({651+i*20,46,15,8},i<(hollow?hollow_.hearts:rise_.hearts)?sf::Color(122,211,166):muted);
      for(int i=0;i<3;++i)rect({779+i*33,46,27,8},i<(hollow?hollow_.guard:rise_.guard)?sf::Color(237,154,104):muted);
      tip=padMode_?"Stick / D-pad   Move       B   Dodge       X   Parry       A   Strike":"WASD / arrows   Move       Space   Dodge       Q   Parry       E   Strike";
    }else if(hollow){
      std::string use=padMode_?"A":"E";
      tip=hollow_.atExit()?use+"   Continue to the upper mountain":hollow_.atEntry()?use+"   Descend to Bracken Rise":use+"   Inspect a pool or bell       Follow the lanterns uphill";
    }else if(rise_.cleared() && feet.y<51)tip=padMode_?"A   Enter Glasswind Hollow":"E   Enter Glasswind Hollow";
    else if(feet.y>397)tip=padMode_?"Walk uphill to the fork       A   Return to Willow Bay":"Walk uphill to the fork       E   Return to Willow Bay";
    else tip=padMode_?"Stick / D-pad   Walk       B   Dodge       Start   Pause":"WASD / arrows   Walk       Space   Dodge       Esc   Pause";
    rect({0,int(uiHeight_)-37,960,37},sf::Color(34,62,63,236));text(tip,20,int(uiHeight_)-32,18,paper,920);
  }
  void drawWorld() {
    auto ui=window_.getView();
    const float height=uiHeight_/2;
    float top=std::clamp(position_.y-height*.60f,0.f,500.f-height);
    sf::View camera(sf::FloatRect(0,std::floor(top),480,height)); camera.setViewport(ui.getViewport());
    window_.setView(camera);art_.environment(window_);
    // A return sign stands at the downhill end of every main trail.
    if(page_==Page::Climb) {
      int x=int(map_.start.x)+26;
      rect({x,445,3,11},sf::Color(128,86,58));rect({x-5,441,14,6},gold);
    }
    const auto& p=activePuzzle();
    // Animated water, pollen and the lodge hearth share the world camera.
    for(int i=0;i<18;++i){
      int x=(i*73+int(animationTime_*10))%480;
      if(std::abs(x-map_.crossings[0])<16 || std::abs(x-map_.crossings[1])<16)continue;
      rect({x,map_.creekY(float(x))+(i%9)-4,4+i%5,1},sf::Color(176,205,181));
    }
    for(int i=0;i<3;++i){
      float t=std::fmod(animationTime_*.35f+i/3.f,1.f);
      rect({int(map_.start.x-47+3*std::sin(t*5)),int(399-18*t),3+i,3},sf::Color(227,219,192,120));
    }
    const bool supply=map_.activity==TrailActivity::Supplies;
    if(!p.route.empty() && !visited_.empty()){
      sf::VertexArray line(sf::LineStrip);
      for(int i:visited_){auto m=map_.markers[i];line.append({{m.x,m.y-7},gold});}
      line.append({{position_.x,position_.y-10},gold});window_.draw(line);
    }
    for(int i=0;i<int(map_.markers.size());++i){
      auto m=map_.markers[i];int x=int(m.x),y=int(m.y);
      if(!p.route.empty()){
        const bool tied=std::find(visited_.begin(),visited_.end(),i)!=visited_.end();
        rect({x-11,y-3,22,6},sf::Color(103,105,86));
        rect({x-7,y-8,14,7},sf::Color(153,150,120));
        rect({x-2,y-28,3,24},sf::Color(105,77,52));
        const auto& symbol=p.options[i].text;
        const sf::Color cloth=symbol=="Sun" ? gold : symbol=="Moon" ? sf::Color(154,170,210) :
            symbol=="Bloom" ? sf::Color(217,139,136) : sf::Color(103,156,116);
        rect({x+1,y-28,13,9},cloth);rect({x+1,y-19,7,2},cloth);
        if(tied){rect({x-4,y-11,8,3},gold);rect({x-4,y-7,8,2},gold);}
        continue;
      }
      if(supply && carrying_==i)continue;
      rect({x-3,y-4,6,10},sf::Color(92,69,49));
      rect({x-15,y-15,30,14},supply ? sf::Color(181,113,63) : sf::Color(103,77,48));
      rect({x-14,y-14,28,3},gold);
      if(supply)rect({x-2,y-15,4,14},paper);
      else {rect({x-9,y-10,17,2},gold);rect({x+5,y-12,2,6},gold);}
    }
    if(supply){
      int x=int(map_.camp.x),y=int(map_.camp.y);
      rect({x-12,y-10,24,15},sf::Color(112,73,48));rect({x-11,y-10,22,4},gold);
      rect({x-2,y-8,4,11},sf::Color(209,156,94));
    }
    if(runner_.progressionState().altitude>=60){
      for(int i=0;i<33;++i){
        int x=(i*67+int(animationTime_*8))%480;
        int y=(i*41+int(animationTime_*18))%500;
        rect({x,y,2,1},sf::Color(249,247,223,190));
      }
    }
    // Guide traversal while leaving the puzzle's route and supply choices to the player.
    bool beforeCreek=position_.y>map_.creekY(position_.x)+8;
    if(ropeOpen_ || (supply && carrying_>=0) || beforeCreek){
      TrailPoint goal=map_.rope;
      if(!ropeOpen_ && supply && carrying_>=0)goal=map_.camp;
      else if(!ropeOpen_){
        int x=std::abs(position_.x-map_.crossings[0])<std::abs(position_.x-map_.crossings[1])
          ? map_.crossings[0]:map_.crossings[1];
        goal={float(x),float(map_.creekY(float(x)))};
      }
      if(goal.y>=top+54 && goal.y<=top+height-25)
        trail::WillowBay::beacon(window_,goal,animationTime_,14);
      else {
        float dx=goal.x-position_.x,dy=goal.y-position_.y,length=std::hypot(dx,dy);
        if(length>0){
          dx/=length;dy/=length;
          for(int n=-3;n<=3;++n){
            int x=int(position_.x+dx*(26-std::abs(n))-dy*n);
            int y=int(position_.y+dy*(26-std::abs(n))+dx*n);
            rect({x,y+1,2,2},ink);rect({x,y,2,1},gold);
          }
        }
      }
    }
    art_.hiker(window_,position_,direction_, moving_ ? int(walkTime_*8)%4 : 0);
    if(carrying_>=0)rect({int(position_.x)-4,int(position_.y)-29,8,6},gold);
    window_.setView(ui);
    for(int i=0;i<int(map_.markers.size());++i){
      if(supply && i==carrying_)continue;
      auto m=map_.markers[i];int x=int(m.x*2),y=int((m.y-std::floor(top))*2)-61;

      int w=std::clamp(20+int(p.options[i].text.size())*10,46,162);
      if(!p.route.empty())y-=22;
      if(y<76 || y>uiHeight_-70)continue;
      rect({x-w/2,y,w,27},sf::Color(51,63,52,245));
      text(p.options[i].text,x-w/2+6,y+2,17,paper,w-12,true);
    }
    const int alt=runner_.progressionState().altitude;
    std::string name=page_==Page::Task ? expeditions()[round_->level].title :
      zone() ? strings_.get(zone()->display_key,zone()->key) : "Willow Bay";
    rect({0,0,960,77},sf::Color(34,54,50,236));
    text("KT   /   "+name+"     "+std::to_string(alt)+(alt==1 ? " step" : " steps"),25,5,18,gold,600,true);
    const bool atCreek=position_.y>map_.creekY(position_.x)+8 && carrying_<0 && !ropeOpen_;
    const std::string objective=ropeOpen_ ? "Find the rope and climb north." :
        atCreek ? "Cross the creek at either wooden footbridge." : p.prompt;
    std::string order;
    if(!p.route.empty() && !ropeOpen_ && !atCreek) {
      for (std::size_t n=visited_.size();n<p.route.size();++n) {
        if(!order.empty())order+="  /  ";
        order+=p.options[p.route[n]].text;
      }
    }
    text(order.empty() ? objective : "Rig the line:   "+order,25,32,26,paper,910,true);
    std::string use=padMode_ ? "A" : "E";
    std::string tip;
    if(flashTime_>0)tip=flash_;
    else if(atBayReturn())tip=use+"   Descend to Glasswind Hollow";
    else if(ropeOpen_)tip="Follow the paths uphill to the rope. Move up to climb.";
    else if(supply && carrying_>=0)tip="Carry "+p.options[carrying_].text+" to the camp crate. Press "+use+" to deliver.";
    else if(atCreek) tip=padMode_ ? "Left stick / D-pad   Walk      Start   Pause" : "WASD / arrows   Walk      Esc   Pause";
    else if(!p.route.empty() && map_.nearest(position_)>=0)tip=use+"   Tie the safety line to this anchor";
    else if(map_.nearest(position_)>=0)tip=use+(supply ? "   Pick up this supply bundle" : map_.activity==TrailActivity::Survey ? "   Inspect this field marker" : "   Take this route");
    else if(page_==Page::Climb && map_.near(position_,{map_.start.x-61,444},31))tip=use+"   Open the lodge journal";
    else tip=(padMode_ ? "Stick / D-pad" : "WASD / arrows")+std::string("   Move north to the ")+(supply ? "supply bundles" : "trail markers")+"     "+use+"   Interact";
    rect({0,int(uiHeight_)-43,960,43},sf::Color(38,55,48,236));
    text(tip,24,int(uiHeight_)-36,20,paper,910);
  }

  void draw() {
    window_.clear(paper);
    if (page_ == Page::Pause) { drawPause(); return; }
    if (page_==Page::Summit) {drawSummit();return;}
    if (page_==Page::Ridge || page_==Page::Hollow) {drawRise();return;}
    if (page_==Page::Bay) {drawBay();return;}
    if (walking()) { drawWorld(); return; }
    if (page_ == Page::Bridge) { drawBridge(); return; }
    if (page_ != Page::Climb) { drawJournal(); return; }
    drawHome();
  }

  sf::RenderWindow window_;
  trail::MountainArt art_;
  trail::WillowCreek creek_;
  std::optional<BridgeSpan> span_;
  bool replay_ = false;
  int held_ = -1;
  sf::Vector2f pointer_;
  sf::Vector2i offset_;
  sf::Font font_;
  sf::Time encounterTime_;
  std::unique_ptr<IDataStore> store_;
  LevelGenerator generator_;
  Curriculum curriculum_;
  StringTable strings_;
  AdaptiveEngine engine_;
  ProgressionService progression_;
  SessionRunner runner_;
  SceneMachine machine_;
  Puzzle puzzle_;
  SessionResult result_;
  trail::WillowBay bay_;
  trail::BayLife life_;
  trail::BrackenRise rise_;
  trail::Glasswind hollow_;
  sf::Texture peakArt_;
  float peakProgress_=0;
  bool owlCleared_=false,homecoming_=false,credits_=false;
  bool ridgeFromBridge_=false;
  TrailPoint bayPosition_=trail::WillowBay::plaza;
  Page page_=Page::Bay, journalOrigin_=Page::Bay;
  bool bridgeFromBay_=false;
  std::unique_ptr<TrailCampaign> campaign_;
  TrailJournal journal_;
  std::optional<ExpeditionRound> round_;
  std::string hint_, notice_;
  std::array<bool,sf::Keyboard::KeyCount> keys_{};
  MountainTrail map_;
  std::vector<int> visited_, savedVisited_;
  TrailPoint position_, savedPos_;
  int carrying_=-1, savedCarry_=-1, direction_=1, pad_=-1, menuIndex_=0, picked_=-1;
  float walkTime_=0, flashTime_=0, bayZoom_=0;
  bool ropeOpen_=false, savedClimb_=false, fromTrail_=false, moving_=false, fullscreen_=true;
  bool focusKeys_=false, padLatch_=false, padMode_=false, sound_=true;
  Page previous_=Page::Climb;
  sf::Clock frameClock_, musicClock_;
  float animationTime_=0;
  std::string flash_;
  float uiHeight_=540.f;
};
}  // namespace

int main(int argc, char** argv) {
  try {
    std::string db="sdp100.db";
    bool fullscreen=true;
    for (int i=1; i<argc; ++i) {
      if (std::string(argv[i])=="--db" && i+1<argc) db=argv[++i];
      else if (std::string(argv[i])=="--windowed") fullscreen=false;
    }
    if(db!="sdp100.db") db=std::filesystem::absolute(db).string();
    const auto exeDir=std::filesystem::absolute(argv[0]).parent_path();
    for(const auto& base:{exeDir,exeDir.parent_path().parent_path()}) {
      if(std::filesystem::exists(base/"shell/assets/climber.png") && std::filesystem::is_directory(base/"content")) {
        std::filesystem::current_path(base); break;
      }
    }
    Shell shell(db, fullscreen);
    shell.run();
  } catch (const std::exception& e) {
    std::cerr << "KT: " << e.what() << '\n';
    return 1;
  }
}
