/*
                                    88888888
                                  888888888888
                                 88888888888888
                                8888888888888888
                               888888888888888888
                              888888  8888  888888
                              88888    88    88888
                              888888  8888  888888
                              88888888888888888888
                              88888888888888888888
                             8888888888888888888888
                          8888888888888888888888888888
                        88888888888888888888888888888888
                              88888888888888888888
                            888888888888888888888888
                           888888  8888888888  888888
                           888     8888  8888     888
                                   888    888

                                   OCTOBANANA

Licensed under the MIT License

Copyright (c) 2020 Brett Robinson <https://octobanana.com/>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef APP_HH
#define APP_HH

#include "app/util.hh"
#include "app/window.hh"

#include "ob/parg.hh"
#include "ob/ring.hh"
#include "ob/text.hh"
#include "ob/term.hh"
#include "ob/timer.hh"
#include "ob/prism.hh"
#include "ob/readline.hh"
#include "ob/belle/belle.hh"

#include <boost/asio.hpp>

#include <cmath>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <array>
#include <mutex>
#include <tuple>
#include <deque>
#include <regex>
#include <atomic>
#include <chrono>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <iomanip>
#include <complex>
#include <utility>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <unordered_map>

using Read = OB::Belle::IO::Read;
using Key = OB::Belle::IO::Read::Key;
using Mouse = OB::Belle::IO::Read::Mouse;
using Tick = std::chrono::nanoseconds;
using Clock = std::chrono::steady_clock;
using Timer = OB::Belle::asio::steady_timer;
using namespace std::chrono_literals;
using namespace std::string_literals;
namespace Text = OB::Text;
namespace Term = OB::Term;
namespace Belle = OB::Belle;
namespace iom = OB::Term::iomanip;
namespace aec = OB::Term::ANSI_Escape_Codes;
namespace fs = std::filesystem;
namespace asio = boost::asio;

class App {
public:
  App(OB::Parg const& pg);
  ~App();

  void run();

private:
  using Position = Vec2n<double>;

  struct Object {
    Size size {0, 0};
    Position position {0, 0};
  };

  struct Box : public Object {
    double velocity {0.0};
  };

  struct Goal {
    using Sprites = std::vector<Object>;
    Sprites sprites;

    using Colliders = std::vector<Object>;
    Colliders colliders;

    Object pass;

    double velocity {0.0};

    struct State {
      enum {
        Null = 0,
        Pass,
        Miss,
      };
    };
    int state {State::Null};
  };
  using Goals = std::vector<Goal>;

  struct Event {
    char32_t type {0};
    std::size_t frame {0};
  };
  using Events = std::deque<Event>;

  struct State {
    unsigned int seed {std::random_device{}()};
    double speed {-16.0};
    double gravity {-80.0};
    double impulse {20.0};
    double box_offset {0.25};
    Size box_size {2, 1};
  };
  State _state;

  struct Record {
    State state;
    Events events;
  };

  Record _record;
  Record _record_best;

  OB::Parg const& _pg;

  void quit();
  void screen_init();
  void screen_deinit();
  void await_signal();
  void await_tick();
  void on_winch();
  void on_pause();
  void on_continue();
  void readline_init();
  void keymap_init();
  void game_init();
  void await_read();
  bool on_read(Read::Null& ctx);
  bool on_read(Read::Mouse& ctx);
  bool on_read(Read::Key& ctx);
  bool input_button(Read::Mouse const& ctx);
  void input_code(Read::Key const& ctx);
  bool input_prompt(Read::Key const& ctx);
  bool input_default(Read::Key const& ctx);
  bool input_map(char32_t const ch);

  void update(double const dt);
  void input();
  void distance(double const dt);
  void ai(double const dt);
  void movement(double const dt);
  void move_trail();
  void move_box(double const dt);
  void move_goals(double const dt);
  void detect_collision();
  void add_goal(double const x, std::size_t const y);
  void increase_velocity();

  void render();
  void draw();
  void draw_vertical(Object const& obj, std::function<void(Style&)> const& fn = {});
  void draw_horizontal(Object const& obj, std::function<void(Style&)> const& fn = {});
  void draw_game();
  void draw_ui();
  void draw_ui_top();
  void draw_ui_bottom();
  void draw_box();
  void draw_trail(double const i);
  void draw_trails();
  void draw_goals();
  void draw_prompt();

  Box _box;
  std::vector<Object> _trail;
  Goals _goals;
  bool _playing {false};
  bool _mouse_down {false};
  std::size_t _frame {0};
  std::size_t _mouse_frames {0};
  double _distance {0.0};
  double const _delta_ai_target {3.0};
  double _delta_ai {_delta_ai_target};
  std::size_t _score {0};
  std::size_t _high_score {0};
  std::size_t _goal_spacing {0};
  double _max_velocity {0.0};
  double _min_velocity {0.0};
  std::size_t _window_height {0};
  std::size_t _goal_width {0};

  std::string _ui_name {"FLOATYBOX v0.1.0"};
  std::string _ui_start {"Click or <Space> to float!"};
  std::string _ui_high_score {"New High Score!"};

  struct Config {
    double fps {30.0};
    bool color {true};

    struct Style {
      OB::Prism::RGBA bg        {OB::Prism::Hex("1b1e24")};
      OB::Prism::RGBA prompt    {OB::Prism::Hex("abb2bf")};
      OB::Prism::RGBA ui        {OB::Prism::Hex("abb2bf")};
      OB::Prism::RGBA ui_bg     {OB::Prism::Hex("3e4452")};
      OB::Prism::RGBA button    {OB::Prism::Hex("abb2bf")};
      OB::Prism::HSLA box       {OB::Prism::Hex("df6c3e")};
      OB::Prism::HSLA trail     {OB::Prism::Hex("abb2bf")};
      OB::Prism::HSLA goal      {OB::Prism::Hex("61afef")};
      OB::Prism::HSLA goal_pass {OB::Prism::Hex("e5c07b")};
      OB::Prism::HSLA goal_miss {OB::Prism::Hex("d30946")};
    } style;
  } _cfg;

  Style _style_base {Style::Bit_24, Style::Null, _cfg.style.bg, _cfg.style.bg};
  Style _style_default {Style::Default, Style::Null, {}, {}};

  std::array<std::string, 8> _bar_vertical {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
  std::array<std::string, 8> _bar_horizontal {"▏", "▎", "▍", "▌", "▋", "▊", "▉", "█"};

  bool _fixed_size {false};
  std::size_t _width {40};
  std::size_t _height {40};

  asio::io_context _io {1};
  Belle::Signal _sig {_io};
  Read _read {_io};

  OB::Readline _readline;
  std::unordered_map<char32_t, std::function<void()>> _keymap;

  Timer _timer {_io};
  Tick _time {0ns};
  Tick _ftime {0ns};
  Tick _tick {std::chrono::milliseconds(static_cast<long int>(1000.0 / _cfg.fps))};
  Tick _timestep {16ms};
  double _timescale {1.0};
  OB::Timer<Clock> _tick_timer;
  std::chrono::time_point<Clock> _tick_begin {(Clock::time_point::min)()};
  std::chrono::time_point<Clock> _tick_end {(Clock::time_point::min)()};
  double _fps_actual {0.0};

  std::unique_ptr<OB::Term::Mode> _term_mode;
  Window _win;

  std::vector<std::pair<char32_t, Tick>> _code {{0, 0ms}, {0, 0ms}, {0, 0ms}, {0, 0ms}, {0, 0ms}, {0, 0ms}, {0, 0ms}, {0, 0ms}, {0, 0ms}, {0, 0ms}};
};

#endif // APP_HH
