#include <gtest/gtest.h>
#include <string>
#include "concurrent.h"
#include <vector>
#include <atomic>
#include <iostream>
#include <memory>
#include <chrono>
#include <iostream>
namespace {
   struct DummyObject { };

   // Verify clean destruction
   struct TrueAtExit {
      std::atomic<bool>* flag;
      explicit TrueAtExit(std::atomic<bool>* f) : flag(f) {
         flag->store(false);
      }

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
      void DoDelayedCall() {
         std::this_thread::sleep_for(std::chrono::seconds(1));
      }
   };
}

   
TEST(TestOfconcurrent, CompilerCheckForEmptyStruct) {
   concurrent<DummyObject> doNothing1{};
   concurrent<DummyObject> doNothing2;
   concurrent<DummyObject> doNothing3 = {};
}

TEST(TestOfconcurrent, CompilerCheckForString) {
   concurrent<std::string> doNothing1{"start"};
   concurrent<std::string> doNothing2 = {"start"};
}

TEST(TestOfconcurrent, VerifyDestruction) {
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

TEST(TestOfconcurrent, VerifyFifoCalls) {

   concurrent<std::string> asyncString = {"start"};
   auto received = asyncString([](std::string & s) { s.append(" received message"); return std::string{s}; });
      
   auto clear = asyncString([](std::string & s) { s.clear(); return s; });
      
   EXPECT_EQ("start received message", received.get());
   EXPECT_EQ("", clear.get());

   std::string toCompare;
   for (size_t index = 0; index < 100000; ++index) {
      toCompare.append(std::to_string(index)).append(" ");
      asyncString([ = ](std::string & s){s.append(std::to_string(index)).append(" ");});
   }
   
   auto appended = asyncString([](const std::string & s) { return s; });
   EXPECT_EQ(appended.get(), toCompare);
}

TEST(TestOfconcurrent, VerifyImmediateReturnForSlowFunctionCalls) {

   typedef std::chrono::steady_clock clock;
   auto start = clock::now();
   {
      concurrent<DelayedCaller> snail;
      for (size_t call = 0; call < 10; ++call) {
         snail([](DelayedCaller & slowRunner) { slowRunner.DoDelayedCall(); });
      }
      EXPECT_LT(std::chrono::duration_cast<std::chrono::seconds>(clock::now() - start).count(), 1);
   } // at destruction all 1 second calls will be executed before we quit
   
   EXPECT_TRUE(std::chrono::duration_cast<std::chrono::seconds>(clock::now() - start).count() >= 10); // 
}





