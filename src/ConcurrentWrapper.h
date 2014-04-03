#pragma once

// From a Herb Sutter Channel 9 presentation http://channel9.msdn.com/Shows/Going+Deep/C-and-Beyond-2012-Herb-Sutter-Concurrency-and-Parallelism
// ConcurrentWrapper can wrap ANY object
// All access to the Wrapper is done though a lambda. 
// All access call to the Wrapper is done asynchronously but they are executed in FIFO order
// 
// At scope exit all queued jobs has to finish before the ConcurrentWrapper goes out of scope

#include <shared_queue.hpp>
#include <thread>
#include <future>
#include <functional>

namespace concurrent {
  typedef std::function<void() > Callback;
  // ===========================================
  // Concurrent class wrapper. All access to the wrapped
  //      object goes through the ()(Func) operator
  //      all "Func" are executed ON the wrapped object
  //      Example:
  //     concurrent<string> shared_string;
  //     shared_string([](string& str){ str.append("Hello");});
  // helper for setting promise/exception
  template<typename Fut, typename F, typename T>
  void set_value(std::promise<Fut>& p, F& f, T&t) {
    p.set_value(f(t));
  }

// helper for setting promise/exception for promise of void
  template<typename F, typename T>
  void set_value(std::promise<void>& p, F& f, T&t) {
    f(t);
    p.set_value();
  }
} // namespace concurrent


  // Basically a light weight active object.
  // all input happens in the background. At shutdown it exits only after all 
  // queued requests are handled.
  template <class T> class ConcurrentWrapper {
    mutable T t;
    mutable shared_queue<concurrent::Callback> q;
    bool done = false;  // not atomic since only the bg thread is touching it
    std::thread thd;

    void run() const {
      concurrent::Callback call;
      while (!done) {
        q.wait_and_pop(call);
        call();
      }
    }

  public:
    ConcurrentWrapper(T t_) 
    : t(t_), 
     thd([ = ]{concurrent::Callback call; while (!done) { q.wait_and_pop(call); call();   }}) 
     { }

    ~ConcurrentWrapper() {
      q.push([ = ]{done = true;});
      thd.join();
    }


    template<typename F>
    auto operator()(F f) const -> std::future<decltype(f(t))> {
      auto p = std::make_shared < std::promise < decltype(f(t)) >> ();
      auto future_result = p->get_future();
      q.push([ = ]{
        try {
          concurrent::set_value(*p, f, t); }        catch (...) {
          p->set_exception(std::current_exception()); }
      });
      return future_result;
    }
  };




