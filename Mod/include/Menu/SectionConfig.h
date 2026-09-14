#pragma once

struct SpawnConfig {
    float distanceForward;
    float distanceUp;
    float scale = 1.0f;
    bool snapToGround = true;
};

struct PreviewConfig {
    bool livePreview = false;
    bool autoRotate = false;
    float rotationSpeed = 45.0f;
};
