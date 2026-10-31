#pragma once

class MovingAverage {
public:
  explicit MovingAverage(int windowSize)
  : windowSize_(windowSize > MaxSize ? MaxSize : windowSize),
    index_(0),
    count_(0),
    sum_(0.0f)
  {}

  float update(float value) {
    sum_ -= values_[index_];
    values_[index_] = value;
    sum_ += value;

    index_ = (index_ + 1) % windowSize_;
    if (count_ < windowSize_) {
      ++count_;
    }

    return sum_ / count_;
  }

  float value() const {
    return count_ == 0 ? 0.0f : sum_ / count_;
  }

private:
  static constexpr int MaxSize = 8;
  float values_[MaxSize]{};
  int windowSize_;
  int index_;
  int count_;
  float sum_;
};

