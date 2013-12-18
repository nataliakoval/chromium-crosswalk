// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CHROME_FRAME_DELEGATE_H_
#define CHROME_FRAME_CHROME_FRAME_DELEGATE_H_

#include <atlbase.h>
#include <atlwin.h>
#include <queue>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/pending_task.h"
#include "base/synchronization/lock.h"
#include "chrome/common/automation_constants.h"
#include "ipc/ipc_message.h"

class GURL;
struct ContextMenuModel;

namespace net {
class URLRequestStatus;
}

namespace gfx {
class Rect;
}

// A common interface supported by all the browser specific ChromeFrame
// implementations.
class ChromeFrameDelegate {
 public:
  typedef HWND WindowType;

  virtual WindowType GetWindow() const = 0;
  virtual void GetBounds(RECT* bounds) = 0;
  virtual std::string GetDocumentUrl() = 0;
  virtual void OnAutomationServerReady() = 0;
  virtual void OnAutomationServerLaunchFailed(
      AutomationLaunchResult reason, const std::string& server_version) = 0;
  virtual bool OnMessageReceived(const IPC::Message& msg) = 0;
  virtual void OnChannelError() = 0;

  // This remains in interface since we call it if Navigate()
  // returns immediate error.
  virtual void OnLoadFailed(int error_code, const std::string& url) = 0;

  // Returns true if this instance is alive and well for processing automation
  // messages.
  virtual bool IsValid() const = 0;

 protected:
  virtual ~ChromeFrameDelegate() {}
};

extern UINT kAutomationServerReady;
extern UINT kMessageFromChromeFrame;

class ChromeFrameDelegateImpl : public ChromeFrameDelegate {
 public:
  virtual WindowType GetWindow() { return NULL; }
  virtual void GetBounds(RECT* bounds) {}
  virtual std::string GetDocumentUrl() { return std::string(); }
  virtual void OnAutomationServerReady() {}
  virtual void OnAutomationServerLaunchFailed(
      AutomationLaunchResult reason, const std::string& server_version) {}
  virtual void OnLoadFailed(int error_code, const std::string& url) {}
  virtual bool OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelError() {}

  static bool IsTabMessage(const IPC::Message& message);

  virtual bool IsValid() const {
    return true;
  }
};

// This interface enables tasks to be marshaled to desired threads.
class TaskMarshaller {  // NOLINT
 public:
  virtual void PostTask(const tracked_objects::Location& from_here,
                        const base::Closure& task) = 0;
};

// T is expected to be something CWindowImpl derived, or at least to have
// PostMessage(UINT, WPARAM) method. Do not forget to CHAIN_MSG_MAP
template <class T> class TaskMarshallerThroughWindowsMessages
    : public TaskMarshaller {
 public:
  TaskMarshallerThroughWindowsMessages() {}
  virtual void PostTask(const tracked_objects::Location& posted_from,
                        const base::Closure& task) OVERRIDE {
    T* this_ptr = static_cast<T*>(this);
    if (this_ptr->IsWindow()) {
      this_ptr->AddRef();
      base::PendingTask* pending_task =
          new base::PendingTask(posted_from, task);
      PushTask(pending_task);
      this_ptr->PostMessage(MSG_EXECUTE_TASK,
                            reinterpret_cast<WPARAM>(pending_task));
    } else {
      DVLOG(1) << "Dropping MSG_EXECUTE_TASK message for destroyed window.";
    }
  }

 protected:
  ~TaskMarshallerThroughWindowsMessages() {
    DeleteAllPendingTasks();
  }

  void DeleteAllPendingTasks() {
    base::AutoLock lock(lock_);
    DVLOG_IF(1, !pending_tasks_.empty()) << "Destroying "
                                         << pending_tasks_.size()
                                         << " pending tasks";
    while (!pending_tasks_.empty()) {
      base::PendingTask* task = pending_tasks_.front();
      pending_tasks_.pop();
      delete task;
    }
  }

  BEGIN_MSG_MAP(PostMessageMarshaller)
    MESSAGE_HANDLER(MSG_EXECUTE_TASK, ExecuteTask)
  END_MSG_MAP()

 private:
  enum { MSG_EXECUTE_TASK = WM_APP + 6 };
  inline LRESULT ExecuteTask(UINT, WPARAM wparam, LPARAM,
                             BOOL& handled) {  // NOLINT
    base::PendingTask* pending_task =
        reinterpret_cast<base::PendingTask*>(wparam);
    if (pending_task && PopTask(pending_task)) {
      pending_task->task.Run();
      delete pending_task;
    }

    T* this_ptr = static_cast<T*>(this);
    this_ptr->Release();
    return 0;
  }

  inline void PushTask(base::PendingTask* pending_task) {
    base::AutoLock lock(lock_);
    pending_tasks_.push(pending_task);
  }

  // If |pending_task| is front of the queue, removes the task and returns true,
  // otherwise we assume this is an already destroyed task (but Window message
  // had remained in the thread queue).
  inline bool PopTask(base::PendingTask* pending_task) {
    base::AutoLock lock(lock_);
    if (!pending_tasks_.empty() && pending_task == pending_tasks_.front()) {
      pending_tasks_.pop();
      return true;
    }

    return false;
  }

  base::Lock lock_;
  std::queue<base::PendingTask*> pending_tasks_;
};

#endif  // CHROME_FRAME_CHROME_FRAME_DELEGATE_H_
