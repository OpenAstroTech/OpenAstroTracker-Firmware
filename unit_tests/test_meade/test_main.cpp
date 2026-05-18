#include <unity.h>

void register_meade_parser_tests();
void register_meade_distance_tests();
void register_meade_extra_tests();
void register_meade_focus_tests();
void register_meade_get_tests();
void register_meade_gps_tests();
void register_meade_home_tests();
void register_meade_init_tests();
void register_meade_movement_tests();
void register_meade_quit_tests();
void register_meade_set_tests();
void register_meade_slew_rate_tests();
void register_meade_sync_tests();

void setUp(void)
{
}

void tearDown(void)
{
}

int main(int, char **)
{
    UNITY_BEGIN();

    register_meade_parser_tests();

    register_meade_get_tests();
    register_meade_set_tests();
    register_meade_quit_tests();
    register_meade_distance_tests();
    register_meade_init_tests();
    register_meade_sync_tests();
    register_meade_home_tests();
    register_meade_slew_rate_tests();
    register_meade_gps_tests();
    register_meade_focus_tests();
    register_meade_movement_tests();
    register_meade_extra_tests();

    return UNITY_END();
}