#include "platform/subprocess.h"

#include <stdlib.h>
#include <windows.h>

#include <memory>
#include <vector>

namespace platform {

namespace {

enum OpenFlags {
  kOpenForReading,
  kOpenForWriting,
};

class ScopedHandle {
 public:
  ScopedHandle(const stdext::file_system::Path& path,
               OpenFlags flags) {
    _SECURITY_ATTRIBUTES security_attributes;
    memset(&security_attributes, 0, sizeof(security_attributes));
    security_attributes.nLength = sizeof(security_attributes);
    security_attributes.lpSecurityDescriptor = NULL;
    security_attributes.bInheritHandle = TRUE;

    handle_ = CreateFileA(
        path.c_str(),
        (flags == kOpenForReading ? GENERIC_READ : GENERIC_WRITE),
        0 ,
        &security_attributes,
        (flags == kOpenForReading ? OPEN_EXISTING : CREATE_ALWAYS),
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    }

    ScopedHandle(const ScopedHandle& rhs) = delete;
    ScopedHandle& operator=(const ScopedHandle& rhs) = delete;

    ~ScopedHandle() {
      if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
      }
    }
    HANDLE handle() const { return handle_; }

  private:
    HANDLE handle_;
};

class ScopedAttributeList {
 public:
  ScopedAttributeList(std::vector<HANDLE>* inherit_handles) {
    SIZE_T attribute_list_size;

    if (!InitializeProcThreadAttributeList(NULL, 1, 0, &attribute_list_size) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      return;
    }

    attribute_list_ = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
        HeapAlloc(GetProcessHeap(), 0, attribute_list_size));
    if (attribute_list_ == NULL) {
      return;
    }

    if (!InitializeProcThreadAttributeList(
             attribute_list_, 1, 0, &attribute_list_size)) {
      return;
    }
    initialized_ = true;

    DWORD num_handles = inherit_handles->size();
    if (!UpdateProcThreadAttribute(
             attribute_list_,
             0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
             inherit_handles->data(),
             num_handles * sizeof(HANDLE), NULL, NULL)) {
      return;
    }
    updated_ = true;
  }

  ~ScopedAttributeList() {
    if (initialized_) DeleteProcThreadAttributeList(attribute_list_);
    if (attribute_list_) HeapFree(GetProcessHeap(), 0, attribute_list_);
  }

  bool error() const { return !updated_; }

  LPPROC_THREAD_ATTRIBUTE_LIST attribute_list() const {
    assert(!error());
    return attribute_list_;
  }

 private:
  LPPROC_THREAD_ATTRIBUTE_LIST attribute_list_ = NULL;
  bool initialized_ = false;
  bool updated_ = false;
};

}  // namespace

int SystemCommand(
    const char* command,
    const stdext::optional<stdext::file_system::Path>& stdout_file,
    const stdext::optional<stdext::file_system::Path>& stderr_file,
    const stdext::optional<stdext::file_system::Path>& stdin_file) {
  std::unique_ptr<char[]> command_copy(new char[strlen(command) + 1]);
  strcpy(command_copy.get(), command);

  STARTUPINFOEX startup_info;
  memset(&startup_info, 0, sizeof(startup_info));
  startup_info.StartupInfo.cb = sizeof(startup_info);
  startup_info.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

  stdext::optional<ScopedHandle> stdout_handle;
  stdext::optional<ScopedHandle> stderr_handle;
  stdext::optional<ScopedHandle> stdin_handle;

  std::vector<HANDLE> redirect_handles;

  if (stdout_file) {
    stdout_handle.emplace(*stdout_file, kOpenForWriting);
    if (stdout_handle->handle() == INVALID_HANDLE_VALUE) {
      return 1;
    }
    startup_info.StartupInfo.hStdOutput = stdout_handle->handle();
    redirect_handles.push_back(stdout_handle->handle());
  }

  if (stderr_file) {
    stderr_handle.emplace(*stderr_file, kOpenForWriting);
    if (stderr_handle->handle() == INVALID_HANDLE_VALUE) {
      return 1;
    }
    startup_info.StartupInfo.hStdError = stderr_handle->handle();
    redirect_handles.push_back(stderr_handle->handle());
  }

  if (stdin_file) {
    stdin_handle.emplace(*stdin_file, kOpenForReading);
    if (stdin_handle->handle() == INVALID_HANDLE_VALUE) {
      return 1;
    }
    startup_info.StartupInfo.hStdInput = stdin_handle->handle();
    redirect_handles.push_back(stdin_handle->handle());
  }

  // We must explicitly list all file handles that the child process should
  // inherit.
  stdext::optional<ScopedAttributeList> attribute_list;
  if (!redirect_handles.empty()) {
    attribute_list.emplace(&redirect_handles);
    if (attribute_list->error()) {
      return 1;
    }
    startup_info.lpAttributeList = attribute_list->attribute_list();
  } else {
    startup_info.lpAttributeList = NULL;
  }

  PROCESS_INFORMATION process_info;
  memset(&process_info, 0, sizeof(process_info));

  if (!CreateProcessA(
          NULL, command_copy.get(), NULL, NULL,
          /* inherit handles */ startup_info.lpAttributeList != NULL,
          EXTENDED_STARTUPINFO_PRESENT,
          NULL, NULL,
          &startup_info.StartupInfo, &process_info)) {
    return 1;
  }

  DWORD wait_result = WaitForSingleObject(process_info.hProcess, INFINITE);
  int return_code = 0;
  if (wait_result != WAIT_OBJECT_0) {
    TerminateProcess(process_info.hProcess, 1);
    return_code = 1;
  } else {
    DWORD exit_code;
    if (GetExitCodeProcess(process_info.hProcess, &exit_code)) {
      return_code = exit_code;
    } 
  }

  CloseHandle(process_info.hProcess);
  CloseHandle(process_info.hThread);
  return return_code;
}

}  // namespace platform
