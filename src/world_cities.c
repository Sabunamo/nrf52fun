#include "world_cities.h"
#include <string.h>
#include <math.h>

const city_data_t* find_city_by_name(const char* city_name)
{
    if (!city_name) {
        return NULL;
    }
    
    for (int i = 0; i < WORLD_CITIES_COUNT; i++) {
        if (strcmp(world_cities[i].city_name, city_name) == 0) {
            return &world_cities[i];
        }
    }
    
    return NULL; // City not found
}

int get_total_cities_count(void)
{
    return WORLD_CITIES_COUNT;
}

const city_data_t* get_city_by_index(int index)
{
    if (index < 0 || index >= WORLD_CITIES_COUNT) {
        return NULL;
    }
    
    return &world_cities[index];
}

static double calculate_distance(double lat1, double lon1, double lat2, double lon2)
{
    // Calculate distance between two points using Haversine formula (simplified)
    double dlat = lat2 - lat1;
    double dlon = lon2 - lon1;
    
    // Simple euclidean distance approximation for nearby points
    // More accurate than full Haversine for city matching purposes
    double distance = sqrt(dlat * dlat + dlon * dlon);
    return distance;
}

const city_data_t* find_nearest_city(double latitude, double longitude)
{
    if (WORLD_CITIES_COUNT == 0) {
        return NULL;
    }
    
    const city_data_t* nearest_city = &world_cities[0];
    double min_distance = calculate_distance(latitude, longitude, 
                                           world_cities[0].latitude, 
                                           world_cities[0].longitude);
    
    // Search through all cities to find the nearest one
    for (int i = 1; i < WORLD_CITIES_COUNT; i++) {
        double distance = calculate_distance(latitude, longitude,
                                           world_cities[i].latitude,
                                           world_cities[i].longitude);
        
        if (distance < min_distance) {
            min_distance = distance;
            nearest_city = &world_cities[i];
        }
    }
    
    return nearest_city;
}