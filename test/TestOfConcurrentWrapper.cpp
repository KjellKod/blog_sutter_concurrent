#include <gtest/gtest.h>
#include <string>
#include <atomic>
#include <memory>
#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <future>
#include <cassert>
#include <iostream>
#include "concurrent.hpp"

namespace {
   typedef std::chrono::steady_clock clock;

   // Random function from http://www2.research.att.com/~bs/C++0xFAQ.html#std-random
   int random_int(int low, int high) {
      using namespace std;
      static std::random_device rd; // Seed with a real random value, if available
      static default_random_engine engine{rd()};
      typedef uniform_int_distribution<int> Distribution;
      static Distribution distribution{};

      return distribution(engine, Distribution::param_type{low, high});
   }

   struct DummyObject { };



   // Verify clean destruction

   struct TrueAtExit {
      std::atomic<bool>* flag;
      explicit TrueAtExit(std::atomic<bool>* f) : flag(f) { flag->store(false); }
      bool value() { return *flag; }
      ~TrueAtExit() { flag->store(true); }
      // concurrent improvement from the original Herb Sutter example
      // Which would copy/move the object into the concurrent wrapper
      // i.e the original concurrent wrapper could not use an object such as this (or a unique_ptr for that matter)
      TrueAtExit(const TrueAtExit&) = delete;
      TrueAtExit& operator=(const TrueAtExit&) = delete;
   };



   // Verify concurrent runs,. "no" delay for the caller. 
   struct DelayedCaller {
      void DoDelayedCall() { std::this_thread::sleep_for(std::chrono::seconds(1));   }
   };

   
   // To verify that it is FIFO access
   class FlipOnce {
      std::atomic<size_t>* _stored_counter;
      std::atomic<size_t>* _stored_attempts;
      bool _is_flipped;
      size_t _counter;
      size_t _attempts;

   public:

      explicit FlipOnce(std::atomic<size_t>* c, std::atomic<size_t>* t)
      : _stored_counter(c), _stored_attempts(t)
      , _is_flipped(false), _counter(0), _attempts(0) { }

      ~FlipOnce() {
         if (0 == *_stored_counter) {
            std::cout << "FlipOnce was with no atomics in the doFlip operation " << std::endl;
            (*_stored_counter) = _counter;
            (*_stored_attempts) = _attempts;
         } else {
            std::cout << "FlipOnce was WITH atomics in the doFlipAtomic operation " << std::endl;
            assert(0==_counter);
            assert(0==_attempts);
         }
      }

      // Void flip will  count up NON ATOMIC internal variables. They are non atomic to avoid 
      // any kind of unforseen atomic synchronization. Only in the destructor will the values 
      // be saved to the atomic storages
      void doFlip() {
         if (!_is_flipped) {
            std::this_thread::sleep_for(std::chrono::milliseconds(random_int(0, 1000)));
            _is_flipped = true;
            _counter++;
         }
         _attempts++;
      }

      void doFlipAtomic() {
            if (!_is_flipped) {
            std::this_thread::sleep_for(std::chrono::milliseconds(random_int(0, 1000)));
            _is_flipped = true;
            (*_stored_counter)++;
         }
         (*_stored_attempts)++;
      }
   };
   
   
   struct Animal {  virtual std::string sound() = 0;  };
   
   struct Dog : public Animal { 
      std::string sound() override {   return  {"Wof Wof"};}
   };
   struct Cat : public Animal {
      std::string sound() override {  return  {"Miauu Miauu"};}
   };
} // anonymous

TEST(TestOfConcurrent, CompilerCheckForEmptyStruct) {
   concurrent<DummyObject> doNothing1{};
   concurrent<DummyObject> doNothing2;
   concurrent<DummyObject> doNothing3 = {};
}

TEST(TestOfConcurrent, CompilerCheckUniquePtrTest) {
   typedef std::unique_ptr<Animal> RaiiAnimal;
   concurrent<RaiiAnimal> animal1{new Dog};
   concurrent<RaiiAnimal> animal2{new Cat};

   auto make_sound = [](RaiiAnimal & animal) {
      return animal->sound();
   };
   EXPECT_EQ("Wof Wof", animal1(make_sound).get());
   EXPECT_EQ("Miauu Miauu", animal2(make_sound).get());
}



TEST(TestOfConcurrent, VerifyDestruction) {
   std::atomic<bool> flag{true};
   {
      concurrent<TrueAtExit> notifyAtExit1{&flag};
      EXPECT_FALSE(flag); // i.e. constructor has run
   }
   {
      EXPECT_TRUE(flag); // notifyAtExit destructor
      concurrent<TrueAtExit> notifyAtExit2 = {&flag};
      EXPECT_FALSE(flag);
   }
   EXPECT_TRUE(flag); // notifyAtExit destructor
}



TEST(TestOfConcurrent, VerifyFifoCalls) {

   concurrent<std::string> asyncString = {"start"};
   auto received = asyncString([](std::string & s) {
      s.append(" received message"); return std::string{s}; });

   auto clear = asyncString([](std::string & s) {
      s.clear(); return s; });

   EXPECT_EQ("start received message", received.get());
   EXPECT_EQ("", clear.get());

   std::string toCompare;
   for (size_t index = 0; index < 100000; ++index) {
      toCompare.append(std::to_string(index)).append(" ");
      asyncString([ = ](std::string & s){s.append(std::to_string(index)).append(" ");});
   }

   auto appended = asyncString([](const std::string & s) {
      return s; });
   EXPECT_EQ(appended.get(), toCompare);
}



