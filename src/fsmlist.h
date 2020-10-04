//
// Created by napik-PC on 20.09.2020.
//

#ifndef IOSPFD_FSMLIST_H
#define IOSPFD_FSMLIST_H

#include "tinyfsm.hpp"

#include "elevator.hpp"
#include "motor.hpp"

using fsm_list = tinyfsm::FsmList<Motor, Elevator>;

/** dispatch event to both "Motor" and "Elevator" */
template<typename E>
void send_event(E const &event) {
    fsm_list::template dispatch<E>(event);
}

#endif //IOSPFD_FSMLIST_H
