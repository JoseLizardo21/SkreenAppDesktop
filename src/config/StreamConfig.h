#ifndef STREAM_CONFIG_H
#define STREAM_CONFIG_H

struct StreamConfig {
    int bitrate{4000};           // kbps
    int keyframe_interval{30};   // frames between keyframes
    int encoder_speed{7};        // 1 = best quality, 7 = fastest
};

#endif
