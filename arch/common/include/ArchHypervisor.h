#pragma once

#include "ArchGuest.h"

class ArchHyperVisor {

    static ArchHyperVisor *instance_;

  public:
    static ArchHyperVisor *instance();

    virtual void init_guest(/*TODO*/) = 0;
};