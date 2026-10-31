/**
 * @file test_mpu_isolation.c
 * @brief The MPU stops every kind of forbidden access, on real hardware.
 *
 * Unprivileged processes attempt one forbidden access each, one per stage:
 * another partition's memory (read and write), flash, privileged kernel
 * memory, a peripheral, and code execution from a non-executable region. For
 * each, the hardware must raise a MemManage fault with exactly the right CFSR
 * bits and the right fault address, nothing may reach the HardFault path, the
 * victim memory must be unchanged, the health monitor must restart the
 * offender, and the other processes must keep running.
 *
 * IPC ports are among the kernel memory: their state, wait lists and message
 * storage are in the kernel's .bss, the same region as the kernel victim
 * below, and no MPU region gives a partition access to them.
 *
 * QEMU checks that a fault is contained. Only the real MPU says which fault
 * status and address it reports.
 */

#include "hil_stages.h"

extern uint8_t __partition_control_domain_start[];
extern uint8_t __partition_application_domain_start[];

enum
{
    R_LEAK = HS_FIRST_FREE
};

#define OWNER_CONTROL     ( 0U )
#define OWNER_APPLICATION ( 1U )

typedef enum
{
    ACT_WRITE,
    ACT_READ,
    ACT_EXECUTE
} action_t;

typedef struct
{
    uint32_t owner;
    action_t action;
    volatile uint32_t * target;
    char const * name;
} stage_t;

#define FLASH_VICTIM ( ( volatile uint32_t * ) ( HIL_FLASH_BASE + 0x1000UL ) )

#define SENTINEL    ( 0x5E47157EUL )
#define BX_LR       ( 0x4770U ) /* Thumb: bx lr */
#define EXEC_OFFSET ( 64U )     /* word offset of the code stub in a domain */

#define DATA_VIOLATION ( HIL_CFSR_DACCVIOL | HIL_CFSR_MMARVALID )

static volatile uint32_t s_kernel_victim = SENTINEL;

#define CONTROL_DOMAIN                                                         \
    ( ( volatile uint32_t * ) __partition_control_domain_start )
#define APPLICATION_DOMAIN                                                     \
    ( ( volatile uint32_t * ) __partition_application_domain_start )
#define EXEC_STUB                                                              \
    ( ( volatile uint32_t * ) ( ( uint8_t * ) CONTROL_DOMAIN + EXEC_OFFSET ) )

static stage_t const k_stages[] = {
    { OWNER_CONTROL, ACT_WRITE, 0, "write other partition" },
    { OWNER_CONTROL, ACT_READ, 0, "read other partition" },
    { OWNER_CONTROL, ACT_WRITE, FLASH_VICTIM, "write flash" },
    { OWNER_CONTROL, ACT_WRITE, &s_kernel_victim, "write kernel memory" },
    { OWNER_CONTROL, ACT_WRITE, HIL_RCC_AHB4ENR, "write peripheral" },
    { OWNER_CONTROL, ACT_EXECUTE, 0, "execute from data" },
    { OWNER_APPLICATION, ACT_WRITE, 0, "write other partition" },
};

#define STAGES ( sizeof( k_stages ) / sizeof( k_stages[ 0 ] ) )

static volatile uint32_t * target_of( uint32_t stage )
{
    volatile uint32_t * target = k_stages[ stage ].target;

    /* Linker-symbol addresses are not constant expressions on every
     * toolchain, so those targets are filled in here. */
    switch( stage )
    {
        case 0U:
        case 1U:
            target = APPLICATION_DOMAIN;
            break;
        case 5U:
            target = EXEC_STUB;
            break;
        case 6U:
            target = CONTROL_DOMAIN;
            break;
        default:
            break;
    }

    return target;
}

static hs_expect_t expect_for( uint32_t stage )
{
    hs_expect_t e;

    e.cfsr = DATA_VIOLATION;
    e.mmfar = ( uint32_t ) target_of( stage );
    e.bfar = 0U;

    if( k_stages[ stage ].action == ACT_EXECUTE )
    {
        /* An instruction fetch violation does not give a fault address. */
        e.cfsr = HIL_CFSR_IACCVIOL;
        e.mmfar = 0U;
    }

    return e;
}

static uint32_t s_snapshot[ STAGES ];

