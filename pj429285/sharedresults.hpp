#ifndef SHAREDRESULTS_HPP
#define SHAREDRESULTS_HPP

#include <map>

#include <iostream> // do usunięcia

class SharedResults {
private:

  std::mutex mut;
  std::map<InfInt, uint64_t> count;
  std::map<InfInt, std::condition_variable> for_count;

public:

  uint64_t calcCollatz(InfInt n) {
    if (n == 1) {
      return 0;
    }

    std::unique_lock<std::mutex> mut_lock(mut);
    if (count.find(n) == count.end()) {
      count[n] = 0;
      mut_lock.unlock();

      if (n % 2 == 1) {
        count[n] = calcCollatz(n * 3 + 1) + 1;
      }
      else {
        count[n] = calcCollatz(n / 2) + 1;
      }

      mut_lock.lock();

      for_count[n].notify_all();
      return count[n];
    }
    else if (count[n] == 0) {
      for_count[n].wait(mut_lock, [&] { return count[n] != 0; });
      return count[n];
    }
    else {
      return count[n];
    }
  }

};

#endif