/// I asked the robots to hand me a sine function and it seems to work?

#define PI 3.14159265358979323846f

// Helper to calculate absolute value
float absolute_val(float x) {
    return (x < 0) ? -x : x;
}

// Bare-metal Sine function using Taylor Series
float sin(float x) {
    // 1. Map x to the range [-PI, PI]
    while (x > PI)  x -= 2.0f * PI;
    while (x < -PI) x += 2.0f * PI;

    // 2. Taylor series calculation
    float term = x;  // First term of the expansion is x
    float sin_x = x; // Initialize sum with the first term
    float x_squared = x * x;
    
    // Iterate until the terms become insignificantly small
    // or for a fixed number of steps
    for (int i = 3; absolute_val(term) > 0.00001f; i += 2) {
        term = -term * x_squared / (float)(i * (i - 1));
        sin_x += term;
    }

    return sin_x;
}
