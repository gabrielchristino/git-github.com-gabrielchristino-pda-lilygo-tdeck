#include "utils.h"
#include <WiFi.h>
#include <time.h>

namespace Utils {

void initTimeSync()
{
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    // Wait for time to be set
    time_t now = time(nullptr);
    int retry = 0;
    const int retry_count = 10;
    while (now < 8 * 3600 * 2 && retry < retry_count) {
        delay(500);
        now = time(nullptr);
        retry++;
    }
}

struct tm getCurrentTime()
{
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    return timeinfo;
}

} // namespace Utils
