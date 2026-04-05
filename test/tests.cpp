// Copyright 2021 GHA Test Team

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <cstdint>
#include <thread>
#include <chrono>
#include <stdexcept>

#include "TimedDoor.h"

using ::testing::_;
using ::testing::Mock;

class MockTimer : public TimerClient {
 public:
  MOCK_METHOD(void, Timeout, (), (override));
};

class MockDoor : public Door {
 public:
  MOCK_METHOD(void, lock, (), (override));
  MOCK_METHOD(void, unlock, (), (override));
  MOCK_METHOD(bool, isDoorOpened, (), (override));
};

TEST(MockDoorTest, MockDoorVerifiesLockUnlockAndStateCalls) {
  MockDoor mockDoor;

  EXPECT_CALL(mockDoor, lock()).Times(testing::Exactly(1));
  EXPECT_CALL(mockDoor, unlock()).Times(testing::Exactly(1));
  EXPECT_CALL(mockDoor, isDoorOpened()).Times(testing::AtLeast(2))
      .WillOnce(testing::Return(false))
      .WillOnce(testing::Return(true));

  mockDoor.lock();
  EXPECT_FALSE(mockDoor.isDoorOpened());
  mockDoor.unlock();
  EXPECT_TRUE(mockDoor.isDoorOpened());
}

// ===============DOOR_TEST===============
class DoorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    tDoor = new TimedDoor(2);
  }

  void TearDown() override {
    delete tDoor;
  }

  TimedDoor* tDoor;
};

TEST_F(DoorTest, TimedDoorInitializesWithCorrectTimeoutAndClosedState) {
  EXPECT_EQ(tDoor->getTimeOut(), 2);
  EXPECT_FALSE(tDoor->isDoorOpened());
}

TEST_F(DoorTest, UnlockChangesDoorStateToOpened) {
  tDoor->unlock();
  EXPECT_TRUE(tDoor->isDoorOpened());
  tDoor->lock();
  EXPECT_FALSE(tDoor->isDoorOpened());
}

TEST_F(DoorTest, LockChangesDoorStateToClosed) {
  tDoor->lock();
  EXPECT_FALSE(tDoor->isDoorOpened());
  tDoor->unlock();
  EXPECT_TRUE(tDoor->isDoorOpened());
}

TEST_F(DoorTest, ThrowStateThrowsRuntimeError) {
  EXPECT_THROW(tDoor->throwState(), std::runtime_error);
}

TEST_F(DoorTest, TimerThrowsExceptionWhenDoorRemainsOpenAfterTimeout) {
  Timer timer;
  tDoor->unlock();
  EXPECT_TRUE(tDoor->isDoorOpened());

  DoorTimerAdapter adapter(*tDoor);

  EXPECT_THROW({
    timer.tregister(tDoor->getTimeOut(), &adapter);
  }, std::runtime_error);
}

TEST_F(DoorTest, TimerDoesNotThrowExceptionWhenDoorIsClosed) {
  Timer timer;
  tDoor->lock();
  EXPECT_FALSE(tDoor->isDoorOpened());

  DoorTimerAdapter adapter(*tDoor);

  EXPECT_NO_THROW({
    timer.tregister(tDoor->getTimeOut(), &adapter);
  });
}

TEST_F(DoorTest, DoorCanHandleMultipleLockUnlockCycles) {
  for (int i = 0; i < 5; i++) {
    tDoor->unlock();
    EXPECT_TRUE(tDoor->isDoorOpened());
    tDoor->lock();
    EXPECT_FALSE(tDoor->isDoorOpened());
  }
}

TEST_F(DoorTest, TimedDoorCanBeCreatedWithDifferentTimeoutValues) {
  int timeout = 5;
  TimedDoor* tDoor2 = new TimedDoor(timeout);
  EXPECT_EQ(tDoor2->getTimeOut(), timeout);
  delete tDoor2;
}

TEST_F(DoorTest, NoExceptionThrownWhenDoorClosedBeforeTimeoutExpires) {
  Timer timer;
  tDoor->unlock();
  EXPECT_TRUE(tDoor->isDoorOpened());

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  tDoor->lock();
  EXPECT_FALSE(tDoor->isDoorOpened());

  DoorTimerAdapter adapter(*tDoor);
  EXPECT_NO_THROW({
    timer.tregister(tDoor->getTimeOut(), &adapter);
  });
}

TEST_F(DoorTest, NoExceptionThrownWhenDoorClosedImmediatelyAfterOpening) {
  Timer timer;
  DoorTimerAdapter adapter(*tDoor);

  tDoor->unlock();
  tDoor->lock();

  EXPECT_NO_THROW({
    timer.tregister(tDoor->getTimeOut(), &adapter);
  });
}

// ===============ADAPTER_TEST===============
class AdapterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    tDoor = new TimedDoor(3);
    adapter = new DoorTimerAdapter(*tDoor);
  }

  void TearDown() override {
    delete adapter;
    delete tDoor;
  }

  TimedDoor* tDoor;
  DoorTimerAdapter* adapter;
};

TEST_F(AdapterTest, AdapterTimeoutThrowsExceptionWhenDoorIsOpened) {
  tDoor->unlock();
  EXPECT_TRUE(tDoor->isDoorOpened());
  EXPECT_THROW(adapter->Timeout(), std::runtime_error);
}

TEST_F(AdapterTest, AdapterTimeoutDoesNotThrowExceptionWhenDoorIsClosed) {
  tDoor->lock();
  EXPECT_FALSE(tDoor->isDoorOpened());
  EXPECT_NO_THROW(adapter->Timeout());
}

TEST(TimedDoorMultipleAdaptersTest, MultipleAdaptersShareSameDoorState) {
  TimedDoor door(2);
  door.unlock();

  DoorTimerAdapter adapter1(door);
  DoorTimerAdapter adapter2(door);

  EXPECT_TRUE(door.isDoorOpened());

  door.lock();
  EXPECT_FALSE(door.isDoorOpened());

  EXPECT_NO_THROW(adapter1.Timeout());
  EXPECT_NO_THROW(adapter2.Timeout());
}

// ===============TIMER_TEST===============
class TimerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    timer = new Timer();
    mockTimer = new MockTimer();
  }

  void TearDown() override {
    delete timer;
    delete mockTimer;
  }

  Timer* timer;
  MockTimer* mockTimer;
};

TEST_F(TimerTest, TimerCallsTimeoutOnRegisteredClientAfterDelay) {
  EXPECT_CALL(*mockTimer, Timeout()).Times(1);
  timer->tregister(0, mockTimer);
}

TEST_F(TimerTest, TimerHandlesNullClientWithoutCrashing) {
  EXPECT_NO_THROW(timer->tregister(1, nullptr));
}
