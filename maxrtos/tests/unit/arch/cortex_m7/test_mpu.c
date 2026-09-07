/**
 * @file test_mpu.c
 * @brief Unit tests for the MPU region configuration module.
 *
 * No MPU register is accessed. This module tests only the configuration
 * representation, validation, and access functions.
 */

#include <assert.h>
#include <stdio.h>

#include "maxrtos/arch/cortex_m7/mpu.h"

static void test_config_init_rejects_null( void )
{
    assert( maxrtos_mpu_config_init( NULL ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_config_init_rejects_null: PASS\n" );
}

static void test_config_init_succeeds_and_marks_all_unconfigured( void )
{
    maxrtos_mpu_config_t config;
    maxrtos_mpu_region_config_t region;
    maxrtos_partition_id_t p;

    assert( maxrtos_mpu_config_init( &config ) == MAXRTOS_OK );

    for( p = 0U; p < MAXRTOS_MAX_PARTITIONS; p++ )
    {
        assert( maxrtos_mpu_get_partition_region(
                    &config,
                    p,
                    &region ) == MAXRTOS_ERR_NOT_FOUND );
    }

    printf( "test_config_init_succeeds_and_marks_all_unconfigured: PASS\n" );
}

static void test_set_region_rejects_bad_args( void )
{
    maxrtos_mpu_config_t config;

    assert( maxrtos_mpu_config_init( &config ) == MAXRTOS_OK );

    assert( maxrtos_mpu_set_partition_region(
                NULL,
                0U,
                0x20000000UL,
                1024UL,
                MAXRTOS_MPU_ACCESS_READ_WRITE,
                false ) == MAXRTOS_ERR_INVALID_ARG );

    assert( maxrtos_mpu_set_partition_region(
                &config,
                MAXRTOS_MAX_PARTITIONS,
                0x20000000UL,
                1024UL,
                MAXRTOS_MPU_ACCESS_READ_WRITE,
                false ) == MAXRTOS_ERR_INVALID_ARG );

    /* Below the architectural minimum region size. */
    assert( maxrtos_mpu_set_partition_region(
                &config,
                0U,
                0x20000000UL,
                16UL,
                MAXRTOS_MPU_ACCESS_READ_WRITE,
                false ) == MAXRTOS_ERR_INVALID_ARG );

    /* Not a power of two. */
    assert( maxrtos_mpu_set_partition_region(
                &config,
                0U,
                0x20000000UL,
                1000UL,
                MAXRTOS_MPU_ACCESS_READ_WRITE,
                false ) == MAXRTOS_ERR_INVALID_ARG );

    /* Misaligned base address. */
    assert( maxrtos_mpu_set_partition_region(
                &config,
                0U,
                0x20000001UL,
                1024UL,
                MAXRTOS_MPU_ACCESS_READ_WRITE,
                false ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_set_region_rejects_bad_args: PASS\n" );
}

static void test_set_region_accepts_valid_power_of_two_sizes( void )
{
    maxrtos_mpu_config_t config;

    assert( maxrtos_mpu_config_init( &config ) == MAXRTOS_OK );

    /* Exercise the architectural minimum region size. */
    assert( maxrtos_mpu_set_partition_region(
                &config,
                0U,
                0x20000000UL,
                32UL,
                MAXRTOS_MPU_ACCESS_READ_WRITE,
                false ) == MAXRTOS_OK );

    assert( maxrtos_mpu_set_partition_region(
                &config,
                1U,
                0x20001000UL,
                64UL,
                MAXRTOS_MPU_ACCESS_READ_WRITE,
                false ) == MAXRTOS_OK );

    assert( maxrtos_mpu_set_partition_region(
                &config,
                2U,
                0x20002000UL,
                4096UL,
                MAXRTOS_MPU_ACCESS_READ_ONLY,
                true ) == MAXRTOS_OK );

    printf( "test_set_region_accepts_valid_power_of_two_sizes: PASS\n" );
}

static void test_set_then_get_roundtrips_all_fields_correctly( void )
{
    maxrtos_mpu_config_t config;
    maxrtos_mpu_region_config_t region;

    assert( maxrtos_mpu_config_init( &config ) == MAXRTOS_OK );

    assert( maxrtos_mpu_set_partition_region(
                &config,
                3U,
                0x24000000UL,
                8192UL,
                MAXRTOS_MPU_ACCESS_READ_WRITE,
                false ) == MAXRTOS_OK );

    assert( maxrtos_mpu_get_partition_region(
                &config,
                3U,
                &region ) == MAXRTOS_OK );

    assert( region.base_address == 0x24000000UL );
    assert( region.size_bytes == 8192UL );
    assert( region.access == MAXRTOS_MPU_ACCESS_READ_WRITE );
    assert( region.executable == false );

    printf( "test_set_then_get_roundtrips_all_fields_correctly: PASS\n" );
}

static void test_get_region_rejects_bad_args( void )
{
    maxrtos_mpu_config_t config;
    maxrtos_mpu_region_config_t region;

    assert( maxrtos_mpu_config_init( &config ) == MAXRTOS_OK );

    assert( maxrtos_mpu_get_partition_region(
                NULL,
                0U,
                &region ) == MAXRTOS_ERR_INVALID_ARG );

    assert( maxrtos_mpu_get_partition_region(
                &config,
                0U,
                NULL ) == MAXRTOS_ERR_INVALID_ARG );

    assert( maxrtos_mpu_get_partition_region(
                &config,
                MAXRTOS_MAX_PARTITIONS,
                &region ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_get_region_rejects_bad_args: PASS\n" );
}

static void test_set_region_does_not_affect_other_partitions( void )
{
    maxrtos_mpu_config_t config;
    maxrtos_mpu_region_config_t region;

    assert( maxrtos_mpu_config_init( &config ) == MAXRTOS_OK );

    assert( maxrtos_mpu_set_partition_region(
                &config,
                0U,
                0x20000000UL,
                1024UL,
                MAXRTOS_MPU_ACCESS_READ_WRITE,
                false ) == MAXRTOS_OK );

    /* Partition 1 must remain unconfigured. */
    assert( maxrtos_mpu_get_partition_region(
                &config,
                1U,
                &region ) == MAXRTOS_ERR_NOT_FOUND );

    printf( "test_set_region_does_not_affect_other_partitions: PASS\n" );
}

int main( void )
{
    test_config_init_rejects_null();
    test_config_init_succeeds_and_marks_all_unconfigured();
    test_set_region_rejects_bad_args();
    test_set_region_accepts_valid_power_of_two_sizes();
    test_set_then_get_roundtrips_all_fields_correctly();
    test_get_region_rejects_bad_args();
    test_set_region_does_not_affect_other_partitions();

    printf( "all mpu config tests passed\n" );

    return 0;
}
