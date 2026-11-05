#include "pda/intent_pt.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <vector>
#include <string>

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    std::ifstream file(argv[1], std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), {});
    pda::ai::IntentPt model;
    if (!model.begin(data.data(), data.size())) return 3;
    std::cout << std::setprecision(9);
    std::string text;
    while (std::getline(std::cin, text)) {
        pda::ai::IntentPt::Prediction prediction;
        float features[pda::ai::IntentPt::inputs];
        if (!model.predict(text.c_str(), prediction) || !model.features(text.c_str(), features, model.featureProfile())) {
            std::cout << "{\"error\":\"INPUT\"}\n"; continue;
        }
        std::cout << "{\"predicted\":\"" << model.labelName(prediction.label) << "\",\"score\":" << prediction.score
                  << ",\"margin\":" << prediction.margin << ",\"guard_rejected\":" << (prediction.policyRejected ? "true" : "false") << ",\"probabilities\":[";
        for (int i = 0; i < model.labels; ++i) std::cout << (i ? "," : "") << prediction.probabilities[i];
        std::cout << "],\"features\":[";
        for (int i = 0; i < model.inputs; ++i) std::cout << (i ? "," : "") << features[i];
        std::cout << "]}\n";
    }
}
