#include "prayerTime.h"
#ifdef USE_NEO6M_GPS
    #include "gps_neo6m.h"
#else
    #include "gps_neo7m.h"
#endif
#include <zephyr/kernel.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <stdio.h>
#include <math.h>

// External variables from main.c
extern double Lng, Lat, D;

// Prayer time calculation constants
// Sun altitude at Fajr and Isha
float fajr_Angle = 18;
float isha_Angle = 17;
// Factor of shadow length at Asr
float asr_Angle_factor = 1;
// Sun's declination
int TimeZone = 1;  // Default UTC+1, will be auto-configured from GPS

// Need M_PI constant for calculations
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Global variable to store current Julian Day
static double current_JulianDay = 0.0;


// Convert Gregorian date to Julian Day
double convert_Gregor_2_Julian_Day(float d, int m, int y) {
    printk("[JD CALC] Input: day=%.1f, month=%d, year=%d\n", d, m, y);

    if (m <= 2) {
        m = m + 12;
        y = y - 1;
    }

    int A = (int)floor(y / 100.0);
    int B = 2 - A + (int)floor(A / 4.0);
    current_JulianDay = floor(365.25 * (y + 4716)) + floor(30.6001 * (m + 1)) + d + B - 1524.5;

    printk("[JD CALC] Result JD: %.1f\n", current_JulianDay);
    return current_JulianDay;
}

// Convert Gregorian date to Hijri calendar
hijri_date_t convert_Gregor_2_Hijri_Date(float D, int M, int X, double JD) {
    int Ba, w;
    if(M < 3) {
        M += 12;
        X -= 1;
    }

    int a = (int)floor(X / 100.0);

    if(JD < 2299161) {
        Ba = (int)JD;
    } else {
        Ba = 2 - a + (int)floor(a / 4.0);
    }

    long b = (long)floor(365.25 * X) + (long)floor(30.6001 * (M + 1)) + (long)D + 1722519 + Ba;

    long c = (long)floor((b - 122.1) / 365.25);
    
    if(M > 2) {
        X = c - 4716;
    } else if (M < 3) {
        X = c - 4715;
    }

    long d = (long)floor(365.25 * c);

    long e = (long)floor((b - d) / 30.6001);
    
    if (e < 14) {
        M = (int)(e - 1);
    } else if (e > 13) {
        M = (int)(e - 13);
    }

    D = (float)(b - d - (long)floor(30.6001 * e));

    int reminderOfDiv = X % 4;
    if(reminderOfDiv == 0) {
        w = 1;
    } else {
        w = 2;
    }

    int N = (int)floor((275 * M) / 9.0) - (int)floor((M + 9) / 12.0) * w + (int)D - 30;

    int A = X - 623;
    
    int B = (int)floor(A / 4.0);

    int C = A % 4;
    
    float C1 = 365.2501f * C;
    
    int C2 = (int)floor(C1);

    float Co = C1 - C2;
    
    if(Co > 0.5f) {
        C2 += 1;
    }

    uint64_t D1 = 1461 * (uint64_t)B + 170 + C2;
    
    int Q = (int)floor((double)D1 / 10631.0);
    
    int R = (int)(D1 % 10631);
    
    int J = (int)floor(R / 354.0);

    int K = R % 354;
    
    int O = (int)floor((11 * J + 14) / 30.0);
    
    int H = 30 * Q + J + 1;  // Hijri Year
    
    int JJ = K - O + N - 1;  // Day number in hijri year

    // Handle leap year or common year
    if(JJ > 354) {
        int CL = H % 30;
        
        float DL = (float)((11 * CL + 3) % 30);
        
        if(DL < 19) {
            JJ -= 354;
            H += 1;
        } else if(DL > 18) {
            JJ -= 355;
            H += 1;
        }
    } else if(JJ == 0) {
        JJ = 355;
        H -= 1;
    }

    // Convert day of year to day and month
    int S = (int)floor((JJ - 1) / 29.5);
    
    int month_Hijri = 1 + S;
    int day_Hijri = (int)floor(JJ - (29.5 * S));
    
    printk("Hijri Date: %d/%d/%d\n", day_Hijri, month_Hijri, H);
    
    // Create and return the complete Hijri date structure
    hijri_date_t hijri_date = {
        .day = day_Hijri,
        .month = month_Hijri,
        .year = H
    };
    
    return hijri_date;
}

