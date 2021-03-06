// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/api/api_resource.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/id_util.h"
#include "extensions/common/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::NotificationService;

namespace extensions {

class ApiResourceManagerUnitTest : public ExtensionsTest {
 public:
  ApiResourceManagerUnitTest()
      : notification_service_(NotificationService::Create()) {}

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<NotificationService> notification_service_;
};

class FakeApiResource : public ApiResource {
 public:
  explicit FakeApiResource(const std::string& owner_extension_id)
      : ApiResource(owner_extension_id) {}
  virtual ~FakeApiResource() {}
  static const BrowserThread::ID kThreadId = BrowserThread::UI;
};

TEST_F(ApiResourceManagerUnitTest, TwoAppsCannotShareResources) {
  scoped_ptr<ApiResourceManager<FakeApiResource> > manager(
      new ApiResourceManager<FakeApiResource>(NULL));
  scoped_refptr<extensions::Extension> extension_one =
      test_util::CreateExtensionWithID("one");
  scoped_refptr<extensions::Extension> extension_two =
      test_util::CreateExtensionWithID("two");

  const std::string extension_one_id(extension_one->id());
  const std::string extension_two_id(extension_two->id());

  int resource_one_id = manager->Add(new FakeApiResource(extension_one_id));
  int resource_two_id = manager->Add(new FakeApiResource(extension_two_id));
  CHECK(resource_one_id);
  CHECK(resource_two_id);

  // Confirm each extension can get its own resource.
  ASSERT_TRUE(manager->Get(extension_one_id, resource_one_id) != NULL);
  ASSERT_TRUE(manager->Get(extension_two_id, resource_two_id) != NULL);

  // Confirm neither extension can get the other's resource.
  ASSERT_TRUE(manager->Get(extension_one_id, resource_two_id) == NULL);
  ASSERT_TRUE(manager->Get(extension_two_id, resource_one_id) == NULL);

  // And make sure we're not susceptible to any Jedi mind tricks.
  ASSERT_TRUE(manager->Get(std::string(), resource_one_id) == NULL);
}

}  // namespace extensions
