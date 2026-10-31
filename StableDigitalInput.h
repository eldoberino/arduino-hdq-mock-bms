#pragma once

class StableDigitalInput {
public:
  explicit StableDigitalInput(int sampleCount)
  : sampleCount_(sampleCount > MaxSamples ? MaxSamples : sampleCount),
    index_(0),
    filled_(0),
    stableValue_(false)
  {}

  bool update(bool value) {
    values_[index_] = value;
    index_ = (index_ + 1) % sampleCount_;

    const bool first = values_[0];
    if (filled_ < sampleCount_) {
      ++filled_;  // return 1st value until filled
    } else {
      // return previous stable value when not all are the same
      for (int i = 1; i < sampleCount_; ++i) {
        if (values_[i] != first) {
          return stableValue_;
        }
      }
    }
    stableValue_ = first;
    return stableValue_;
  }

private:
  static constexpr int MaxSamples = 8;
  bool values_[MaxSamples]{};
  int sampleCount_;
  int index_;
  int filled_;
  bool stableValue_;
}; 

