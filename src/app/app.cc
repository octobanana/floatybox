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

#include "app/app.hh"
#include "app/util.hh"
#include "ob/string.hh"
#include "ob/term.hh"
#include "ob/prism.hh"

#include <unistd.h>

#include <cmath>
#include <cstdlib>

#include <array>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <utility>
#include <functional>
#include <string_view>

App::App(OB::Parg const& pg) : _pg {pg} {
}

App::~App() {
}

void App::quit() {
  _io.stop();
}

void App::on_pause() {
  _timer.cancel();
  screen_deinit();
  kill(getpid(), SIGSTOP);
}

void App::on_continue() {
  screen_init();
  on_winch();
  _tick_end = Clock::now();
  _tick_timer.clear().start(_tick_end);
  await_tick();
}

void App::screen_init() {
  std::cout
  << aec::cursor_hide
  << aec::screen_push
  << aec::cursor_hide
  << aec::mouse_enable
  << aec::screen_clear
  << aec::cursor_home
  << std::flush;
  _term_mode->set_raw();
}

void App::screen_deinit() {
  std::cout
  << aec::mouse_disable
  << aec::nl
  << aec::screen_pop
  << aec::cursor_show
  << std::flush;
  _term_mode->set_cooked();
}

void App::await_tick() {
  _tick_timer.stop();
  _timer.expires_at(_tick_timer.end() + std::max(_timestep, std::max(_tick, _tick_timer.time<Tick>())));

  _timer.async_wait([&](auto ec) {
    if (ec) {return;}

    _tick_begin = _tick_end;
    _tick_end = Clock::now();
    _tick_timer.clear().start(_tick_end);
    auto delta = std::chrono::duration_cast<Tick>(_tick_end - _tick_begin);
    delta = Tick(static_cast<long int>(delta.count() * _timescale));
    _time += delta;
    _ftime += delta;
    _fps_actual = 1000.0 / std::chrono::duration_cast<std::chrono::milliseconds>(delta).count();

    double const dt = std::chrono::duration<double>(_timestep).count();
    while (_ftime >= _timestep) {
      _ftime -= _timestep;
      update(dt);
      ++_frame;
    }

    render();
    await_tick();
  });
}

void App::await_signal() {
  _sig.on_signal({SIGINT, SIGTERM}, [&](auto const& ec, auto sig) {
    // std::cerr << "\nEvent: " << Belle::Signal::str(sig) << "\n";
    quit();
  });

  _sig.on_signal(SIGWINCH, [&](auto const& ec, auto sig) {
    // std::cerr << "\nEvent: " << Belle::Signal::str(sig) << "\n";
    _sig.wait();
    on_winch();
  });

  _sig.on_signal(SIGTSTP, [&](auto const& ec, auto sig) {
    // std::cerr << "\nEvent: " << Belle::Signal::str(sig) << "\n";
    _sig.wait();
    on_pause();
  });

  _sig.on_signal(SIGCONT, [&](auto const& ec, auto sig) {
    // std::cerr << "\nEvent: " << Belle::Signal::str(sig) << "\n";
    _sig.wait();
    on_continue();
  });

  _sig.wait();
}

void App::on_winch() {
  if (!_fixed_size) {
    OB::Term::size(_width, _height);
    _state = {};
    game_init();
  }

  _win.size = {_width, _height};
  _win.winch();

  if (_readline._mode == OB::Readline::Mode::autocomplete_init || _readline._mode == OB::Readline::Mode::autocomplete) {
    _readline.normal();
  }
  _readline.size(_width, _height);
}

void App::await_read() {
  _read.on_read([&](auto const& ctx) {
    auto nctx = ctx;
    std::visit([&](auto& e) {on_read(e);}, nctx);
  });

  _read.run();
}

bool App::on_read(Read::Null& ctx) {
  // std::cerr << "DBG> Read::Null: " << ctx.str << "\n";
  return false;
}

bool App::on_read(Read::Mouse& ctx) {
  ctx.pos.x -= 1;
  ctx.pos.y = _height - ctx.pos.y;

  if (input_button(ctx)) {
    return true;
  }

  if (input_map(ctx.ch)) {
    return true;
  }

  return false;
}

bool App::on_read(Read::Key& ctx) {
  input_code(ctx);

  if (input_prompt(ctx)) {
    return true;
  }

  if (input_default(ctx)) {
    return true;
  }

  if (input_default(ctx)) {
    return true;
  }

  if (input_map(ctx.ch)) {
    return true;
  }

  return false;
}