// ============================================================================
// NEW: Tabular Islamic Calendar Algorithm (more accurate)
// Based on the Kuwaiti algorithm / standard arithmetical scheme
// Reference: https://webspace.science.uu.nl/~gent0113/islam/islam_tabcal.htm
// ============================================================================

// Hijri calendar epoch: July 16, 622 CE (Julian) = JD 1948439.5
#define HIJRI_EPOCH 1948439.5

// 30-year cycle: 19 common years (354 days) + 11 leap years (355 days) = 10631 days
#define DAYS_PER_30_YEAR_CYCLE 10631

// Check if a Hijri year is a leap year (has 355 days instead of 354)
// Leap years in 30-year cycle: 2, 5, 7, 10, 13, 16, 18, 21, 24, 26, 29
static bool is_hijri_leap_year_tabular(int year) {
    int y = ((year - 1) % 30) + 1;  // Position in 30-year cycle (1-30)
    return (y == 2 || y == 5 || y == 7 || y == 10 || y == 13 ||
            y == 16 || y == 18 || y == 21 || y == 24 || y == 26 || y == 29);
}

// Get number of days in a Hijri month (Tabular calendar)
// Odd months (1,3,5,7,9,11) = 30 days
// Even months (2,4,6,8,10) = 29 days
// Month 12 = 29 days (common year) or 30 days (leap year)
static int hijri_month_days_tabular(int month, int year) {
    if (month == 12) {
        return is_hijri_leap_year_tabular(year) ? 30 : 29;
    }
    return (month % 2 == 1) ? 30 : 29;
}

// Convert Julian Day to Hijri date using Tabular Islamic Calendar
hijri_date_t convert_JD_to_Hijri_Tabular(double JD) {
    // Days since Hijri epoch
    long days_since_epoch = (long)floor(JD - HIJRI_EPOCH);

    // Handle dates before the Hijri epoch
    if (days_since_epoch < 0) {
        printk("Hijri (Tabular): Invalid (before epoch)\n");
        return (hijri_date_t){.day = 1, .month = 1, .year = 1};
    }

    // Calculate 30-year cycles elapsed
    int cycles = days_since_epoch / DAYS_PER_30_YEAR_CYCLE;
    int remaining_days = days_since_epoch % DAYS_PER_30_YEAR_CYCLE;

    // Find year within cycle
    int year_in_cycle = 0;
    int days_counted = 0;

    for (int y = 1; y <= 30; y++) {
        int year_days = (y == 2 || y == 5 || y == 7 || y == 10 || y == 13 ||
                         y == 16 || y == 18 || y == 21 || y == 24 || y == 26 || y == 29) ? 355 : 354;
        if (days_counted + year_days > remaining_days) {
            year_in_cycle = y;
            remaining_days -= days_counted;
            break;
        }
        days_counted += year_days;
    }

    // Calculate Hijri year (1-based)
    int hijri_year = cycles * 30 + year_in_cycle;

    // Find month and day from remaining days (0-based day count within year)
    int day_of_year = remaining_days + 1;  // Convert to 1-based
    int hijri_month = 1;
    int hijri_day = day_of_year;

    for (int m = 1; m <= 12; m++) {
        int month_len = hijri_month_days_tabular(m, hijri_year);
        if (hijri_day <= month_len) {
            hijri_month = m;
            break;
        }
        hijri_day -= month_len;
        hijri_month = m + 1;
    }

    // Bounds check
    if (hijri_month > 12) hijri_month = 12;
    if (hijri_day < 1) hijri_day = 1;
    if (hijri_day > 30) hijri_day = 30;

    printk("Hijri (Tabular): %d/%d/%d (JD=%.1f, days_since_epoch=%ld)\n",
           hijri_day, hijri_month, hijri_year, JD, days_since_epoch);

    return (hijri_date_t){
        .day = hijri_day,
        .month = hijri_month,
        .year = hijri_year
    };
}

// Calculate day of the week from Julian Day
const char* day_Of_Weak(double JD) {
    JD += 1.5;
    int w = (long)JD % 7;
    
    // Declare constant name of weeks
    const char * WEEKS[] = {"Sunday", "Monday", "Tuesday", "Wednesday", 
                            "Thursday", "Friday", "Saturday"
                            };
    
    // Convert Julian Day to integer parts for printk
    double jd_val = JD - 1.5;
    int jd_int = (int)jd_val;
    int jd_frac = (int)((jd_val - jd_int) * 1000000);
    printk("Julian Day: %d.%06d, Day of week index: %d, Day: %s\n", jd_int, jd_frac, w, WEEKS[w]);
    
    return WEEKS[w];
}

