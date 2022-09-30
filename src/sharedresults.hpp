#ifndef SHAREDRESULTS_HPP
#define SHAREDRESULTS_HPP

#include <map>

class SharedResults {
private:

  std::mutex mut;
  std::map<InfInt, uint64_t> count; // saved results
  std::map<InfInt, std::condition_variable> for_count;

public:

  uint64_t calcCollatz(InfInt n) {
    if (n == 1) {
      return 0;
    }

    std::unique_lock<std::mutex> mut_lock(mut);
    if (count.find(n) == count.end()) { // This thread will compute calcCollatz(n).
      count[n] = 0; // We mark that there is a thread computing calcCollatz(n).
      mut_lock.unlock();

      if (n % 2 == 1) {
        count[n] = calcCollatz(n * 3 + 1) + 1;
      }
      else {
        count[n] = calcCollatz(n / 2) + 1;
      }

      mut_lock.lock();

      for_count[n].notify_all(); // We unlock threads that are waiting for calcCollatz(n).
      return count[n];
    }
    else if (count[n] == 0) { // Someone is currently computing CalcCollatz(n) so we have to wait.
      for_count[n].wait(mut_lock, [&] {return count[n] != 0; });
      return count[n];
    }
    else { // Result has already been computed.
      return count[n];
    }
  }

};

#endif