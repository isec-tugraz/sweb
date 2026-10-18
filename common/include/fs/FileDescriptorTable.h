#pragma once

#include "types.h"
#include "umap.h"
#include "Mutex.h"

class KernelFileHandle;

class FileDescriptorTable
{
  public:
    FileDescriptorTable();
    ~FileDescriptorTable();

    uint32 add(KernelFileHandle* file_handle);
    int close(uint32 fd);
    KernelFileHandle* get(uint32 fd);

  private:
    KernelFileHandle* remove(uint32 fd);
    ustl::map<uint32, KernelFileHandle*> file_descriptors_;
    Mutex fd_lock_;
    uint32 next_fd_;
};

extern FileDescriptorTable global_file_descriptor_table;
