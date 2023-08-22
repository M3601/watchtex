#include <tex.hpp>

#include <atomic>
#include <clone3.hpp>
#include <csignal>
#include <fstream>
#include <jot.hpp>
#include <mutex>
#include <optional>
#include <rkhash.hpp>
#include <utility>
#include <vector>

extern "C" {
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
}

typedef std::filesystem::path path_t;

using hash = hash_t<0x3dad792b, 0x37f5bdcb, 0x3ce6a7af, 0x318d14ef>;

static void remove_comments(char *content, u64 size) {
  static constexpr char NOCOMMENT = '?';

  if (content == nullptr) return;

  for (u64 i = 0; i < size; i++)
    if (content[i] == '%')
      while (i < size && content[i] != '\n') content[i++] = NOCOMMENT;

  static constexpr hash START("\\begin{comment}"), END("\\end{comment}");
  static constexpr u64 N = START.length, M = END.length;
  hash span1, span2;
  u64 i;
  for (i = 0; i < N && i < size; i++) span1.add_right(content[i]);
  for (i = N - M; i < N && i < size; i++) span2.add_right(content[i]);
  bool mode = false;
  std::vector<std::pair<u64, u64>> segments;
  for (; i < size; i++) {
    bool flip = (span1 == START) || (span2 == END);
    if (flip && (mode = !mode)) segments.emplace_back(i, size - 1);
    if (mode) segments.back().second = i + 1;
    span1.remove_left(content[i - N]);
    span1.add_right(content[i]);
    span2.remove_left(content[i - M]);
    span2.add_right(content[i]);
  }
  for (auto &&[start, end] : segments) {
    u64 length = end - start - M;
    char *ptr  = content + start;
    std::memset(ptr, NOCOMMENT, length);
    if (length) {
      *ptr = '\n';
      ptr += length - 1;
      *ptr = '\n';
    }
  }
  content[size] = 0;
}

static std::optional<std::string> next_token(const char *ptr) {
  if (ptr == nullptr) return std::nullopt;
  u64 len, opened;
  len = opened = 0;
  if (ptr[0] && ptr[0] == '{') opened++;
  while (opened && ptr[++len]) {
    if (ptr[len] == '{') opened++;
    if (ptr[len] == '}') opened--;
  }
  if (opened) return std::nullopt;
  if (len == 0) return "";
  return std::string(ptr + 1, len - 1);
}

static char *next_include(char *ptr) {
  static hash COMMANDS[] = {
    hash("\\input"),
    hash("\\include"),
    hash("\\includeonly"),
  };
  if (ptr == nullptr) return nullptr;
  for (auto &&mask : COMMANDS) {
    hash span;
    u64 i;
    for (i = 0; i < mask.length && ptr[i]; i++) span.add_right(ptr[i]);
    if (span == mask) return ptr + mask.length;
    for (; ptr[i]; i++) {
      span.remove_left(ptr[i - mask.length]);
      span.add_right(ptr[i]);
      if (span == mask) return ptr + i + 1;
    }
  }
  return nullptr;
}

static void compile(path_t path);

namespace tex {
void analyze(path_t path, graph_t &deps, graph_t &roots) {
  path = std::filesystem::canonical(path);
  path = std::filesystem::absolute(path);
  if (!std::filesystem::exists(path)) {
    jot::warn("analyze: path `{}` does not exist", path.string());
    return;
  }
  if (std::filesystem::is_regular_file(path) && path.extension() == ".tex") {
    // clear previous dependencies
    for (auto &&dep : deps[path]) roots[dep].erase(path);
    deps[path].clear();
    path_t directory = path.parent_path();
    const u64 size   = std::filesystem::file_size(path);
    if (size == 0) {
      jot::warn("analyze: path `{}` is empty", path.string());
      return;
    }
    char *content = new char[size + 1];
    {
      std::ifstream input(path, std::ios::binary);
      input.read(content, size);
      content[size] = 0;
      input.close();
    }
    remove_comments(content, size);
    char *ptr = content;
    while ((ptr = next_include(ptr))) {
      auto token = next_token(ptr);
      if (!token.has_value()) continue;
      jot::info("{}", token.value());
      path_t dep = directory / token.value();
      if (!std::filesystem::exists(dep)) {
        jot::warn("analyze: path `{}` does not exist", dep.string());
        continue;
      }
      if (std::filesystem::is_regular_file(dep) && dep.extension() == ".tex") {
        deps[path].insert(dep);
        roots[dep].insert(path);
      } else {
        jot::warn("analyze: dependency `{}` not supported", dep.string());
      }
    }
    delete[] content;
  } else if (std::filesystem::is_directory(path)) {
    for (auto &&entry : std::filesystem::directory_iterator(path)) {
      bool ok = false;
      ok      = ok || (std::filesystem::is_regular_file(entry) && entry.path().extension() == ".tex");
      ok      = ok || (std::filesystem::is_directory(entry) && entry.path().filename() != "node_modules");
      if (!ok) continue;
      analyze(entry, deps, roots);
    }
  } else {
    jot::warn("analyze: path `{}` is not analyzable", path.string());
    return;
  }
}

void build(const path_t &path, const graph_t &, const graph_t &roots) {
  std::vector<path_t> todo;
  {
    std::vector<path_t> queue;
    queue.push_back(path);
    while (queue.size()) {
      auto next = queue.back();
      queue.pop_back();
      if (roots.contains(next) && roots.at(next).size()) {
        for (auto &&root : roots.at(next)) queue.push_back(root);
      } else {
        todo.push_back(next);
      }
    }
  }
  for (auto &&path : todo) compile(path);
}
} // namespace tex

