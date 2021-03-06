#include "Vehicle.h"
#include "Intersection.h"
#include "Street.h"
#include <future>
#include <iostream>
#include <random>

int Vehicle::_vehid_cnt = 0;

Vehicle::Vehicle() {
  _currStreet = nullptr;
  _posStreet = 0.0;
  _type = ObjectType::objectVehicle;
  // _speed = 400; // m/s
  _speed = (rand() % (5 - 2 + 1) + 2) * 100; // 2~500m/s
  _vehid = _vehid_cnt;
  _vehid_cnt++;

  // data race 현상이 있네... 슈바르....? 여러번 run 해보면 vehicle id 가
  // 순서대로 안나옴
}

Vehicle::~Vehicle() { _vehid_cnt--; }

void Vehicle::setCurrentDestination(std::shared_ptr<Intersection> destination) {
  // update destination
  _currDestination = destination;

  // reset simulation parameters
  _posStreet = 0.0;
}

void Vehicle::simulate() {
  // launch drive function in a thread
  threads.emplace_back(std::thread(&Vehicle::drive, this));
}

// virtual function which is executed in a thread
void Vehicle::drive() {
  // print id of the current thread
  std::cout << "Vehicle #" << _vehid
            << "::drive: thread id = " << std::this_thread::get_id()
            << std::endl;

  // initalize variables
  bool hasEnteredIntersection = false;
  double cycleDuration = 1; // duration of a single simulation cycle in ms
  std::chrono::time_point<std::chrono::system_clock> lastUpdate;

  // init stop watch
  lastUpdate = std::chrono::system_clock::now();
  while (true) {
    // sleep at every iteration to reduce CPU usage
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    // compute time difference to stop watch
    long timeSinceLastUpdate =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - lastUpdate)
            .count();
    if (timeSinceLastUpdate >= cycleDuration) {
      // update position with a constant velocity motion model
      _posStreet += _speed * timeSinceLastUpdate / 1000;

      // compute completion rate of current street
      double completion = _posStreet / _currStreet->getLength();

      // compute current pixel position on street based on driving direction
      std::shared_ptr<Intersection> i1, i2;
      i2 = _currDestination;
      i1 = i2->getID() == _currStreet->getInIntersection()->getID()
               ? _currStreet->getOutIntersection()
               : _currStreet->getInIntersection();

      double x1, y1, x2, y2, xv, yv, dx, dy, l;
      i1->getPosition(x1, y1);
      i2->getPosition(x2, y2);
      dx = x2 - x1;
      dy = y2 - y1;
      l = sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (x1 - x2));
      xv = x1 + completion *
                    dx; // new position based on line equation in parameter form
      yv = y1 + completion * dy;
      this->setPosition(xv, yv);

      // check wether halting position in front of destination has been reached
      if (completion >= 0.9 && !hasEnteredIntersection) {
        // Task L2.1 : Start up a task using std::async which takes a reference
        // to the method Intersection::addVehicleToQueue, the object
        // _currDestination and a shared pointer to this using the
        // get_shared_this() function. Then, wait for the data to be available
        // before proceeding to slow down.

        std::future<void> ftr = std::async(&Intersection::addVehicleToQueue,
                                           _currDestination, get_shared_this());
        // std::thread t1 = std::thread(&Vehicle::addID, &v1, 1); 이 개념으로
        // 생각하면 댐, make_thread_with_arg_3_class_member.cpp에 정리되어
        // 있는데~ 간단히 말하자면
        // std::async(&Class::method, &Instance, method's arg1, arg2 ...);
        // 이런 표기법 or &Instance자리에 &없이 shared_ptr사용

        ftr.get();
        // get에 wait이 포함된다. (단, 두 번 get사용하면 안댐)

        // slow down and set intersection flag
        _speed /= 10.0;
        hasEnteredIntersection = true;
      }

      // check wether intersection has been crossed
      if (completion >= 1.0 && hasEnteredIntersection) {
        // choose next street and destination
        std::vector<std::shared_ptr<Street>> streetOptions =
            _currDestination->queryStreets(_currStreet);
        std::shared_ptr<Street> nextStreet;
        if (streetOptions.size() > 0) {
          // pick one street at random and query intersection to enter this
          // street
          std::random_device rd;
          std::mt19937 eng(rd());
          std::uniform_int_distribution<> distr(0, streetOptions.size() - 1);
          nextStreet = streetOptions.at(distr(eng));
        } else {
          // this street is a dead-end, so drive back the same way
          nextStreet = _currStreet;
        }

        // pick the one intersection at which the vehicle is currently not
        std::shared_ptr<Intersection> nextIntersection =
            nextStreet->getInIntersection()->getID() ==
                    _currDestination->getID()
                ? nextStreet->getOutIntersection()
                : nextStreet->getInIntersection();

        // send signal to intersection that vehicle has left the intersection
        _currDestination->vehicleHasLeft(get_shared_this());

        // assign new street and destination
        this->setCurrentDestination(nextIntersection);
        this->setCurrentStreet(nextStreet);

        // reset speed and intersection flag
        _speed *= 10.0;
        hasEnteredIntersection = false;
      }

      // reset stop watch for next cycle
      lastUpdate = std::chrono::system_clock::now();
    }
  } // eof simulation loop
}