bool App::input_button(Read::Mouse const& ctx) {
  if (ctx.ch == Read::Mouse::Release_left) {
    if (ctx.pos.x >= (_width - 3) && ctx.pos.y == _height - 1) {
      quit();
      return true;
    }
  }

  return false;
}

void App::input_code(Read::Key const& ctx) {
  _code.erase(_code.begin());
  _code.emplace_back(std::make_pair(ctx.ch, _time));

  if (_code.back().second - _code.front().second < 3000ms) {
    if (
        _code[0].first == Key::Up &&
        _code[1].first == Key::Up &&
        _code[2].first == Key::Down &&
        _code[3].first == Key::Down &&
        _code[4].first == Key::Left &&
        _code[5].first == Key::Right &&
        _code[6].first == Key::Left &&
        _code[7].first == Key::Right &&
        _code[8].first == 'b' &&
        _code[9].first == 'a'
        ) {
    }
    else if (
        _code[0].first == Key::Up &&
        _code[1].first == Key::Down &&
        _code[2].first == Key::Left &&
        _code[3].first == Key::Right &&
        _code[4].first == Key::Down &&
        _code[5].first == Key::Up &&
        _code[6].first == Key::Right &&
        _code[7].first == Key::Left &&
        _code[8].first == '0' &&
        _code[9].first == '8'
        ) {
    }
  }
}

bool App::input_prompt(Read::Key const& ctx) {
  if (_readline._typing) {
    if (_readline(OB::Text::Char32(ctx.ch, ctx.str))) {
      _readline._typing = false;
      auto input = _readline.get();
      _readline.clear();
    }

    return true;
  }

  return false;
}

bool App::input_default(Read::Key const& ctx) {
  switch (ctx.ch) {
    case OB::Term::ctrl_key('c'): {
      kill(getpid(), SIGINT);
      return true;
    }
    case OB::Term::ctrl_key('z'): {
      kill(getpid(), SIGTSTP);
      return true;
    }

    case OB::Term::ctrl_key('l'): {
      kill(getpid(), SIGWINCH);
      return true;
    }

    default: {
      break;
    }
  }

  return false;
}

bool App::input_map(char32_t const ch) {
  if (auto it = _keymap.find(ch); it != _keymap.end()) {
    // if (_record.events.empty() || _record.events.back().frame != _frame) {
    //   it->second();
    //   _record.events.emplace_back(Event{ch, _frame});
    //   std::wcerr << "Event(" << ch << ", " << _frame << ")\n";
    // }
    it->second();

    return true;
  }

  return false;
}

void App::update(double const dt) {
  input();
  distance(dt);
  ai(dt);
  movement(dt);
  detect_collision();
}

void App::input() {
  _io.run_for(std::chrono::microseconds(1));

  // if (_replay && _record_best.events.size()) {
  //   if (_record_best.events.front().frame == _frame) {
  //     input_map(_record_best.events.front().type);
  //     _record_best.events.pop_front();
  //   }
  // }

  if (_mouse_down) {
    if (_mouse_frames == 0 || _mouse_frames >= 8) {
      increase_velocity();
    }
    ++_mouse_frames;
  }
}

void App::distance(double const dt) {
  _distance += -_state.speed * dt;
}

void App::ai(double const dt) {
  if (!_playing) {
    _delta_ai += dt;
    if (_delta_ai < _delta_ai_target) {
      return;
    }

    for (std::size_t i = 0; i < _goals.size(); ++i) {
      auto const& goal = _goals[i];
      if (goal.state == Goal::State::Null) {
        auto const min_y = 0.4 + goal.sprites.back().position.y + goal.sprites.back().size.y;
        if (_box.position.y < min_y) {
          _box.velocity = _state.impulse;
        }
        break;
      }
    }
  }
}

void App::movement(double const dt) {
  move_trail();
  move_box(dt);
  move_goals(dt);
}

void App::move_trail() {
  while (_distance >= _trail.back().size.x) {
    _distance -= _trail.back().size.x;
    for (std::size_t i = 0; i < _trail.size() - 1; ++i) {
      _trail[i].position.y = _trail[i + 1].position.y;
    }
    _trail.back().position.y = _box.position.y;
  }
}

