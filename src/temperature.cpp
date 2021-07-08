#include "../Configuration.hpp"
#include "Utility.hpp"
#include "temperature.hpp"

#if THERMISTOR == 1

double Temperature::calcTemp() 
{
    uint8_t i;
    float average;
    double steinhart;
    int samples[NUMSAMPLES];

    for (i=0; i< NUMSAMPLES; i++) {
        samples[i] = analogRead(THERMISTOR_PIN);
        delay(10);
    }

    average = 0;
    for (i=0; i< NUMSAMPLES; i++) {
     average += samples[i];
    }
    average /= NUMSAMPLES;
    average = 1023 / average - 1;
    average = SERIESRESISTOR / average;

    steinhart = average / THERMISTORNOMINAL;     // (R/Ro)
    steinhart = log(steinhart);                  // ln(R/Ro)
    steinhart /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
    steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
    steinhart = 1.0 / steinhart;                 // Invert
    steinhart -= 273.15;                         // convert absolute temp to C


    return(steinhart);
}


#endif