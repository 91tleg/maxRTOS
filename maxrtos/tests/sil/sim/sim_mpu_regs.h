/**
 * @file sim_mpu_regs.h
 * @brief MPU register identifiers and write access for the model.
 */

#ifndef MAXRTOS_SIL_SIM_MPU_REGS_H
#define MAXRTOS_SIL_SIM_MPU_REGS_H

#include <stdint.h>

#define SIM_MPU_REG_CTRL ( 0U )
#define SIM_MPU_REG_RNR  ( 1U )
#define SIM_MPU_REG_RBAR ( 2U )
#define SIM_MPU_REG_RASR ( 3U )

/** Write an MPU register, as the driver's memory-mapped store would. */
void sim_mpu_write( uint32_t reg, uint32_t value );

#endif /* MAXRTOS_SIL_SIM_MPU_REGS_H */