static void inject( uint32_t stage )
{
    volatile uint32_t * const target = target_of( stage );

    switch( k_stages[ stage ].action )
    {
        case ACT_WRITE:
            *target = 0xBADC0DE5UL;
            break;
        case ACT_READ:
            EMU_RES[ R_LEAK ] = *target;
            break;
        case ACT_EXECUTE:
            ( ( void ( * )( void ) )( ( uint32_t ) target | 1U ) )();
            break;
        default:
            break;
    }
}

static void injector( uint32_t owner )
{
    EMU_RES
    [owner == OWNER_CONTROL ? HS_ENTRIES_CONTROL : HS_ENTRIES_APPLICATION]++;

    for( ;; )
    {
        uint32_t const stage = EMU_RES[ HS_STAGE ];

        if( ( stage < STAGES ) && ( k_stages[ stage ].owner == owner ) &&
            hs_claim( stage ) )
        {
            inject( stage );
            hs_escaped( stage );
        }

        maxrtos_yield();
    }
}

static void control_aux( void * arg )
{
    volatile uint32_t * const own = CONTROL_DOMAIN + 16U;

    ( void ) arg;

    /* An allowed access must not fault: the partition's own domain. */
    *own = 0x11111111UL;

    injector( OWNER_CONTROL );
}

static void application_aux( void * arg )
{
    volatile uint32_t * const own = APPLICATION_DOMAIN + 16U;

    ( void ) arg;

    *own = 0x22222222UL;

    injector( OWNER_APPLICATION );
}

static void control_main( void * arg )
{
    ( void ) arg;
    for( ;; )
    {
        EMU_RES[ HS_BEAT_CONTROL ]++;
        maxrtos_yield();
    }
}
static void application_main( void * arg )
{
    ( void ) arg;
    for( ;; )
    {
        EMU_RES[ HS_BEAT_APPLICATION ]++;
        maxrtos_yield();
    }
}

void emu_test_setup( void )
{
    uint32_t i;

    /* Plant the victims (privileged, before any process runs) and remember
     * what every target holds so the poll can prove it was not written. */
    CONTROL_DOMAIN[ 0 ] = SENTINEL;
    APPLICATION_DOMAIN[ 0 ] = SENTINEL;
    *( volatile uint16_t * ) EXEC_STUB = BX_LR;

    for( i = 0U; i < STAGES; i++ )
    {
        s_snapshot[ i ] = *target_of( i );
    }

    EMU_CREATE( control_main, PARTITION_ID_control, control_main );
    EMU_CREATE( control_aux, PARTITION_ID_control, control_aux );
    EMU_CREATE( application_main, PARTITION_ID_application, application_main );
    EMU_CREATE( application_aux, PARTITION_ID_application, application_aux );
}

static int verify( uint32_t stage )
{
    int result = EMU_RUNNING;

    if( ( k_stages[ stage ].action != ACT_EXECUTE ) &&
        ( *target_of( stage ) != s_snapshot[ stage ] ) )
    {
        result = hil_fail_values(
            hs_label( stage, "victim changed" ),
            *target_of( stage ),
            s_snapshot[ stage ] );
    }
    else if(
        ( k_stages[ stage ].action == ACT_READ ) &&
        ( EMU_RES[ R_LEAK ] != 0U ) )
    {
        result = hil_fail_values(
            hs_label( stage, "read leaked" ), EMU_RES[ R_LEAK ], 0U );
    }
    else
    {
        /* intact */
    }

    return result;
}

int emu_test_poll( uint32_t tick )
{
    static hs_expect_t expect[ STAGES ];
    uint32_t i;
    uint32_t control_faults = 0U;
    uint32_t application_faults = 0U;
    int result;

    if( tick == 1U )
    {
        for( i = 0U; i < STAGES; i++ )
        {
            expect[ i ] = expect_for( i );
        }
    }

    result = hs_poll( expect, STAGES, verify );

    if( result == HS_DONE )
    {
        for( i = 0U; i < STAGES; i++ )
        {
            if( k_stages[ i ].owner == OWNER_CONTROL )
            {
                control_faults++;
            }
            else
            {
                application_faults++;
            }
        }

        result =
            hs_check_containment( tick, control_faults, application_faults );
    }
    else if( result == HS_RUNNING )
    {
        result = EMU_RUNNING;
    }
    else
    {
        /* EMU_FAIL */
    }

    return result;
}
