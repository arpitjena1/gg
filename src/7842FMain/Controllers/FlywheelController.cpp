#include "FlywheelController.hpp"

#include <cassert>

FlywheelController::FlywheelController(IntakeController*& iintake, Motor* iflywheel,
                                       ADIEncoder* isensor, VelMath* ivelMath,
                                       lib7842::emaFilter* irpmFilter, lib7842::velPID* ipid,
                                       double imotorSlew) :
  intake(iintake),
  flywheel {iflywheel},
  sensor(isensor),
  velMath(ivelMath),
  rpmFilter(irpmFilter),
  pid(ipid),
  motorSlew(imotorSlew),
  task(taskFnc, this) {
  flywheel->move(0);
}

void FlywheelController::setRpm(double rpm) { targetRpm = rpm; }

double FlywheelController::getTargetRpm() { return targetRpm; }

void FlywheelController::disable() {
  if (!disabled) flywheel->moveVoltage(0);
  disabled = true;
}

void FlywheelController::enable() { disabled = false; }

void FlywheelController::resetSlew() {
  lastPower = 0;
  motorPower = 0;
}

void FlywheelController::run() {

 
  
  sensor->reset();

  double lastRpm = 0;
  Timer accelTimer;
  EmaFilter accelEma(0.15);

  while (true) {
    if (!disabled || intake->indexerSlave) // there is a motor available
    {
      currentRpm = rpmFilter->filter(velMath->step(sensor->get()).convert(rpm));
      motorPower = pid->calculate(targetRpm, currentRpm);

      if (motorPower <= 0) motorPower = 0; // Prevent motor from spinning backward
      // Give the motor a bit of a starting boost
      if (motorPower > lastPower && lastPower < 10 && motorPower > 10) lastPower = 10;

      // This slews the motor by limiting the rate of change of the motor speed
      double increment = motorPower - lastPower;
      if (increment > motorSlew) motorPower = lastPower + motorSlew;
      else if (increment < motorSlew)
        motorPower = lastPower - motorSlew * 3.0;
      lastPower = motorPower;

      // moves whatever motor is available
      if (!disabled) flywheel->move(motorPower);
      if (intake->indexerSlave) intake->indexer->move(-motorPower);
      // std::cout << motorPower << std::endl;
    } else // If no motors are available, approximate how much the flywheel slows down
    {
      // lastPower = lastPower <= 0 ? 0 : lastPower - 0.24;
      // motorPower = 0;
    }

    // std::cout << std::setprecision(3) << "Target/4: " << targetRpm/4 << " Rpm/4: " <<
    // currentRpm/4 << " Power: "<< motorPower << " D: "<< pid->getD() << " Sensor: " <<
    // sensor->get() << std::endl;
   

    if (accelTimer.getDtFromMark() >= 50_ms) {
      currentAccel = accelEma.filter(currentRpm - lastRpm);
      isShot = currentAccel < -95.0 / 15.0;
      // std::cout << "Accel: " << currentRpm - lastRpm << " Shot: " << isShot << std::endl;
      lastRpm = currentRpm;
      accelTimer.placeMark();
    }

    pros::delay(10);
  }
}

void FlywheelController::taskFnc(void* input) {
  pros::delay(500);
  FlywheelController* that = static_cast<FlywheelController*>(input);
  that->run();
}
