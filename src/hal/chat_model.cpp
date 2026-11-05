#ifdef PDA_CHAT
#include "chat_model.h"
#include "pda/intent_pt.h"
#include <stdint.h>
#include <Arduino.h>
extern const uint8_t chatStart[] asm("_binary_data_ai_intent_pt_bin_start");
extern const uint8_t chatEnd[] asm("_binary_data_ai_intent_pt_bin_end");
namespace pda { namespace hal {
bool chatClassify(const char *text,int &label) {
    ai::IntentPt model; ai::IntentPt::Prediction result;
    label=0;
    if (!model.begin(chatStart,chatEnd-chatStart) || !model.predict(text,result)) return false;
    label=result.label; return true;
}
void ptClassify(const char *text) {
    ai::IntentPt model;ai::IntentPt::Prediction result;
    const uint32_t started=micros();
    if(!model.begin(chatStart,chatEnd-chatStart)||!model.predict(text,result)) {
        Serial.println("PT INPUT ERROR");Serial.flush();Serial.println();Serial.flush();return;
    }
    Serial.printf("PT RESULT LABEL=%s RAW=%s SCORE=%.8f MARGIN=%.8f US=%u MODEL=pt-current GUARD=%u EXECUTED=0\n",
        model.labelName(result.label),model.labelName(result.rawLabel),result.score,result.margin,
        unsigned(micros()-started),unsigned(result.policyRejected));
    Serial.flush();Serial.println();Serial.flush();
}
} }
#endif
