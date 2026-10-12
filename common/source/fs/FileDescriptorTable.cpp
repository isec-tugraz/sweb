#include "FileDescriptorTable.h"
#include "kprintf.h"
#include "debug.h"
#include "assert.h"
#include "Mutex.h"
#include "ScopeLock.h"
#include "File.h"

FileDescriptorTable global_file_descriptor_table;

FileDescriptorTable::FileDescriptorTable() :
    file_descriptors_(), fd_lock_("File descriptor table lock"), next_fd_(3)
{
}

FileDescriptorTable::~FileDescriptorTable()
{
  while (!file_descriptors_.empty())
  {
    auto it = file_descriptors_.begin();
    File* file = it->second;
    file_descriptors_.erase(it->first);
    file->closeFileDescriptor();
  }
}

uint32 FileDescriptorTable::add(File* file)
{
  assert(file);
  ScopeLock lock(fd_lock_);
  uint32 fd = next_fd_++;
  assert(file_descriptors_.find(fd) == file_descriptors_.end());
  file_descriptors_[fd] = file;
  file->addFileDescriptor();
  debug(VFS_FILE, "File descriptor table, add fd %u for file %p\n", fd, file);
  return fd;
}

File* FileDescriptorTable::remove(uint32 fd)
{
  ScopeLock lock(fd_lock_);
  auto it = file_descriptors_.find(fd);
  if (it == file_descriptors_.end())
    return 0;

  File* file = it->second;
  file_descriptors_.erase(fd);
  debug(VFS_FILE, "File descriptor table, remove fd %u\n", fd);
  return file;
}

int FileDescriptorTable::close(uint32 fd)
{
  File* file = remove(fd);
  if (!file)
    return -1;

  file->closeFileDescriptor();
  return 0;
}

File* FileDescriptorTable::get(uint32 fd)
{
  ScopeLock lock(fd_lock_);
  auto it = file_descriptors_.find(fd);
  return it == file_descriptors_.end() ? 0 : it->second;
}
