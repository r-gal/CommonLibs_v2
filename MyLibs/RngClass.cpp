 #include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "GeneralConfig.h"

#if CONF_USE_RNG == 1

#include "RngClass.hpp"

#include "rng.h"

extern RNG_HandleTypeDef hrng;

uint32_t RngUnit_c::GetRandomVal(void)
{
  uint32_t value;
  HAL_StatusTypeDef res = HAL_RNG_GenerateRandomNumber(&hrng, &value);
  if(res != HAL_OK)
  {
    printf(" Rng error\n");
  }
  return value;
}

#endif