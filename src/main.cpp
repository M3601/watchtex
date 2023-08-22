#include <array>
#include <compare>
#include <csignal>
#include <filesystem>
#include <fmt/color.h>
#include <fmt/format.h>
#include <jot.hpp>
#include <map>
#include <shrdmm.hpp>
#include <string>
#include <tex.hpp>
#include <types.hpp>
#include <watcher.hpp>
extern "C" {
#include <sys/inotify.h>
}

typedef std::filesystem::path path_t;
typedef std::array<u64, 22> statistic_t;

void atstart(void);
void atend(void);
void interrupt(int);

void welcome(void);
statistic_t &updatestat(path_t path, u32 mask);
std::string maskstr(u32 mask);

static std::map<path_t, statistic_t> statistics;
static watcher_t watcher;

#define MATCH(code, mask) (((code) & (mask)) == (mask))

int main(int argc, char *argv[]) {
  { atstart(); }
  path_t path = argc > 1 ? argv[1] : ".";
  path        = std::filesystem::canonical(path);
  path        = std::filesystem::absolute(path);
  jot::info("watching `{}`", path.string());
  watcher.add(path);
  watcher.start();
  graph_t deps, roots;
  tex::analyze(path, deps, roots);
  while (true) {
    auto event = watcher.poll();
    jot::debug("{} {}", event.path.string(), maskstr(event.mask));
    if (MATCH(event.mask, IN_CREATE | IN_ISDIR)) {
      watcher.add(event.path);
      continue;
    }
    if (MATCH(event.mask, IN_DELETE | IN_ISDIR)) {
      watcher.remove(event.path);
      continue;
    }
    if (MATCH(event.mask, IN_DELETE_SELF)) {
      watcher.remove(event.path);
      continue;
    }
    if (event.path.extension().string() != ".tex") continue;
    const auto &stat = updatestat(event.path, event.mask);
    if (MATCH(event.mask, IN_CLOSE_WRITE)) {
      jot::info("`{}` modified [x{}]", event.path.string(), stat[2]);
      tex::analyze(event.path, deps, roots);
      tex::build(event.path, deps, roots);
    }
  }
  { atend(); }
  return 0;
}

static constexpr u32 FLAGS[] = {
  IN_ACCESS,    IN_ATTRIB,      IN_CLOSE_WRITE, IN_CLOSE_NOWRITE, IN_CREATE,   IN_DELETE,  IN_DELETE_SELF, IN_MODIFY,
  IN_MOVE_SELF, IN_MOVED_FROM,  IN_MOVED_TO,    IN_OPEN,          IN_IGNORED,  IN_ISDIR,   IN_Q_OVERFLOW,  IN_UNMOUNT,
  IN_ONLYDIR,   IN_DONT_FOLLOW, IN_EXCL_UNLINK, IN_MASK_CREATE,   IN_MASK_ADD, IN_ONESHOT,
};
static constexpr std::string_view FLAGSSTR[] = {
  "ACCESS",    "ATTRIB",      "CLOSE_WRITE", "CLOSE_NOWRITE", "CREATE",   "DELETE",  "DELETE_SELF", "MODIFY",
  "MOVE_SELF", "MOVED_FROM",  "MOVED_TO",    "OPEN",          "IGNORED",  "ISDIR",   "Q_OVERFLOW",  "UNMOUNT",
  "ONLYDIR",   "DONT_FOLLOW", "EXCL_UNLINK", "MASK_CREATE",   "MASK_ADD", "ONESHOT",
};
static_assert(sizeof(FLAGS) / sizeof(*FLAGS) == sizeof(FLAGSSTR) / sizeof(*FLAGSSTR));
static constexpr u64 NFLAGS = sizeof(FLAGS) / sizeof(*FLAGS);

void atstart(void) {
  std::setbuf(stdout, nullptr);
  std::setbuf(stderr, nullptr);
  shrdmm::init();
  jot::init();
  std::signal(SIGINT, interrupt);
  welcome();
}
void atend(void) {
  watcher.stop();
  jot::deinit();
  shrdmm::deinit();
}

void interrupt(int) {
  fmt::print(stderr, "\n");
  jot::info("interrupted by user");
  for (auto &&[path, stat] : statistics) {
    jot::debug("{}:", path.string());
    for (u64 i = 0; i < NFLAGS; i++)
      if (stat[i]) jot::debug("  {}: {}", FLAGSSTR[i], stat[i]);
    if (stat[2]) { jot::info("`{}` has been modified {} times", path.string(), stat[2]); }
  }
  atend();
  std::exit(1);
}

void welcome(void) {
  static constexpr auto style1  = fmt::emphasis::bold;
  static constexpr auto style2  = fmt::fg(fmt::rgb(0x2962ff)) | fmt::emphasis::bold;
  static constexpr auto watch   = fmt::styled("Watch", style1);
  static constexpr auto tex     = fmt::styled("TeX", style2);
  static constexpr auto version = "1.0";
  fmt::print(stderr, "{}{} v{}\n", watch, tex, version);
}

statistic_t &updatestat(path_t path, u32 mask) {
  if (!statistics.contains(path)) statistics.insert({ path, statistic_t() });
  auto &stat = statistics.at(path);
  for (u32 i = 0; i < NFLAGS; i++)
    if (MATCH(mask, FLAGS[i])) stat[i]++;
  return stat;
}

std::string maskstr(u32 mask) {
  static constexpr std::string_view SEP = " | ";
  std::string str;
  for (u32 i = 0; i < NFLAGS; i++)
    if (MATCH(mask, FLAGS[i])) {
      str += FLAGSSTR[i];
      str += SEP;
    }
  if (!str.empty()) str.erase(str.size() - SEP.size());
  return str;
}