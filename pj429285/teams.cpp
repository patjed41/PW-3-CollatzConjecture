#include <utility>
#include <deque>
#include <future>
#include <vector> // do raportu
#include <semaphore.h> // do raportu
#include <sys/mman.h> // do raportu
#include <unistd.h> // do raportu
#include <sys/wait.h> // do raportu

#include "teams.hpp"
#include "contest.hpp"
#include "collatz.hpp"


// do usunięcia
#include <iostream>

void newThreadsThread(uint64_t idx, const InfInt& singleInput,
                      ContestResult& result, std::mutex& mut,
                      uint64_t& running_threads,
                      std::condition_variable& all_threads_running,
                      const std::shared_ptr<SharedResults>& sharedResults) {
  if (sharedResults) {
    result[idx] = sharedResults->calcCollatz(singleInput);
  }
  else {
    result[idx] = calcCollatz(singleInput);
  }

  mut.lock();
  running_threads--;
  mut.unlock();
  all_threads_running.notify_one();
}

ContestResult TeamNewThreads::runContestImpl(ContestInput const & contestInput) {
  ContestResult result;
  result.resize(contestInput.size());
  uint64_t idx = 0;

  std::mutex mut;
  std::condition_variable all_threads_running;
  uint64_t running_threads = 0;

  for (const InfInt& singleInput : contestInput) {
    std::unique_lock<std::mutex> mut_lock(mut);
    all_threads_running.wait(mut_lock, [&] { return running_threads < getSize(); });
    running_threads++;
    mut_lock.unlock();

    std::thread thread = createThread([&, idx] { newThreadsThread(idx, singleInput, result, mut, running_threads, all_threads_running, getSharedResults()); });
    thread.detach();

    idx++;
  }

  std::unique_lock<std::mutex> mut_lock(mut);
  all_threads_running.wait(mut_lock, [&] { return running_threads == 0; });

  return result;
}

void constThreadsThread(uint32_t thread_id, uint32_t thread_num,
                        ContestResult& result, const ContestInput& contestInput,
                        const std::shared_ptr<SharedResults>& sharedResults) {
  for (uint32_t i = thread_id; i < result.size(); i += thread_num) {
    if (sharedResults) {
      result[i] = sharedResults->calcCollatz(contestInput[i]);
    }
    else {
      result[i] = calcCollatz(contestInput[i]);
    }
  }
}

ContestResult TeamConstThreads::runContestImpl(ContestInput const & contestInput)
{
  ContestResult result;
  result.resize(contestInput.size());

  std::vector<std::thread> threads(getSize());
  for (uint32_t thread_id = 0; thread_id < getSize(); thread_id++) {
    threads[thread_id] = createThread([&, thread_id] { constThreadsThread(thread_id, getSize(), result, contestInput, getSharedResults()); });
  }

  for (uint32_t thread_id = 0; thread_id < getSize(); thread_id++) {
    threads[thread_id].join();
  }

  return result;
}

ContestResult TeamPool::runContest(ContestInput const & contestInput)
{
  ContestResult result;
  result.resize(contestInput.size());

  cxxpool::thread_pool thread_pool(getSize());

  std::vector<std::future<uint64_t>> futures;
  for (const InfInt& singleInput : contestInput) {
    if (getSharedResults()) {
      futures.push_back(thread_pool.push([&] { return getSharedResults()->calcCollatz(singleInput); }));
    }
    else {
      futures.push_back(thread_pool.push([&] { return calcCollatz(singleInput); }));
    }
  }

  for (uint32_t i = 0; i < result.size(); i++) {
    result[i] = futures[i].get();
  }

  return result;
}

struct newProcessesMem {
  sem_t new_process;
  uint64_t result[];
};

ContestResult TeamNewProcesses::runContest(ContestInput const & contestInput)
{
  int flags = MAP_SHARED | MAP_ANONYMOUS;
  int prot = PROT_READ | PROT_WRITE;
  int fd_memory = -1;
  auto *mapped_mem = (newProcessesMem *) mmap(nullptr, sizeof(newProcessesMem) + sizeof(uint64_t) * contestInput.size(),
                                              prot, flags, fd_memory, 0);
  assert(mapped_mem != MAP_FAILED);

  uint32_t thread_num = getSize();

  assert(!sem_init(&(mapped_mem->new_process), 1, thread_num));

  uint32_t wait_num = 0;

  for (uint64_t idx = 0; idx < contestInput.size(); idx++) {
    assert(!sem_wait(&(mapped_mem->new_process)));
    switch (fork()) {
      case -1:
        std::cerr << "fork" << getSize();
        exit(1);
      case 0:
        mapped_mem->result[idx] = calcCollatz(contestInput[idx]);
        assert(!sem_post(&(mapped_mem->new_process)));
        exit(0);
      default:
        if (idx >= thread_num) {
          wait(nullptr);
          wait_num++;
        }
        break;
    }
  }

  while (wait_num < contestInput.size()) {
    wait(nullptr);
    wait_num++;
  }

  ContestResult r(contestInput.size());
  for (uint32_t i = 0; i < r.size(); i++) {
    r[i] = mapped_mem->result[i];
  }

  sem_destroy(&(mapped_mem->new_process));

  return r;
}

ContestResult TeamConstProcesses::runContest(ContestInput const & contestInput)
{
  int flags = MAP_SHARED | MAP_ANONYMOUS;
  int prot = PROT_READ | PROT_WRITE;
  int fd_memory = -1;
  auto* result = (uint64_t *) mmap(nullptr, sizeof(uint64_t) * contestInput.size(),
                                   prot, flags, fd_memory, 0);
  assert(result != MAP_FAILED);

  uint32_t process_num = getSize();

  for (uint32_t process_id = 0; process_id < process_num; process_id++) {
    switch (fork()) {
      case -1:
        std::cerr << "fork" << getSize();
        exit(1);
      case 0:
        for (uint32_t i = process_id; i < contestInput.size(); i += process_num) {
          result[i] = calcCollatz(contestInput[i]);
        }
        exit(0);
    }
  }

  for (uint32_t i = 0; i < process_num; i++) {
    wait(nullptr);
  }

  ContestResult r(contestInput.size());
  for (uint32_t i = 0; i < r.size(); i++) {
    r[i] = result[i];
  }

  return r;
}

ContestResult TeamAsync::runContest(ContestInput const & contestInput)
{
  ContestResult result;
  result.resize(contestInput.size());

  std::vector<std::future<uint64_t>> futures;
  for (const InfInt& singleInput : contestInput) {
    if (getSharedResults()) {
      futures.push_back(std::async(std::launch::async, [&] { return getSharedResults()->calcCollatz(singleInput); }));
    }
    else {
      futures.push_back(std::async(std::launch::async, [&] { return calcCollatz(singleInput); }));
    }
  }

  for (uint32_t i = 0; i < result.size(); i++) {
    result[i] = futures[i].get();
  }

  return result;
}
