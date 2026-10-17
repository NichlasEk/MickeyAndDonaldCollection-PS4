#pragma once
#include <cstdint>
// Called after acquire, before any render commands target this image.
void ps4_wait_image_reusable(uint32_t index);
void ps4_check_present();