void App::move_box(double const dt) {
  _box.velocity += _state.gravity * dt;
  _box.velocity = clamp(_box.velocity, _min_velocity, _max_velocity);
  _box.position.y += _box.velocity * dt;
  _box.position.y = clamp(_box.position.y, 1.0, static_cast<double>(_height - 2));

  auto const y = _box.position.y;
  if (y <= 1 || y >= _height - 2) {
    _box.velocity = 0.0;
  }
}

void App::move_goals(double const dt) {
  for (auto& goal : _goals) {
    if (goal.state == Goal::State::Pass) {
      for (auto& sprite : goal.sprites) {
        sprite.position.x = lerp(goal.sprites[0].position.x, -4.0, 1.0 - std::pow(1.0 - 0.9, dt));
        sprite.position.y = lerp(goal.sprites[0].position.y, -4.0, 1.0 - std::pow(1.0 - 0.9, dt));
      }
    }
    else {
      for (auto& sprite : goal.sprites) {
        sprite.position.x += _state.speed * dt;
        sprite.position.y += goal.velocity * dt;
      }
    }

    for (auto& collider : goal.colliders) {
      collider.position.x += _state.speed * dt;
      collider.position.y += goal.velocity * dt;
    }

    goal.pass.position.x += _state.speed * dt;
    goal.pass.position.y += goal.velocity * dt;

    if (goal.sprites.back().position.y <= 1 || goal.sprites.front().position.y + goal.sprites.front().size.y >= _height - 1) {
      goal.velocity = -goal.velocity;
    }
  }

  while (_goals.size() && static_cast<int>(std::floor(_goals.front().colliders.back().position.x + _goals.front().colliders.back().size.x)) < 0) {
    _goals.erase(_goals.begin());
  }

  while (_goals.size() > 2 && static_cast<int>(std::floor(_goals[_goals.size() - 2].colliders.back().position.x)) > _width) {
    _goals.erase(_goals.end() - 1);
  }

  while (_goals.size() && static_cast<int>(std::floor(_goals.back().colliders.back().position.x)) < static_cast<int>(_width)) {
    auto const& obj = _goals.back().colliders.back();
    double const x = obj.position.x + obj.size.x + _goal_spacing;
    add_goal(x, random_range(_window_height + 1ul, _height - (_window_height * 2ul) - 1ul, _state.seed++));
  }
}

void App::detect_collision() {
  for (auto& goal : _goals) {
    if (goal.state != Goal::State::Null) {continue;}

    for (auto& collider : goal.colliders) {
      if (intersect(_box.position, _box.position + static_cast<Vec2n<double>>(_box.size), collider.position, collider.position + static_cast<Vec2n<double>>(collider.size))) {
        goal.state = Goal::State::Miss;
        goal.velocity = 0.0;
        _playing = false;
        if (_score > _high_score) {
          _high_score = _score;
        }
        _score = 0;
        return;
      }
    }

    if (intersect(_box.position, _box.position + static_cast<Vec2n<double>>(_box.size), goal.pass.position, goal.pass.position + static_cast<Vec2n<double>>(goal.pass.size))) {
      goal.state = Goal::State::Pass;
      if (_playing) {
        ++_score;
      }
      return;
    }
  }
}

void App::add_goal(double const x, std::size_t const y) {
  Goal goal;

  // velocity
  goal.velocity = random_range(-4, 4, _state.seed);

  // sprites
  Object sprite;
  // top
  sprite.size = Size(_goal_width, _goal_width / 2ul);
  sprite.position = Position(x, y + _window_height);
  goal.sprites.emplace_back(sprite);
  // bottom
  sprite.size = Size(_goal_width, _goal_width / 2ul);
  sprite.position = Position(x, y - (_window_height / 2ul));
  goal.sprites.emplace_back(sprite);

  // colliders
  Object collider;
  // top
  collider.size = Size(_goal_width, _height);
  collider.position = Position(x, y + _window_height);
  goal.colliders.emplace_back(collider);
  // bottom
  collider.size = Size(_goal_width, _height);
  collider.position = Position(x, static_cast<double>(y) - static_cast<double>(_height));
  goal.colliders.emplace_back(collider);

  // pass collider
  goal.pass.size = Size(2ul, _window_height);
  goal.pass.position = Position(collider.position.x + collider.size.x + _box.size.x, y);

  _goals.emplace_back(goal);
}

