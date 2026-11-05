#pragma once
namespace pda { namespace hal {
bool chatClassify(const char *text,int &label);
void ptClassify(const char *text); // Same model, diagnostic only; never executes.
} }