// Astronomical calculation functions for prayer times

double degreeCorrected(long x) {
    x = x % 360;
    if (x < 0) {
        x += 360;
    }
    printk("degreeCorrected: %ld\n", x);
    return (double)x;
}

double Degree_2_Radian(long double Degrees) {
    double rad = (double)Degrees * (M_PI / 180.0);
    return rad;
}

double Radian_2_Degree(double Rad) {
    double degree = Rad * (180.0 / M_PI);
    return degree;
}

double twilligt(double winkel) {
    double twilligt_val = acos((-sin(Degree_2_Radian(winkel)) - (sin(D) * sin(Degree_2_Radian(Lat)))) / 
                              (cos(D) * cos(Degree_2_Radian(Lat)))) / Degree_2_Radian(15);
    
    if (isnan(twilligt_val)) {
        double twilligt_45 = acos((-sin(Degree_2_Radian(winkel)) - (sin(D) * sin(Degree_2_Radian(45)))) / 
                                 (cos(D) * cos(Degree_2_Radian(45)))) / Degree_2_Radian(15);
        return twilligt_45;
    } else {
        return twilligt_val;
    }
}

double calc_asrAngle(int factor) {
    long double acot = (M_PI / 2.0) - atan((double)factor + tan(Degree_2_Radian(Lat) - D));
    double angle = acos(sin(acot - sin(Degree_2_Radian(Lat)) * sin(D)) / 
                       (cos(Degree_2_Radian(Lat)) * cos(D))) / Degree_2_Radian(15);
    return angle;
}

double calc_altitude(void) {
    // Use current GPS seeHeight if available, otherwise use default value
    double altitude_in_meter = 0.0;  // Default sea level
    
    if (current_gps.seeHeight_valid) {
        altitude_in_meter = current_gps.seeHeight;
        // Convert to integer representation for printk
        int alt_int = (int)altitude_in_meter;
        int alt_frac = (int)((altitude_in_meter - alt_int) * 10);
        printk("Using GPS altitude: %d.%d meters\n", alt_int, alt_frac);
    } else {
        printk("GPS altitude not available, using sea level (0m)\n");
    }
    
    double altitude_correction = (-2.076 * sqrt(altitude_in_meter)) / 60.0;
    // Convert to integer representation for printk
    int corr_int = (int)(altitude_correction * 1000);
    printk("Altitude correction: %d.%03d degrees\n", corr_int / 1000, abs(corr_int % 1000));
    return altitude_correction;
}

