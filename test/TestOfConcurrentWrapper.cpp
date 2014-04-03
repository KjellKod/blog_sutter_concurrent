#include <gtest/gtest.h>
#include <string>
#include "ConcurrentWrapper.h"
#include <vector>
#include <atomic>
#include <iostream>
#include <memory>

namespace {
  struct DummyObject {};

  struct TrueAtExit {
   std::atomic<bool>* flag;
   explicit TrueAtExit(std::atomic<bool>* f) : flag(f){
      flag->store(false);
   }
   
   bool value() { return *flag; }
   
   ~TrueAtExit() {
      flag->store(true);
   }
   
   //TrueAtExit(const TrueAtExit&) = delete;
   
};
}

TEST(TestOfConcurrentWrapper, InitializeWithNothingReally) {
   ConcurrentWrapper<DummyObject> doNothing1{{}};
}



/*
TEST(TestOfConcurrentWrapper, InitializeWithNothingReally) {
   ConcurrentWrapper<std::string> doNothing1 = {"start"};
   ConcurrentWrapper<std::string> doNothing2{"start"};
   ConcurrentWrapper<DummyObject> doNothing3{};
   ConcurrentWrapper<DummyObject> doNothing4 = {};
   ConcurrentWrapper<std::vector<int>> doNothing5;

   {
     std::atomic<bool> flag;
     ConcurrentWrapper<TrueAtExit> notifyAtExit1{TrueAtExit{&flag}};
     ConcurrentWrapper<TrueAtExit> notifyAtExit2 = {TrueAtExit{&flag}};
    }
}


TEST(TestOfConcurrentWrapper, SimpleCall) {

  ConcurrentWrapper<std::string> asyncString = {"start"};
  auto receivedFuture = asyncString([](std::string& s){ s.append(" received message"); return std::string{s}; });
  EXPECT_EQ("start received message", receivedFuture.get());
}



TEST(TestOfConcurrentWrapper, InitializeAndVerifyDestruction) {

   std::atomic<bool> flag{true};
   {
     ConcurrentWrapper<std::unique_ptr<TrueAtExit>> notifyAtExit{std::unique_ptr<TrueAtExit>{ new TrueAtExit{&flag}}};
     EXPECT_FALSE(flag);
     auto futureValue = notifyAtExit([](std::unique_ptr<TrueAtExit>& obj) { return obj->value(); } );
     EXPECT_FALSE(futureValue.get());
   }
   EXPECT_TRUE(flag);
} */

