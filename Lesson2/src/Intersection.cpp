#include <chrono>
#include <future>
#include <iostream>
#include <random>
#include <thread>

#include "Intersection.h"
#include "Street.h"
#include "Vehicle.h"

/* Implementation of class "WaitingVehicles" */

int WaitingVehicles::getSize() { return _vehicles.size(); }

void WaitingVehicles::pushBack(std::shared_ptr<Vehicle> vehicle,
                               std::promise<void> &&promise) {
  _vehicles.push_back(vehicle);
  _promises.push_back(std::move(promise));
}

void WaitingVehicles::permitEntryToFirstInQueue() {
  // Task L2.3 : First, get the entries from the front of _promises and
  // get entries from the front of both queues
  // begin : vector첫번째 자리의 주소를 가리키는 pointer 반환
  // begin에 의해 반환되는 애는 vector<type>::iterator 라는 type을 가진다.
  // front : vector첫번째 자리 value 반환
  auto frontPromise = _promises.begin();
  auto frontVehicle = _vehicles.begin();

  // fulfill promise and send signal back that permission to enter has been
  // granted
  frontPromise->set_value();

  // remove front elements from both queues
  // erase : 해당 인덱스의 데이터를 지우고 그 뒤에 있는 데이터를 남은 자리만큼
  // 앞으로 이동시킨다. arg = vector<type>::iterator 불리는 begin의 반환 type
  _vehicles.erase(frontVehicle);
  _promises.erase(frontPromise);
}

/* Implementation of class "Intersection" */

Intersection::Intersection() {
  _type = ObjectType::objectIntersection;
  _isBlocked = false;
}

void Intersection::addStreet(std::shared_ptr<Street> street) {
  _streets.push_back(street);
}

std::vector<std::shared_ptr<Street>>
Intersection::queryStreets(std::shared_ptr<Street> incoming) {
  // store all outgoing streets in a vector ...
  std::vector<std::shared_ptr<Street>> outgoings;
  for (auto it : _streets) {
    if (incoming->getID() !=
        it->getID()) // ... except the street making the inquiry
    {
      outgoings.push_back(it);
    }
  }

  return outgoings;
}

// adds a new vehicle to the queue and returns once the vehicle is allowed to
// enter
void Intersection::addVehicleToQueue(std::shared_ptr<Vehicle> vehicle) {
  std::cout << "Intersection #" << _id
            << "::addVehicleToQueue: thread id = " << std::this_thread::get_id()
            << std::endl;

  // Task L2.2 : First, add the new vehicle to the waiting line by creating a
  // promise, a corresponding future and then adding both to _waitingVehicles.
  // Then, wait until the vehicle has been granted entry.

  // create promise and future
  std::promise<void> prms;
  std::future<void> ftr = prms.get_future();
  _waitingVehicles.pushBack(vehicle, std::move(prms));

  /*
  ***********************************************************************
  */
  // below : 쓰레드 쓰라는 말은 아닌가벼.. task L2.2는 걍 위에 방식으로
  // 구현하라는 뜻임
  // std::thread t(&WaitingVehicles::pushBack, &_waitingVehicles, vehicle,
  //               std::move(prms));
  // t.join();

  // std::thread t1 = std::thread(&Vehicle::addID, &v1, 1); 이 개념으로
  // 생각하면 댐, make_thread_with_arg_3_class_member.cpp에 정리되어
  // 있는데~ 간단히 말하자면
  // std::async(&Class::method, &Instance, method's arg1, arg2 ...);
  // 이런 표기법 or &Instance자리에 &없이 shared_ptr사용
  /*
  ***********************************************************************
  */

  ftr.wait();

  std::cout << "Intersection #" << _id << ": Vehicle #" << vehicle->get_vehID()
            << " is granted entry." << std::endl;
}

void Intersection::vehicleHasLeft(std::shared_ptr<Vehicle> vehicle) {
  // std::cout << "Intersection #" << _id << ": Vehicle #" << vehicle->getID()
  // << " has left." << std::endl;

  // unblock queue processing
  this->setIsBlocked(false);
}

void Intersection::setIsBlocked(bool isBlocked) {
  _isBlocked = isBlocked;
  // std::cout << "Intersection #" << _id << " isBlocked=" << isBlocked <<
  // std::endl;
}

// virtual function which is executed in a thread
void Intersection::simulate() // using threads + promises/futures + exceptions
{
  // launch vehicle queue processing in a thread
  threads.emplace_back(std::thread(&Intersection::processVehicleQueue, this));
}

void Intersection::processVehicleQueue() {
  // print id of the current thread
  // std::cout << "Intersection #" << _id << "::processVehicleQueue: thread id =
  // " << std::this_thread::get_id() << std::endl;

  // continuously process the vehicle queue
  while (true) {
    // sleep at every iteration to reduce CPU usage
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    // only proceed when at least one vehicle is waiting in the queue
    if (_waitingVehicles.getSize() > 0 && !_isBlocked) {
      // set intersection to "blocked" to prevent other vehicles from entering
      this->setIsBlocked(true);

      // permit entry to first vehicle in the queue (FIFO)
      _waitingVehicles.permitEntryToFirstInQueue();
    }
  }
}
