#ifdef MK4
#include "mk4/FlowControlApp.hpp"
#elif MK5
#include "mk5/FlowControlApp.hpp"
#endif

FlowControlApp app;

void setup() {
    app.begin();
}

void loop() {
    app.loop();
}
