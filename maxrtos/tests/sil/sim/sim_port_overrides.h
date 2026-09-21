/**
 * @file sim_port_overrides.h
 * @brief Redirects the architecture layer's hardware access points
 *        (maxrtos/arch/cortex_m7/port.h) to the virtual target.
 *
 * Force-included (-include) when compiling the architecture sources for
 * the SIL build, so the production files themselves stay untouched.
 */

#ifndef MAXRTOS_SIL_SIM_PORT_OVERRIDES_H
#define MAXRTOS_SIL_SIM_PORT_OVERRIDES_H

#include <stdint.h>

#include "sim_mpu_regs.h"

void sim_port_halt( void );
void sim_port_wfi( void );
void * sim_uaddr_to_ptr( uint32_t address );

#define MAXRTOS_PORT_HALT()  sim_port_halt()
#define MAXRTOS_PORT_WFI()   sim_port_wfi()
#define MAXRTOS_PORT_BARRIER() ( ( void ) 0 )

#define MAXRTOS_PORT_UADDR_TO_PTR( addr_ ) sim_uaddr_to_ptr( ( uint32_t ) ( addr_ ) )

#define MAXRTOS_PORT_MPU_CTRL SIM_MPU_REG_CTRL
#define MAXRTOS_PORT_MPU_RNR  SIM_MPU_REG_RNR
#define MAXRTOS_PORT_MPU_RBAR SIM_MPU_REG_RBAR
#define MAXRTOS_PORT_MPU_RASR SIM_MPU_REG_RASR
#define MAXRTOS_PORT_MPU_REG_WRITE( reg_, value_ ) \
    sim_mpu_write( ( reg_ ), ( uint32_t ) ( value_ ) )

#endif /* MAXRTOS_SIL_SIM_PORT_OVERRIDES_H */