struct job_t {
  i32 pid;
  std::map<path_t, job_t> *jobs;
  path_t path;
  std::mutex *mutex;
  void *stack;
  bool done;
};

static constexpr u64 STACK_SIZE = 0x1000000;

static i32 job_helper(void *arg) {
  path_t path      = (char *)arg;
  path_t directory = path.parent_path();
  std::string base = path.filename().string();
  std::filesystem::current_path(directory);
  i32 nullfd = open("/dev/null", O_RDWR);
  if (nullfd == -1) {
    jot::error("job: cannot open /dev/null");
    return EXIT_FAILURE;
  }
  i32 oldstdout = dup(STDOUT_FILENO);
  i32 oldstderr = dup(STDERR_FILENO);
  dup2(nullfd, STDOUT_FILENO);
  dup2(nullfd, STDERR_FILENO);
  char *const argv[] = { "rubber", "--pdf", "--unsafe", (char *)base.c_str(), NULL };
  execvp("rubber", argv);
  dup2(oldstdout, STDOUT_FILENO);
  dup2(oldstderr, STDERR_FILENO);
  jot::error("cannot execute rubber");
  return EXIT_FAILURE;
}

static i32 job(void *arg) {
  std::unique_ptr<job_t> info = std::make_unique<job_t>(*(job_t *)arg);
  delete (job_t *)arg;
  info->mutex->lock();
  info->mutex->unlock();

  char path[NAME_MAX];
  std::strcpy(path, info->path.c_str());

  void *stack = proc_stack::create(STACK_SIZE);
  struct clone_args options;
  std::memset(&options, 0, sizeof(options));
  options.stack       = (u64)stack;
  options.stack_size  = STACK_SIZE;
  options.exit_signal = SIGCHLD;
  options.flags       = CLONE_VM | CLONE_CLEAR_SIGHAND;

  auto child = clone3(&options, sizeof(options), job_helper, path);

  siginfo_t status;
  auto ret = waitid(P_PID, child, &status, WEXITED);
  proc_stack::release(stack, STACK_SIZE);
  if (ret) {
    jot::error("job: cannot wait for child process");
    return EXIT_FAILURE;
  }
  if (status.si_code == CLD_EXITED) {
    if (status.si_status != EXIT_SUCCESS) {
      jot::warn("compilation terminated with status {}", status.si_status);
    } else {
      jot::info("compilation completed");
    }
  } else if (status.si_code == CLD_KILLED) {
    jot::warn("compilation terminated by signal {}", status.si_status);
  } else if (status.si_code == CLD_DUMPED) {
    jot::warn("compilation terminated by signal {} (core dumped)", status.si_status);
  } else {
    jot::warn("compilation terminated with unknown status {}", status.si_status);
  }

  std::lock_guard<std::mutex> lock(*info->mutex);
  info->jobs->at(info->path).done = true;
  return EXIT_SUCCESS;
}

static void compile(path_t path) {
  static std::mutex jobs_mutex;
  static std::map<path_t, job_t> jobs;

  jobs_mutex.lock();
  if (jobs.contains(path)) {
    const auto &info = jobs.at(path);
    if (!info.done) {
      jot::debug("already compiling `{}`", path.string());
      jot::debug("killing process {}", info.pid);
      if (kill(info.pid, SIGKILL) == -1) {
        jot::warn("failed to kill process {}", info.pid);
      } else {
        jot::debug("killed process {}", info.pid);
      }
    }
    auto ret = waitid(P_PID, info.pid, nullptr, WEXITED);
    proc_stack::release(info.stack, STACK_SIZE);
    if (ret) jot::warn("cannot wait job process");
    jobs.erase(path);
  }
  jobs_mutex.unlock();

  job_t *info = new job_t{
    .pid   = 0,
    .jobs  = &jobs,
    .path  = path,
    .mutex = &jobs_mutex,
    .stack = proc_stack::create(STACK_SIZE),
    .done  = false,
  };

  struct clone_args options;
  std::memset(&options, 0, sizeof(options));
  options.stack       = (u64)info->stack;
  options.stack_size  = STACK_SIZE;
  options.exit_signal = SIGCHLD;
  options.flags       = CLONE_VM | CLONE_CLEAR_SIGHAND;

  info->pid = clone3(&options, sizeof(options), job, info);
  if (info->pid == -1) {
    proc_stack::release(info->stack, STACK_SIZE);
    jot::error("failed to clone process");
    return;
  }
  jobs_mutex.lock();
  jobs[path] = *info;
  jobs_mutex.unlock();
}