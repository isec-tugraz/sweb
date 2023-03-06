#pragma once

#include "ArchGuest.h"

class ArchHypervisor {

    static ArchHypervisor *instance_;

  public:
    static ArchHypervisor *instance();

    virtual void init_guest(/*TODO*/) = 0;
};
