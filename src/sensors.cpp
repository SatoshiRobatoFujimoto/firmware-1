/*
 * Copyright (c) 2017, James Jackson and Daniel Koch, BYU MAGICC Lab
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdbool.h>
#include <stdint.h>

#include "sensors.h"
#include "rosflight.h"

#include <turbovec.h>

namespace rosflight_firmware
{

Sensors::Sensors(ROSflight& rosflight) :
  rf_(rosflight)
{}

void Sensors::init()
{
  new_imu_data_ = false;

  // clear the IMU read error
  rf_.state_manager_.clear_error(StateManager::ERROR_IMU_NOT_RESPONDING);
  rf_.board_.sensors_init();

  // See if the IMU is uncalibrated, and throw an error if it is
  if (rf_.params_.get_param_float(PARAM_ACC_X_BIAS) == 0.0 && rf_.params_.get_param_float(PARAM_ACC_Y_BIAS) == 0.0 &&
      rf_.params_.get_param_float(PARAM_ACC_Z_BIAS) == 0.0 && rf_.params_.get_param_float(PARAM_GYRO_X_BIAS) == 0.0 &&
      rf_.params_.get_param_float(PARAM_GYRO_Y_BIAS) == 0.0 && rf_.params_.get_param_float(PARAM_GYRO_Z_BIAS) == 0.0)
  {
    rf_.state_manager_.set_error(StateManager::ERROR_UNCALIBRATED_IMU);
  }
}


bool Sensors::run(void)
{
  // First, check for new IMU data
  bool new_imu_data = update_imu();


  // Now, Look for disabled sensors while disarmed (poll every 0.5 seconds)
  // These sensors need power to respond, so they might not have been
  // detected on startup, but will be detected whenever power is applied
  // to the 5V rail.
  if (!rf_.state_manager_.state().armed)
  {
    uint32_t now = rf_.board_.clock_millis();
    if (now > (last_time_look_for_disarmed_sensors + 500))
    {
      last_time_look_for_disarmed_sensors = now;
//      if (!rf_.board_.sonar_present())
//      {
//        if (rf_.board_.sonar_check())
//        {
          //          mavlink_log_info("FOUND SONAR", NULL);
//          volatile int debug = 1;
//        }
//      }
      if (!rf_.board_.diff_pressure_present())
      {
        if (rf_.board_.diff_pressure_check())
        {
//                    mavlink_log_info("FOUND DIFF PRESS", NULL);
        }
      }
    }
  }


  // Update whatever sensors are available
  if (rf_.board_.baro_present())
  {
    rf_.board_.baro_read(&data_.baro_altitude, &data_.baro_pressure, &data_.baro_temperature);
  }

  if (rf_.board_.diff_pressure_present())
  {
    if (rf_.board_.baro_present())
    {
      rf_.board_.diff_pressure_set_atm(data_.baro_pressure);
    }
    rf_.board_.diff_pressure_read(&data_.diff_pressure, &data_.diff_pressure_temp, &data_.diff_pressure_velocity);
  }

//  if (rf_.board_.sonar_present())
//  {
//    data_._sonar_range = rf_.board_.sonar_read();
//  }

  if (rf_.board_.mag_present())
  {
    float mag[3];
    rf_.board_.mag_read(mag);
    data_.mag.x = mag[0];
    data_.mag.y = mag[1];
    data_.mag.z = mag[2];
    correct_mag();
  }

  return new_imu_data;
}




bool Sensors::start_imu_calibration(void)
{
  start_gyro_calibration();

  calibrating_acc_flag_ = true;
  rf_.params_.set_param_float(PARAM_ACC_X_BIAS, 0.0);
  rf_.params_.set_param_float(PARAM_ACC_Y_BIAS, 0.0);
  rf_.params_.set_param_float(PARAM_ACC_Z_BIAS, 0.0);
  return true;
}

bool Sensors::start_gyro_calibration(void)
{
  calibrating_gyro_flag_ = true;
  rf_.params_.set_param_float(PARAM_GYRO_X_BIAS, 0.0);
  rf_.params_.set_param_float(PARAM_GYRO_Y_BIAS, 0.0);
  rf_.params_.set_param_float(PARAM_GYRO_Z_BIAS, 0.0);
  return true;
}

bool Sensors::gyro_calibration_complete(void)
{
  return !calibrating_gyro_flag_;
}

//==================================================================
// local function definitions
bool Sensors::update_imu(void)
{
  if (rf_.board_.new_imu_data())
  {
    rf_.state_manager_.clear_error(StateManager::ERROR_IMU_NOT_RESPONDING);
    last_imu_update_ms = rf_.board_.clock_millis();
    if (!rf_.board_.imu_read_all(accel_, &data_.imu_temperature, gyro_, &data_.imu_time))
    {
      return false;
    }

    data_.accel.x = accel_[0] * rf_.params_.get_param_float(PARAM_ACCEL_SCALE);
    data_.accel.y = accel_[1] * rf_.params_.get_param_float(PARAM_ACCEL_SCALE);
    data_.accel.z = accel_[2] * rf_.params_.get_param_float(PARAM_ACCEL_SCALE);

    data_.gyro.x = gyro_[0];
    data_.gyro.y = gyro_[1];
    data_.gyro.z = gyro_[2];

    if (calibrating_acc_flag_ == true)
      calibrate_accel();
    if (calibrating_gyro_flag_)
      calibrate_gyro();

    correct_imu();
    return true;
  }
  else
  {
    // if we have lost 1000 IMU messages then something is wrong
    if (rf_.board_.clock_millis() > last_imu_update_ms + 1000)
    {
      // Tell the board to fix it
      last_imu_update_ms = rf_.board_.clock_millis();
      rf_.board_.imu_not_responding_error();

      // Indicate an IMU error
      rf_.state_manager_.set_error(StateManager::ERROR_IMU_NOT_RESPONDING);
    }
    return false;
  }
}


void Sensors::calibrate_gyro()
{
  gyro_sum_.x = 0.0f;
  gyro_sum_.y = 0.0f;
  gyro_sum_.z = 0.0f;
  gyro_sum_ = vector_add(gyro_sum_, data_.gyro);
  gyro_calibration_count_++;

  if (gyro_calibration_count_ > 100)
  {
    // Gyros are simple.  Just find the average during the calibration
    vector_t gyro_bias = scalar_multiply(1.0/(float)gyro_calibration_count_, gyro_sum_);

    if (norm(gyro_bias) < 1.0)
    {
      rf_.params_.set_param_float(PARAM_GYRO_X_BIAS, gyro_bias.x);
      rf_.params_.set_param_float(PARAM_GYRO_Y_BIAS, gyro_bias.y);
      rf_.params_.set_param_float(PARAM_GYRO_Z_BIAS, gyro_bias.z);

      // Tell the estimator to reset it's bias estimate, because it should be zero now
      rf_.estimator_.reset_adaptive_bias();

      // Tell the state manager that we just completed a gyro calibration
      rf_.state_manager_.set_event(StateManager::EVENT_CALIBRATION_COMPLETE);
    }
    else
    {
      // Tell the state manager that we just failed a gyro calibration
      rf_.state_manager_.set_event(StateManager::EVENT_CALIBRATION_FAILED);
      //      mavlink_log_error("Too much movement for gyro cal", NULL);
    }

    // reset calibration in case we do it again
    calibrating_gyro_flag_ = false;
    gyro_calibration_count_ = 0;
    gyro_sum_.x = 0.0f;
    gyro_sum_.y = 0.0f;
    gyro_sum_.z = 0.0f;
  }
}

vector_t vector_max(vector_t a, vector_t b)
{
  vector_t out = {a.x > b.x ? a.x : b.x,
                  a.y > b.y ? a.y : b.y,
                  a.z > b.z ? a.z : b.z
                 };
  return out;
}

vector_t vector_min(vector_t a, vector_t b)
{
  vector_t out = {a.x < b.x ? a.x : b.x,
                  a.y < b.y ? a.y : b.y,
                  a.z < b.z ? a.z : b.z
                 };
  return out;
}


void Sensors::calibrate_accel(void)
{
  acc_sum_ = vector_add(vector_add(acc_sum_, data_.accel), gravity_);
  acc_temp_sum_ += data_.imu_temperature;
  max_ = vector_max(max_, data_.accel);
  min_ = vector_min(min_, data_.accel);
  accel_calibration_count_++;

  if (accel_calibration_count_ > 1000)
  {
    // The temperature bias is calculated using a least-squares regression.
    // This is computationally intensive, so it is done by the onboard computer in
    // fcu_io and shipped over to the flight controller.
    vector_t accel_temp_bias =
    {
      rf_.params_.get_param_float(PARAM_ACC_X_TEMP_COMP),
      rf_.params_.get_param_float(PARAM_ACC_Y_TEMP_COMP),
      rf_.params_.get_param_float(PARAM_ACC_Z_TEMP_COMP)
    };

    // Figure out the proper accel bias.
    // We have to consider the contribution of temperature during the calibration,
    // Which is why this line is so confusing. What we are doing, is first removing
    // the contribution of temperature to the measurements during the calibration,
    // Then we are dividing by the number of measurements.
    vector_t accel_bias = scalar_multiply(1.0/(float)accel_calibration_count_, vector_sub(acc_sum_,
                                          scalar_multiply(acc_temp_sum_, accel_temp_bias)));

    // Sanity Check -
    // If the accelerometer is upside down or being spun around during the calibration,
    // then don't do anything
    if (norm(vector_sub(max_, min_)) > 1.0)
    {
//      mavlink_log_error("Too much movement for IMU cal", NULL);
      calibrating_acc_flag_ = false;
    }
    else
    {
      if (norm(accel_bias) < 3.0)
      {
        rf_.params_.set_param_float(PARAM_ACC_X_BIAS, accel_bias.x);
        rf_.params_.set_param_float(PARAM_ACC_Y_BIAS, accel_bias.y);
        rf_.params_.set_param_float(PARAM_ACC_Z_BIAS, accel_bias.z);
//        mavlink_log_info("IMU offsets captured", NULL);

        // clear uncalibrated IMU flag
        rf_.state_manager_.clear_error(StateManager::ERROR_UNCALIBRATED_IMU);

        // reset the estimated state
        rf_.estimator_.reset_state();
        calibrating_acc_flag_ = false;
      }
      else
      {
        // check for bad _accel_scale
        if (norm(accel_bias) > 3.0 && norm(accel_bias) < 6.0)
        {
//          mavlink_log_error("Detected bad IMU accel scale value", 0);
          rf_.params_.set_param_float(PARAM_ACCEL_SCALE, 2.0 * rf_.params_.get_param_float(PARAM_ACCEL_SCALE));
          rf_.params_.write();
        }
        else if (norm(accel_bias) > 6.0)
        {
//          mavlink_log_error("Detected bad IMU accel scale value", 0);
          rf_.params_.set_param_float(PARAM_ACCEL_SCALE, 0.5 * rf_.params_.get_param_float(PARAM_ACCEL_SCALE));
          rf_.params_.write();
        }
        else
        {

        }
      }
    }

    // reset calibration counters in case we do it again
    accel_calibration_count_ = 0;
    acc_sum_.x = 0.0f;
    acc_sum_.y = 0.0f;
    acc_sum_.z = 0.0f;
    acc_temp_sum_ = 0.0f;
    max_.x = -1000.0f;
    max_.y = -1000.0f;
    max_.z = -1000.0f;
    min_.x = 1000.0f;
    min_.y = 1000.0f;
    min_.z = 1000.0f;
  }
}

void Sensors::correct_imu(void)
{
  // correct according to known biases and temperature compensation
  data_.accel.x -= rf_.params_.get_param_float(PARAM_ACC_X_TEMP_COMP)*data_.imu_temperature + rf_.params_.get_param_float(
                PARAM_ACC_X_BIAS);
  data_.accel.y -= rf_.params_.get_param_float(PARAM_ACC_Y_TEMP_COMP)*data_.imu_temperature + rf_.params_.get_param_float(
                PARAM_ACC_Y_BIAS);
  data_.accel.z -= rf_.params_.get_param_float(PARAM_ACC_Z_TEMP_COMP)*data_.imu_temperature + rf_.params_.get_param_float(
                PARAM_ACC_Z_BIAS);

  data_.gyro.x -= rf_.params_.get_param_float(PARAM_GYRO_X_BIAS);
  data_.gyro.y -= rf_.params_.get_param_float(PARAM_GYRO_Y_BIAS);
  data_.gyro.z -= rf_.params_.get_param_float(PARAM_GYRO_Z_BIAS);
}

void Sensors::correct_mag(void)
{
  // correct according to known hard iron bias
  float mag_hard_x = data_.mag.x - rf_.params_.get_param_float(PARAM_MAG_X_BIAS);
  float mag_hard_y = data_.mag.y - rf_.params_.get_param_float(PARAM_MAG_Y_BIAS);
  float mag_hard_z = data_.mag.z - rf_.params_.get_param_float(PARAM_MAG_Z_BIAS);

  // correct according to known soft iron bias - converts to nT
  data_.mag.x = rf_.params_.get_param_float(PARAM_MAG_A11_COMP)*mag_hard_x + rf_.params_.get_param_float(
             PARAM_MAG_A12_COMP)*mag_hard_y +
           rf_.params_.get_param_float(PARAM_MAG_A13_COMP)*mag_hard_z;
  data_.mag.y = rf_.params_.get_param_float(PARAM_MAG_A21_COMP)*mag_hard_x + rf_.params_.get_param_float(
             PARAM_MAG_A22_COMP)*mag_hard_y +
           rf_.params_.get_param_float(PARAM_MAG_A23_COMP)*mag_hard_z;
  data_.mag.z = rf_.params_.get_param_float(PARAM_MAG_A31_COMP)*mag_hard_x + rf_.params_.get_param_float(
             PARAM_MAG_A32_COMP)*mag_hard_y +
           rf_.params_.get_param_float(PARAM_MAG_A33_COMP)*mag_hard_z;
}

} // namespace rosflight_firmware
