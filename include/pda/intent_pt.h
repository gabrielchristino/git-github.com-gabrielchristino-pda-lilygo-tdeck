#pragma once
#include <stdint.h>
#include <stddef.h>

namespace pda { namespace ai {
class IntentPt {
public:
    static constexpr int inputs = 1024, hidden = 32, labels = 9;
    static constexpr size_t modelBytes = 132420;
    static constexpr size_t perClassModelBytes = modelBytes + 72;
    struct Prediction {
        int label = 0, rawLabel = 0;
        float score = 0, margin = 0, probabilities[labels] = {};
        bool policyRejected = false;
    };
    bool begin(const uint8_t *data, size_t length);
    bool predict(const char *text, Prediction &result) const;
    static bool features(const char *text, float *output, uint32_t profile = 1);
    uint32_t featureProfile() const { return profile_; }
    static bool policyAllows(const char *text, int label, uint32_t profile = 4);
    static const char *labelName(int label);
private:
    float weight(size_t index) const;
    const uint8_t *data_ = nullptr;
    float thresholds_[labels] = {}, margins_[labels] = {};
    size_t weightsOffset_ = 32;
    uint32_t profile_ = 1;
};
} }