prayer_myFloats_t prayerStruct(void) {
    prayer_myFloats_t localStruct;
    
    // Get current Julian Day
    double JD = current_JulianDay;
    // Convert to integer representation for printk
    int jd_int = (int)JD;
    int jd_frac = (int)((JD - jd_int) * 1000000);
    printk("JulianDay: %d.%06d\n", jd_int, jd_frac);
    
    // The time interval in julian centuries  
    double T = (JD - 2451545)/36525;
    
    // Mean longitudes of sun L
    double L = 280.4665 + (T * 36000.7698);
    
    // Mean longitudes of Moon L'
    double L_Strich = 218.3165 + (T * 481267.8813);
    
    // Lng of ascending node of Moon Ω
    double L_n = (125.04452 - (T*1934.136261) + (pow(T,2)*0.0020708) + (pow(T,3)/450000));
    
    // delta ψ
    double delta_W = (-0.0047*sin(L_n)) - (-0.00036*sin(2*L)) - (0.0000638*sin(2*L_Strich)) + (0.0000583*sin(2*L_n));
    
    // delta ε
    double delta_E = (0.0025*cos(L_n)) + (0.0001583*cos(2*L)) - (0.000027*cos(2*L_Strich)) - (0.00025*cos(2*L_n));
    
    // ε° The mean obliquity of the ecliptics
    double Eo = 23.43929111 - (T*0.01300416667) + (pow(T,2)*0.0000001638) + (pow(T,3)*0.00000050361);
    
    // Obliquity of the ecliptic
    double E = delta_E + Eo;
    
    // Time measured in julian millennia
    double r = (JD - 2451545)/365250;
    
    // Sun's mean longitude
    double Lo = 280.4664567 + (r*360007.6982779) + (pow(r,2)*0.03032028) + (pow(r,3)/49931) - (pow(r,4)/15300) - (pow(r,5)/2000000);
    Lo = degreeCorrected((long)Lo);
    
    // Anomaly of the sun M, sun position
    double M = 357.52911 + (35999.05029*T) - ((0.0001537*pow(T,2)));
    M = degreeCorrected((long)M);
    
    // Eccentricity e of the Earth's orbit
    double e = 0.016708634 - (0.000042037*T) - (0.0000001267*pow(T,2));
    
    // The sun's equation of center C
    double C = ((1.914602 - (0.004817*T) - 0.000014*pow(T, 2))*sin(Degree_2_Radian(M))) + 
               ((0.019993 - 0.000101*T)*sin(2*Degree_2_Radian(M))) + 
               ((0.000289*sin(3*Degree_2_Radian(M))));
    
    // Sun True Longitude
    double sun_True_Lng = (Lo + C);
    
    // Sun's right ascension
    double a = atan2((cos(Degree_2_Radian(E))*sin(Degree_2_Radian(sun_True_Lng))), cos(Degree_2_Radian(sun_True_Lng)));
    a = Radian_2_Degree(a);
    
    if (a < 0) {
        a = (long)a % 360;
    }
    
    // Sun's declination
    D = asin(sin(Degree_2_Radian(E))*sin(Degree_2_Radian(sun_True_Lng)));
    
    // The mean sidereal time at Greenwich θ°
    double sideReal_Time = 280.46061837 + (360.98564736629*(JD - 2451545.0)) + (pow(T,2)*0.000387933) - (pow(T,3)/38710000);
    sideReal_Time = (long)sideReal_Time % 360;
    if(sideReal_Time < 0) {
        sideReal_Time += 360;
    }
    
    // Equation of time
    double EqT = Lo - 0.0057183 - a + (double)(delta_W*cos(E));
    // Convert to integer for printk
    int eqt_int = (int)(EqT * 1000000);
    printk("equation of time: %d.%06d\n", eqt_int / 1000000, abs(eqt_int % 1000000));
    
    double EqT_to_min = EqT/15;
    int eqt_min_int = (int)(EqT_to_min * 1000000);
    printk("equation of time in min: %d.%06d\n", eqt_min_int / 1000000, abs(eqt_min_int % 1000000));

    // Debug: Show timezone being used for prayer calculations
    printk("\n[PRAYER CALC] ===== PRAYER TIME CALCULATION =====\n");
    printk("[PRAYER CALC] Using TimeZone: UTC%+d\n", TimeZone);
    printk("[PRAYER CALC] Longitude: %.6f\n", Lng);
    printk("[PRAYER CALC] Latitude: %.6f\n", Lat);
    printk("[PRAYER CALC] Formula: Dhuhr = 12 + TimeZone - (Lng/15) - EqT_to_min\n");
    printk("[PRAYER CALC] Dhuhr = 12 + %d - (%.6f/15) - %.6f\n", TimeZone, Lng, EqT_to_min);

    // The 5 Prayers and sunrise and sunset
    double Dhuhr = 12 + TimeZone - (Lng/15) - EqT_to_min;
    int dhuhr_int = (int)(Dhuhr * 1000);
    printk("Dhuhr: %d.%03d\n", dhuhr_int / 1000, abs(dhuhr_int % 1000));
    
    // Convert Lng to integer for printk
    int lng_int = (int)(Lng * 1000000);
    printk("Lng: %d.%06d\n", lng_int / 1000000, abs(lng_int % 1000000));
    
    // Get altitude correction
    double alt_correction = calc_altitude();
    
    double Sunrise = Dhuhr - twilligt(0.833 - (-1)*alt_correction);
    int sunrise_int = (int)(Sunrise * 1000);
    
    double Sunset = Dhuhr + twilligt(0.833 + (-1)*alt_correction);
    int sunset_int = (int)(Sunset * 1000);
    
    double Asr = Dhuhr + calc_asrAngle(asr_Angle_factor);
    int asr_int = (int)(Asr * 1000);
    
    // Maghrib = Sunset
    double Magrib = Sunset;
    int magrib_int = (int)(Magrib * 1000);
    
    double Ishaa = Dhuhr + twilligt(isha_Angle);
    int ishaa_int = (int)(Ishaa * 1000);
    
    double Fajr = Dhuhr - twilligt(fajr_Angle);
    int fajr_int = (int)(Fajr * 1000);
    
    // Fill the struct
    localStruct.Dhuhur = Dhuhr;
    localStruct.Assr = Asr;
    localStruct.Maghreb = Magrib;
    localStruct.sunRise = Sunrise;
    localStruct.sunDown = Sunset;
    localStruct.Ishaa = Ishaa;
    localStruct.fajjir = Fajr;

    // Debug: Show all calculated prayer times
    printk("[PRAYER CALC] ===== CALCULATED PRAYER TIMES (decimal hours) =====\n");
    printk("[PRAYER CALC] Fajr:    %.4f\n", Fajr);
    printk("[PRAYER CALC] Sunrise: %.4f\n", Sunrise);
    printk("[PRAYER CALC] Dhuhr:   %.4f\n", Dhuhr);
    printk("[PRAYER CALC] Asr:     %.4f\n", Asr);
    printk("[PRAYER CALC] Maghrib: %.4f\n", Magrib);
    printk("[PRAYER CALC] Isha:    %.4f\n", Ishaa);
    printk("[PRAYER CALC] ===============================================\n\n");

    return localStruct;
}