void App::increase_velocity() {
  _playing = true;
  _delta_ai = 0.0;
  _box.velocity = _state.impulse;
  _box.velocity = clamp(_box.velocity, _min_velocity, _max_velocity);
}

void App::draw() {
  draw_game();
  draw_ui();
}

void App::draw_vertical(Object const& obj, std::function<void(Style&)> const& fn) {
  auto init_style = _cfg.color ? Style{Style::Bit_24, 0, _cfg.style.box, _cfg.style.bg} : _style_default;
  if (fn) {
    fn(init_style);
  }

  if (static_cast<int>(std::floor((obj.position.y + obj.size.y) * 8)) % 8 == 0) {
    for (std::size_t y = 0; y < obj.size.y; ++y) {
      for (std::size_t x = 0; x < obj.size.x; ++x) {
        _win.buf(Pos{static_cast<std::size_t>(std::floor(obj.position.x + x)), static_cast<std::size_t>(std::floor(obj.position.y + y))}, Cell{1, init_style, _bar_vertical[7]});
      }
    }
  }
  else {
    for (std::size_t y = 0; y <= obj.size.y; ++y) {
      std::string block;
      auto style = init_style;

      if (y == 0) {
        block = _bar_vertical[static_cast<std::size_t>(std::floor((obj.position.y + obj.size.y) * 8)) % 8];

        if (_cfg.color) {
          std::swap(style.fg, style.bg);
        }
        else {
          style.attr = Style::Reverse;
        }
      }
      else if (y == obj.size.y) {
        block = _bar_vertical[static_cast<std::size_t>(std::floor((obj.position.y + obj.size.y) * 8)) % 8];
      }
      else {
        block = _bar_vertical[7];
      }

      for (std::size_t x = 0; x < obj.size.x; ++x) {
        _win.buf(Pos{static_cast<std::size_t>(std::floor(obj.position.x + x)), static_cast<std::size_t>(std::floor(obj.position.y + y))}, Cell{1, style, block});
      }
    }
  }
}

void App::draw_horizontal(Object const& obj, std::function<void(Style&)> const& fn) {
  auto init_style = _cfg.color ? Style{Style::Bit_24, 0, _cfg.style.box, _cfg.style.bg} : _style_default;
  if (fn) {
    fn(init_style);
  }

  if (static_cast<int>(std::floor((obj.position.x + obj.size.x) * 8)) % 8 == 0) {
    for (std::size_t x = 0; x < obj.size.x; ++x) {
      for (std::size_t y = 0; y < obj.size.y; ++y) {
        _win.buf(Pos{static_cast<std::size_t>(std::floor(obj.position.x + x)), static_cast<std::size_t>(std::floor(obj.position.y + y))}, Cell{1, init_style, _bar_horizontal[7]});
      }
    }
  }
  else {
    for (std::size_t x = 0; x <= obj.size.x; ++x) {
      std::string block;
      auto style = init_style;

      if (x == 0) {
        block = _bar_horizontal[static_cast<std::size_t>(std::floor((obj.position.x + obj.size.x) * 8)) % 8];

        if (_cfg.color) {
          std::swap(style.fg, style.bg);
        }
        else {
          style.attr = Style::Reverse;
        }
      }
      else if (x == obj.size.x) {
        block = _bar_horizontal[static_cast<std::size_t>(std::floor((obj.position.x + obj.size.x) * 8)) % 8];
      }
      else {
        block = _bar_horizontal[7];
      }

      for (std::size_t y = 0; y < obj.size.y; ++y) {
        _win.buf(Pos{static_cast<std::size_t>(std::floor(obj.position.x + x)), static_cast<std::size_t>(std::floor(obj.position.y + y))}, Cell{1, style, block});
      }
    }
  }
}

void App::draw_game() {
  draw_trails();
  draw_goals();
  draw_box();
}

void App::draw_ui() {
  draw_ui_top();
  draw_ui_bottom();
  draw_prompt();
}

void App::draw_ui_top() {
  auto style = _cfg.color ? Style{Style::Bit_24, 0, _cfg.style.ui, _cfg.style.ui_bg} : _style_default;
  if (_cfg.color) {
    style.attr |= Style::Bold;
  }
  else {
    style.attr |= Style::Reverse;
  }

  _win.buf.put(Pos{0, _height - 1}, Cell{1, style, std::string(_width, ' ')});

  _win.buf.put(Pos{(_width / 2) - (_ui_name.size() / 2), _height - 1}, Cell{1, style, _ui_name});

  style.fg.a(255);
  _win.buf.put(Pos{0, _height - 1}, Cell{1, style, std::to_string(static_cast<int>(std::round(_fps_actual)))});

  if (_cfg.color) {
    style.fg = _cfg.style.button;
  }
  _win.buf.put(Pos{_width - 3, _height - 1}, Cell{1, style, "[X]"});
}

