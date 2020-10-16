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

#ifndef INFO_HH
#define INFO_HH

#include "ob/parg.hh"
#include "ob/term.hh"

#include <cstddef>

#include <string>
#include <string_view>
#include <iostream>

inline int program_info(OB::Parg& pg);
inline bool program_color(std::string_view color);
inline void program_init(OB::Parg& pg);

inline void program_init(OB::Parg& pg) {
  pg.name("floatybox").version("0.1.0 (15.10.2020)");
  pg.description("Float your way through perilous terrain in this endless side-scoller game.");

  pg.usage("");
  pg.usage("[--colour=<on|off|auto>] -h|--help");
  pg.usage("[--colour=<on|off|auto>] -v|--version");
  pg.usage("[--colour=<on|off|auto>] --license");

  pg.info({"Key Bindings", {
    {"q, Q, <ctrl-c>", "quit the program"},
    {"<ctrl-z>", "suspend the program"},
    {"<ctrl-l>", "force screen redraw"},
    {"<backspace>", "reset game and settings to default state"},
    {"<mouse-left>, <up>, <space>, w, k", "increase velocity"},
    {"?", "show key bindings"},
    {"c", "toggle enable/disable colour"},
    {"s", "super slow-motion"},
    {"d", "slow-motion"},
    {"??????????", "secret 1"},
    {"??????????", "secret 2"},
  }});

  pg.info({"Examples", {
    {"floatybox",
      "run the program"},
    {"floatybox --help --colour=off",
      "print the help output, without colour"},
    {"floatybox --help",
      "print the help output"},
    {"floatybox --version",
      "print the program version"},
    {"floatybox --license",
      "print the program license"},
  }});

  pg.info({"Exit Codes", {
    {"0", "normal"},
    {"1", "error"},
  }});

  pg.info({"Meta", {
    {"", "The version format is 'major.minor.patch (day.month.year)'."},
  }});

  pg.info({"Repository", {
    {"", "https://github.com/octobanana/floatybox.git"},
  }});

  pg.info({"Homepage", {
    {"", "https://octobanana.com/software/floatybox"},
  }});

  pg.author("Brett Robinson (octobanana) <octobanana.dev@gmail.com>");

  // general flags
  pg.set("help,h", "Print the help output.");
  pg.set("version,v", "Print the program version.");
  pg.set("license", "Print the program license.");

  // options
  pg.set("colour", "auto", "on|off|auto", "Print the program output with colour either on, off, or auto based on if stdout is a tty, the default value is 'auto'.");

  // allow and capture positional arguments
  // pg.set_pos();
}

inline bool program_color(std::string_view color) {
  return color == "auto" ?
    OB::Term::is_term(STDOUT_FILENO) :
    color == "on";
}

inline int program_info(OB::Parg& pg) {
  // init info/options
  program_init(pg);

  // parse options
  auto const status {pg.parse()};

  // set output color choice
  pg.color(program_color(pg.get<std::string>("colour")));

  if (status < 0) {
    // an error occurred
    std::cerr
    << pg.usage()
    << "\n"
    << pg.error();

    return -1;
  }

  if (pg.get<bool>("help")) {
    // show help output
    std::cout << pg.help();

    return 1;
  }

  if (pg.get<bool>("version")) {
    // show version output
    std::cout << pg.version();

    return 1;
  }

  if (pg.get<bool>("license")) {
    // show license output
    std::cout << pg.license();

    return 1;
  }

  // success
  return 0;
}

#endif // INFO_HH
