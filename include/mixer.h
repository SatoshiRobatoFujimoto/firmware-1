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

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include <stdint.h>
#include <stdbool.h>


// This enum needs to match the "array_of_mixers" variable in mixer.c
typedef enum
{
  QUADCOPTER_PLUS,
  QUADCOPTER_X,
  Y6,
  X8,
  FIXEDWING,
  NUM_MIXERS,
  INVALID_MIXER = 255
} mixer_type_t;

typedef enum
{
  NONE, // None
  S, // Servo
  M, // Motor
  G // GPIO
} output_type_t;

typedef struct
{
  float F;
  float x;
  float y;
  float z;
} command_t;

typedef struct
{
  output_type_t output_type[8];
  float F[8];
  float x[8];
  float y[8];
  float z[8];
} mixer_t;

extern command_t _command;

extern float _aux_command_values[8];
extern output_type_t _aux_command_type[8];

extern float _outputs[8];

void init_PWM();
void init_mixing();
void mix_output();
#ifdef __cplusplus
}
#endif