void App::draw_ui_bottom() {
  auto style = _cfg.color ? Style{Style::Bit_24, 0, _cfg.style.ui, _cfg.style.ui_bg} : _style_default;
  if (_cfg.color) {
    style.attr |= Style::Bold;
  }
  else {
    style.attr |= Style::Reverse;
  }

  _win.buf.put(Pos{0, 0}, Cell{1, style, std::string(_width, ' ')});

  {
    auto style_score = style;
    style_score.fg.a(255);

    auto score = std::to_string(_score);
    _win.buf.put(Pos{0, 0}, Cell{1, style_score, score});

    auto high_score = std::to_string(_high_score);
    _win.buf.put(Pos{_width - high_score.size(), 0}, Cell{1, style_score, high_score});
  }

  if (!_playing) {
    auto pos = Pos((_width / 2) - (_ui_start.size() / 2), 0);
    _win.buf.put(pos, Cell{1, style, _ui_start});
  }
  else if (_score != 0 && _score > _high_score) {
    auto pos = Pos((_width / 2) - (_ui_high_score.size() / 2), 0);
    _win.buf.put(pos, Cell{1, style, _ui_high_score});
  }
}

void App::draw_box() {
  draw_vertical(_box);
}

void App::draw_trail(double const i) {
  draw_vertical(_trail[i], [&](auto& style) {
    if (_cfg.color) {
      style.fg = _cfg.style.trail;
      style.fg.a(255 * (i / _trail.size()));
    }
  });
}

void App::draw_trails() {
  for (std::size_t i = 0; i < _trail.size() - 1; ++i) {
    if (_trail[i].position.y < _trail[i + 1].position.y) {
      draw_trail(i);
    }
  }
  if (_trail[_trail.size() - 2].position.y < _trail.back().position.y) {
    draw_trail(_trail.size() - 1);
  }
}

void App::draw_goals() {
  for (auto const& goal : _goals) {
    auto const& fg = goal.state == Goal::State::Null ? _cfg.style.goal : (goal.state == Goal::State::Pass ? _cfg.style.goal_pass : (_cfg.style.goal_miss));
    for (auto const& sprite : goal.sprites) {
      draw_vertical(sprite, [&](auto& style) {
        style.fg = fg;
        if (goal.state == Goal::State::Pass && sprite.position.x + sprite.size.x < _box.position.x) {
          style.fg.a(255 * ((sprite.position.x + sprite.size.x) / _box.position.x));
        }
        else if (sprite.position.x > _box.position.x) {
          style.fg.a(255 * ((sprite.position.x - _width) / (_box.position.x - _width)));
        }
      });
    }
  }
}

void App::draw_prompt() {
  if (_readline._typing) {
    auto style = _cfg.color ? Style{Style::Bit_24, 0, _cfg.style.prompt, _cfg.style.bg} : Style{Style::Default, 0, {}, {}};

    if (_readline._mode == OB::Readline::Mode::autocomplete_init || _readline._mode == OB::Readline::Mode::autocomplete) {
      _win.buf.cursor(Pos(0, 1));
      _win.buf.put(Cell{1, style, std::string(_width, ' ')});
      _win.buf.cursor(Pos(0, 1));
      _win.buf.put(Cell{1, style, _readline._autocomplete._lhs});
      _win.buf.put(Cell{1, style, _readline._autocomplete._text});
      _win.buf.cursor(Pos(_width - 1, 1));
      _win.buf.put(Cell{1, style, _readline._autocomplete._rhs});
      for (std::size_t i = 0; i < _readline._autocomplete._hls; ++i) {
        _win.buf.col(Pos(_readline._autocomplete._hli + i, 1)).style.attr |= Style::Reverse;
      }
    }

    _readline.refresh();
    _win.buf.cursor(Pos(0, 0));
    _win.buf.put(Cell{1, style, std::string(_width, ' ')});
    _win.buf.cursor(Pos(0, 0));
    _win.buf.put(Cell{1, style, _readline._prompt.lhs});
    _win.buf.put(Cell{1, style, _readline._input.fmt});
    _win.buf.put(Cell{1, style, _readline._prompt.rhs});
    _win.buf.col(Pos(_readline._input.cur + 1, 0)).style.attr |= Style::Reverse;
  }
}

