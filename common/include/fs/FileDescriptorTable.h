#pragma once

#include "types.h"
#include "umap.h"
#include "Mutex.h"

class File;

class FileDescriptorTable
{
  public:
    FileDescriptorTable();
    ~FileDescriptorTable();

    uint32 add(File* file);
    int close(uint32 fd);
    File* get(uint32 fd);

  private:
    File* remove(uint32 fd);
    ustl::map<uint32, File*> file_descriptors_;
    Mutex fd_lock_;
    uint32 next_fd_;
};

extern FileDescriptorTable global_file_descriptor_table;
