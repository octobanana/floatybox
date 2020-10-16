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

#ifndef APP_UTIL_HH
#define APP_UTIL_HH

#include "ob/term.hh"
#include "ob/timer.hh"

#include <cmath>
#include <cassert>
#include <cstddef>

#include <chrono>
#include <limits>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <numeric>
#include <iostream>
#include <algorithm>
#include <functional>

template<typename T>
static T mean(std::vector<T> const& v, std::size_t const size) {
  return std::accumulate(v.begin(), v.begin() + size, static_cast<T>(0)) / size;
}

template<typename T>
static T stdev(std::vector<T> const& v, std::size_t const size, T const mean) {
  std::vector<T> dif (size);
  std::transform(v.begin(), v.begin() + size, dif.begin(), [mean](auto const n) {return n - mean;});
  return std::sqrt(std::inner_product(dif.begin(), dif.end(), dif.begin(), static_cast<T>(0)) / size);
}

template<typename T>
static std::enable_if_t<!std::numeric_limits<T>::is_integer, bool>
almost_equal(T x, T y, int ulp = 2) {
  return std::abs(x - y) <= std::numeric_limits<T>::epsilon() * std::abs(x + y) * ulp || std::abs(x - y) < std::numeric_limits<T>::min();
}

template<typename T>
static T clamp(T const val, T const min, T const max) {
  assert(!(max < min));
  if (val < min) {return min;}
  if (val > max) {return max;}
  return val;
}

template<typename T>
static T clampc(T const val, T const min, T const max) {
  assert(!(max < min));
  if (min == max) {return min;}
  if (val < min) {
    if ((min - val) > ((max - min) + 1)) {
      return max - (((min - val) % ((max - min + 1))) - 1);
    }
    else {
      return max - ((min - val) - 1);
    }
  }
  if (val > max) {
    if ((val - max) > ((max - min) + 1)) {
      return min + (((val - max) % ((max - min + 1))) - 1);
    }
    else {
      return min + ((val - max) - 1);
    }
  }
  return val;
}

template<typename T>
static T clampcf(T const val, T const min, T const max) {
  assert(!(max < min));
  if (almost_equal(min, max)) {return min;}
  if (val < min) {
    if ((min - val) > ((max - min) + 1)) {
      return max - (std::fmod((min - val), ((max - min + 1))) - 1);
    }
    else {
      return max - ((min - val) - 1);
    }
  }
  if (val > max) {
    if ((val - max) > ((max - min) + 1)) {
      return min + (std::fmod((val - max), ((max - min + 1))) - 1);
    }
    else {
      return min + ((val - max) - 1);
    }
  }
  return val;
}

template<typename T, typename F>
static constexpr T lerp(T const a, T const b, F const t) {
  return (a + (t * (b - a)));
}

template<typename T>
static T scale(T const val, T const in_min, T const in_max, T const out_min, T const out_max) {
  assert(in_min <= in_max);
  assert(out_min <= out_max);
  return (out_min + (out_max - out_min) * ((val - in_min) / (in_max - in_min)));
}

template<typename T>
static T scale_log(T const val, T const in_min, T const in_max, T const out_min, T const out_max) {
  assert(in_min <= in_max);
  assert(out_min <= out_max);
  auto const b = std::log(out_max / out_min) / (in_max - in_min);
  auto const a = out_max / std::exp(b * in_max);
  return a * std::exp(b * val);
}

template<typename T = std::size_t>
T random_range(T l, T u, unsigned int seed = std::random_device{}()) {
  std::mt19937 gen(seed);
  std::uniform_int_distribution<T> distr(l, u);
  return distr(gen);
}

template<typename T = std::chrono::milliseconds>
void sleep(T const& duration) {
  std::this_thread::sleep_for(duration);
}

template<typename T = std::chrono::milliseconds>
std::size_t fn_timer(std::function<void()> const& fn) {
  OB::Timer<std::chrono::steady_clock> timer;
  timer.start();
  fn();
  timer.stop();
  return static_cast<std::size_t>(timer.time<T>().count());
}

void dbg_log(std::string const& head, std::string const& body);

#endif // APP_UTIL_HH