void App::render() {
  draw();
  _win.render();
}

void App::keymap_init() {
  _keymap.clear();

  _keymap[':'] = [&]() {
    _readline._typing = true;
  };

  _keymap['?'] = [&]() {
    _timer.cancel();
    screen_deinit();

    std::system(("$(which less) -Ri '+/Key Bindings' <<'EOF'\n" + _pg.help() + "EOF").c_str());

    screen_init();
    on_winch();
    _tick_end = Clock::now();
    _tick_timer.clear().start(_tick_end);
    await_tick();
  };

  _keymap[Key::Backspace] = [&]() {
    _state = {};
    game_init();
  };

  _keymap['q'] = [&]() {
    quit();
  };

  _keymap['Q'] = [&]() {
    quit();
  };

  _keymap['c'] = [&]() {
    _cfg.color = !_cfg.color;
  };

  _keymap['s'] = [&]() {
    _timescale = _timescale != 1.0 ? 1.0 : 0.2;
  };

  _keymap['d'] = [&]() {
    _timescale = _timescale != 1.0 ? 1.0 : 0.5;
  };

  _keymap[' '] = [&]() {
    increase_velocity();
  };

  _keymap['w'] = [&]() {
    increase_velocity();
  };

  _keymap['k'] = [&]() {
    increase_velocity();
  };

  _keymap[Read::Key::Up] = [&]() {
    increase_velocity();
  };

  _keymap[Read::Mouse::Scroll_up] = [&]() {
    increase_velocity();
  };

  _keymap[Read::Mouse::Press_left] = [&]() {
    _mouse_down = true;
    _mouse_frames = 0;
  };

  _keymap[Read::Mouse::Release_left] = [&]() {
    _mouse_down = false;
  };
}

void App::readline_init() {
  _readline.size(_width, _height);
  _readline.boundaries(" `',@()[]{}");
}

void App::game_init() {
  // game state
  _playing = false;
  _timescale = 1.0;
  _frame = 0;
  _mouse_down = false;
  _mouse_frames = 0;
  _distance = 0.0;
  _delta_ai = _delta_ai_target;
  _score = 0;
  _goal_spacing = -_state.speed * (_height / _max_velocity);
  _max_velocity = _state.impulse;
  _min_velocity = -_state.impulse;
  _window_height = (_state.box_size.x * 2) + 1;
  _goal_width = _state.box_size.x + 2;
  _time = 0ns;
  _ftime = 0ns;

  // box
  _box.velocity = 0.0;
  _box.size = _state.box_size;
  _box.position = {static_cast<double>(static_cast<int>(std::trunc(static_cast<double>(_width) * _state.box_offset)) - static_cast<int>(std::trunc(_box.size.x / 2))), static_cast<double>(static_cast<int>(_height / 2) - static_cast<int>(std::trunc(_box.size.y / 2)))};

  // trail
  _trail.clear();
  for (int pos = 0; pos < std::floor(_box.position.x); ++pos) {
    auto& box = _trail.emplace_back(_box);
    box.size.x = 1;
    box.position.x = pos;
  }

  // goals
  _goals.clear();
  add_goal(_width, random_range(_window_height + 1ul, _height - (_window_height * 2ul) - 1ul, _state.seed++));

  // timers
  _timer.cancel();
  _tick_end = Clock::now();
  _tick_timer.clear().start(_tick_end);
  await_tick();
}

void App::run() {
  await_signal();

  auto const is_term = Term::is_term(STDOUT_FILENO);
  if (!is_term) {throw std::runtime_error("stdout is not a tty");}

  if (_cfg.color) {
    _style_base = Style{Style::Bit_24, Style::Null, _cfg.style.bg, _cfg.style.bg};
  }
  else {
    _style_base = Style{Style::Default, Style::Null, {}, {}};
  }
  _win.style_base = _style_base;

  _term_mode = std::make_unique<OB::Term::Mode>();
  screen_init();
  on_winch();
  keymap_init();
  readline_init();
  await_read();
  _state = {};
  game_init();
  _io.run();
  screen_deinit();
}
