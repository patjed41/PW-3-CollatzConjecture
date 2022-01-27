#include <utility>
#include <deque>
#include <future>
#include <vector> // do raportu

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

  for (InfInt const & singleInput : contestInput) {
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
  for (InfInt const& singleInput : contestInput) {
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

ContestResult TeamNewProcesses::runContest(ContestInput const & contestInput)
{
    ContestResult r;
    //TODO
    return r;
}

ContestResult TeamConstProcesses::runContest(ContestInput const & contestInput)
{
    ContestResult r;
    //TODO
    return r;
}

ContestResult TeamAsync::runContest(ContestInput const & contestInput)
{
    ContestResult r;
    //TODO
    return r;
}
