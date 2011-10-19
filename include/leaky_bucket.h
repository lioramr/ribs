#ifndef _LEAKY_BUCKET__H_
#define _LEAKY_BUCKET__H_

#include <sys/time.h>

struct leaky_bucket
{
    uint32_t max_level; // max burst size
    uint32_t max_level_secs;
    uint32_t fill_by;
    uint32_t current_level;
    
    struct timeval timestamp;

    enum
    {
        TIME_PERIOD = 1000000 // micro seconds
    };

    void init(uint32_t drain_rate, uint32_t max_level)
    {
        if (0 == drain_rate) ++drain_rate;
        this->fill_by = TIME_PERIOD / drain_rate;
        this->max_level = max_level * fill_by;
        this->max_level_secs = this->max_level / 1000000;
        this->current_level = 0;
        if (0 > gettimeofday(&timestamp, NULL))
            perror("gettimeofday");
    }

    void drain()
    {
        struct timeval now, res;
        if (0 > gettimeofday(&now, NULL))
            perror("gettimeofday");
        timersub(&now, &timestamp, &res);
        uint32_t diff;
        if (res.tv_sec > max_level_secs || (diff = (res.tv_sec * 1000000 + res.tv_usec)) >= current_level)
            current_level = 0;
        else
            current_level -= diff;
        timestamp = now;
    }

    bool fill()
    {
        drain();
        if (current_level + fill_by > max_level)
            return false;
        current_level += fill_by;
        return true;
    }

    bool fill_overflow()
    {
        drain();
        if (current_level + fill_by > max_level)
            current_level = max_level + fill_by;
        else
            current_level += fill_by;
        return true;
    }

    void set_to_max()
    {
        current_level = max_level + fill_by;
    }
};

#endif // _LEAKY_BUCKET__H_
