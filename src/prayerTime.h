#ifndef PRAYER_TIME_H
#define PRAYER_TIME_H

#include <stdint.h>
#include <zephyr/device.h>

// Structure to hold complete Hijri date
typedef struct {
    int day;
    int month;
    int year;
} hijri_date_t;

// Structure to hold all prayer times
typedef struct {
    double Dhuhur;    // Dhuhr prayer time
    double Assr;      // Asr prayer time
    double Maghreb;   // Maghrib prayer time
    double sunRise;   // Sunrise time
    double sunDown;   // Sunset time
    double Ishaa;     // Isha prayer time
    double fajjir;    // Fajr prayer time
} prayer_myFloats_t;

// Function to convert Gregorian date to Julian Day
double convert_Gregor_2_Julian_Day(float d, int m, int y);

// Function to convert Gregorian date to Hijri calendar
// Parameters: D=Day, M=Month, X=Year, JD=Julian Day
// Returns: Complete Hijri date structure (day, month, year)
hijri_date_t convert_Gregor_2_Hijri_Date(float D, int M, int X, double JD);

// Function to calculate day of the week from Julian Day
// Parameter: JD=Julian Day
// Returns: Day name string (e.g., "Sunday", "Monday", etc.)
const char* day_Of_Weak(double JD);

// Astronomical calculation functions for prayer times
double degreeCorrected(long x);
double Degree_2_Radian(long double Degrees);
double Radian_2_Degree(double Rad);
double twilligt(double winkel);
double calc_asrAngle(int factor);
double calc_altitude(void);

// Function to print current date and time
void prayer_time_print_datetime(const struct device *display_dev, int16_t x, int16_t y, uint16_t text_color, uint16_t bg_color);

// Function to display Julian Day number
void prayer_time_print_julian_day(const struct device *display_dev, int16_t x, int16_t y, double julian_day, uint16_t text_color, uint16_t bg_color);

// Helper function to draw a single character (used internally)
void prayer_time_draw_character(const struct device *display_dev, char c, int16_t x, int16_t y, uint16_t color);

// Function to display Hijri date with day of week
void prayer_time_print_hijri_date(const struct device *display_dev, int16_t x, int16_t y, hijri_date_t hijri_date, const char* day_name, uint16_t text_color, uint16_t bg_color);

// Function to calculate all prayer times
prayer_myFloats_t prayerStruct(void);

// Function to determine next prayer based on current time
int get_next_prayer_index(const char* current_time, const prayer_myFloats_t* prayers);

// Function to blink LED1 for 1 minute at prayer time
void Pray_Athan(void);

#endif // PRAYER_TIME_H