// Function to determine next prayer based on current time
// Returns prayer index (0=Fajr, 1=Shuruq, 2=Dhuhr, 3=Asr, 4=Maghrib, 5=Isha)
int get_next_prayer_index(const char* current_time, const prayer_myFloats_t* prayers)
{
    if (!current_time || !prayers) {
        return 3; // Default to Asr if invalid input
    }

    // Parse current time (format: "HH:MM:SS" or "HH:MM")
    int current_hour = 0, current_min = 0;
    if (sscanf(current_time, "%d:%d", &current_hour, &current_min) < 2) {
        return 3; // Default to Asr if parsing fails
    }

    // Convert current time to decimal hours
    double current_time_decimal = current_hour + (current_min / 60.0);

    // Prayer times in order: Fajr, Sunrise, Dhuhr, Asr, Maghrib, Isha
    double prayer_times[6] = {
        prayers->fajjir,    // 0: Fajr
        prayers->sunRise,   // 1: Shuruq (Sunrise)
        prayers->Dhuhur,    // 2: Dhuhr
        prayers->Assr,      // 3: Asr
        prayers->Maghreb,   // 4: Maghrib
        prayers->Ishaa      // 5: Isha
    };

    // Find next prayer after current time
    for (int i = 0; i < 6; i++) {
        // Ensure prayer times are within 24 hours
        while (prayer_times[i] < 0) prayer_times[i] += 24;
        while (prayer_times[i] >= 24) prayer_times[i] -= 24;

        if (current_time_decimal < prayer_times[i]) {
            return i; // Return index of next prayer
        }
    }

    // If current time is after all prayers, next prayer is Fajr of next day
    return 0; // Fajr
}

// Function to blink LED1 for 1 minute at prayer time
void Pray_Athan(void) {
    #if DT_NODE_HAS_STATUS(DT_ALIAS(led1), okay)
    const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

    if (!device_is_ready(led1.port)) {
        printk("LED1 device not ready\n");
        return;
    }

    int ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Failed to configure LED1: %d\n", ret);
        return;
    }

    printk("Prayer time! Blinking LED1 for 1 minute...\n");

    // Blink for 60 seconds (30 iterations of 2 seconds each)
    for (int i = 0; i < 30; i++) {
        gpio_pin_set_dt(&led1, 1);  // LED on
        k_msleep(1000);              // 1 second on
        gpio_pin_set_dt(&led1, 0);  // LED off
        k_msleep(1000);              // 1 second off
    }

    printk("LED1 blinking completed\n");
    #else
    printk("LED1 not available on this board\n");
    #endif
}

void prayer_set_timezone(int timezone_offset)
{
    TimeZone = timezone_offset;
    printk("[PRAYER] Timezone set to UTC%+d for prayer calculations\n", TimeZone);
}

int prayer_get_timezone(void)
{
    return TimeZone;
}