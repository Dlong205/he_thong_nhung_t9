#pragma once
// Mở rộng - Yaw bù nghiêng (tilt-compensated) từ mag + Roll/Pitch (rad→deg).
// mx' = mx*cosP + mz*sinP
// my' = mx*sinR*sinP + my*cosR - mz*sinR*cosP
// yaw = atan2(-my', mx'). Không lọc thêm (mag đã OSR/DSR trong chip).
float yaw_tilt(float mx, float my, float mz, float roll_deg, float pitch_deg);
float yaw_raw(float mx, float my); // không bù (để so sánh, thấy rõ sai khi nghiêng)
float yaw_wrap180(float yaw);      // về [-180,180]