TEST(TestOfConcurrent, VerifyImmediateReturnForSlowFunctionCalls) {
   auto start = clock::now();
   {
      concurrent<DelayedCaller> snail;
      for (size_t call = 0; call < 10; ++call) {
         snail([](DelayedCaller & slowRunner) {
            slowRunner.DoDelayedCall(); });
      }
      EXPECT_LT(std::chrono::duration_cast<std::chrono::seconds>(clock::now() - start).count(), 1);
   } // at destruction all 1 second calls will be executed before we quit

   EXPECT_TRUE(std::chrono::duration_cast<std::chrono::seconds>(clock::now() - start).count() >= 10); // 
}



std::future<void> DoAFlip(concurrent<FlipOnce>& flipper) {
   return flipper([] (FlipOnce & obj) {
      obj.doFlip(); });
}
TEST(TestOfConcurrent, IsConcurrentReallyAsyncWithFifoGuarantee__Wait1Minute) {
   auto start = clock::now();

   for (size_t howmanyflips = 0; howmanyflips < 60; ++howmanyflips) {
      std::atomic<size_t> count_of_flip{0};
      std::atomic<size_t> total_thread_access{0};
      {
         concurrent<FlipOnce> flipOnceObject{&count_of_flip, &total_thread_access};
         ASSERT_EQ(0, count_of_flip);
         auto try0 = std::async(std::launch::async, DoAFlip, std::ref(flipOnceObject));
         auto try1 = std::async(std::launch::async, DoAFlip, std::ref(flipOnceObject));
         auto try2 = std::async(std::launch::async, DoAFlip, std::ref(flipOnceObject));
         auto try3 = std::async(std::launch::async, DoAFlip, std::ref(flipOnceObject));
         auto try4 = std::async(std::launch::async, DoAFlip, std::ref(flipOnceObject));
         auto try5 = std::async(std::launch::async, DoAFlip, std::ref(flipOnceObject));
         auto try6 = std::async(std::launch::async, DoAFlip, std::ref(flipOnceObject));
         auto try7 = std::async(std::launch::async, DoAFlip, std::ref(flipOnceObject));
         auto try8 = std::async(std::launch::async, DoAFlip, std::ref(flipOnceObject));
         auto try9 = std::async(std::launch::async, DoAFlip, std::ref(flipOnceObject));

         // scope exit. ALL jobs will be executed before this finished. 
         //This means that all 10 jobs in the loop must be done
         // all 10 will wait here till they are finished
      }
      auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();
      std::cout << "Run " << howmanyflips << ", took: " << total_time << " milliseconds" << std::endl;
      start = clock::now();
      ASSERT_EQ(1, count_of_flip);
      ASSERT_EQ(10, total_thread_access);
   }
}

std::future<void> DoAFlipAtomic(concurrent<FlipOnce>& flipper) {
   return flipper([] (FlipOnce & obj) {
      obj.doFlipAtomic(); });
}
TEST(TestOfConcurrent, IsConcurrentReallyAsyncWithFifoGuarantee__AtomicInside_Wait1Minute) {
   auto start = clock::now();

   for (size_t howmanyflips = 0; howmanyflips < 60; ++howmanyflips) {
      std::atomic<size_t> count_of_flip{0};
      std::atomic<size_t> total_thread_access{0};
      {
         concurrent<FlipOnce> flipOnceObject{&count_of_flip, &total_thread_access};
         ASSERT_EQ(0, count_of_flip);
         auto try0 = std::async(std::launch::async, DoAFlipAtomic, std::ref(flipOnceObject));
         auto try1 = std::async(std::launch::async, DoAFlipAtomic, std::ref(flipOnceObject));
         auto try2 = std::async(std::launch::async, DoAFlipAtomic, std::ref(flipOnceObject));
         auto try3 = std::async(std::launch::async, DoAFlipAtomic, std::ref(flipOnceObject));
         auto try4 = std::async(std::launch::async, DoAFlipAtomic, std::ref(flipOnceObject));
         auto try5 = std::async(std::launch::async, DoAFlipAtomic, std::ref(flipOnceObject));
         auto try6 = std::async(std::launch::async, DoAFlipAtomic, std::ref(flipOnceObject));
         auto try7 = std::async(std::launch::async, DoAFlipAtomic, std::ref(flipOnceObject));
         auto try8 = std::async(std::launch::async, DoAFlipAtomic, std::ref(flipOnceObject));
         auto try9 = std::async(std::launch::async, DoAFlipAtomic, std::ref(flipOnceObject));

         // scope exit. ALL jobs will be executed before this finished. 
         //This means that all 10 jobs in the loop must be done
         // all 10 will wait here till they are finished
      }
      auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();
      std::cout << "Run 2: " << howmanyflips << ", took: " << total_time << " milliseconds" << std::endl;
      start = clock::now();
      ASSERT_EQ(1, count_of_flip);
      ASSERT_EQ(10, total_thread_access);
   }
}



