// ---------------------------------------------------------------------------
// Host-side unit tests for the pure decision logic in MedLogic.
// No board required:   pio test -e native
// ---------------------------------------------------------------------------
#include <unity.h>
#include <string.h>
#include "MedLogic.h"

using namespace medlogic;

// --- shouldTrigger ---------------------------------------------------------

void test_trigger_when_time_matches_and_not_fired(void)
{
    MedicineSchedule s = {8, 0};
    TEST_ASSERT_TRUE(shouldTrigger(s, false, 8, 0));
}

void test_no_trigger_when_already_fired(void)
{
    MedicineSchedule s = {8, 0};
    TEST_ASSERT_FALSE(shouldTrigger(s, true, 8, 0));
}

void test_no_trigger_when_minute_differs(void)
{
    MedicineSchedule s = {8, 0};
    TEST_ASSERT_FALSE(shouldTrigger(s, false, 8, 1));
}

void test_no_trigger_when_hour_differs(void)
{
    MedicineSchedule s = {8, 0};
    TEST_ASSERT_FALSE(shouldTrigger(s, false, 9, 0));
}

// --- shouldReset -----------------------------------------------------------

void test_reset_when_hour_left(void)
{
    MedicineSchedule s = {8, 0};
    TEST_ASSERT_TRUE(shouldReset(s, 9));
}

void test_no_reset_during_scheduled_hour(void)
{
    MedicineSchedule s = {8, 0};
    TEST_ASSERT_FALSE(shouldReset(s, 8));
}

// --- isBoxOpen -------------------------------------------------------------

void test_box_open_reads_high(void)
{
    TEST_ASSERT_TRUE(isBoxOpen(REED_OPEN));   // HIGH
    TEST_ASSERT_FALSE(isBoxOpen(REED_CLOSED)); // LOW
}

// --- formatTime ------------------------------------------------------------

void test_format_time_pads_zeros(void)
{
    char buf[9];
    formatTime(buf, 9, 5, 3);
    TEST_ASSERT_EQUAL_STRING("09:05:03", buf);
}

void test_format_time_full_values(void)
{
    char buf[9];
    formatTime(buf, 23, 59, 59);
    TEST_ASSERT_EQUAL_STRING("23:59:59", buf);
}

// --- formatDate ------------------------------------------------------------

void test_format_date_pads_zeros(void)
{
    char buf[11];
    formatDate(buf, 7, 3, 2026);
    TEST_ASSERT_EQUAL_STRING("07/03/2026", buf);
}

void test_format_date_full_values(void)
{
    char buf[11];
    formatDate(buf, 31, 12, 2026);
    TEST_ASSERT_EQUAL_STRING("31/12/2026", buf);
}

// --- full-day walk: each dose fires exactly once ---------------------------
// Simulates checkSchedule() minute-by-minute over 24h against the real
// schedule and asserts every box triggers exactly one time.

void test_each_dose_fires_exactly_once_per_day(void)
{
    MedicineSchedule schedule[6] = {
        {8, 0}, {10, 0}, {12, 0}, {15, 0}, {18, 0}, {21, 0}};
    bool triggered[6] = {false, false, false, false, false, false};
    int fireCount[6] = {0, 0, 0, 0, 0, 0};

    for (int h = 0; h < 24; h++)
    {
        for (int m = 0; m < 60; m++)
        {
            for (int i = 0; i < 6; i++)
            {
                if (shouldTrigger(schedule[i], triggered[i], h, m))
                {
                    fireCount[i]++;
                    triggered[i] = true; // medicineAlert() marks it taken
                }
                if (shouldReset(schedule[i], h))
                {
                    triggered[i] = false;
                }
            }
        }
    }

    for (int i = 0; i < 6; i++)
    {
        TEST_ASSERT_EQUAL_INT_MESSAGE(1, fireCount[i],
                                      "a dose did not fire exactly once");
    }
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char **)
{
    UNITY_BEGIN();

    RUN_TEST(test_trigger_when_time_matches_and_not_fired);
    RUN_TEST(test_no_trigger_when_already_fired);
    RUN_TEST(test_no_trigger_when_minute_differs);
    RUN_TEST(test_no_trigger_when_hour_differs);

    RUN_TEST(test_reset_when_hour_left);
    RUN_TEST(test_no_reset_during_scheduled_hour);

    RUN_TEST(test_box_open_reads_high);

    RUN_TEST(test_format_time_pads_zeros);
    RUN_TEST(test_format_time_full_values);
    RUN_TEST(test_format_date_pads_zeros);
    RUN_TEST(test_format_date_full_values);

    RUN_TEST(test_each_dose_fires_exactly_once_per_day);

    return UNITY_END();
}
