#include <iostream>
#include "STSL/RJRobot.h"

using namespace std;

int main() {

    RJRobot robot;

    while(true) {

        robot.Wait(1000ms);
    }

    return 0;
}