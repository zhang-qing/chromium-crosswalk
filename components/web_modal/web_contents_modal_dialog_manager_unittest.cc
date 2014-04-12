// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_modal/web_contents_modal_dialog_manager.h"

#include <map>

#include "base/memory/scoped_ptr.h"
#include "components/web_modal/native_web_contents_modal_dialog_manager.h"
#include "components/web_modal/test_web_contents_modal_dialog_manager_delegate.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_modal {

// Tracks persistent state changes of the native WC-modal dialog manager.
class NativeManagerTracker {
 public:
  enum DialogState {
    UNKNOWN,
    NOT_SHOWN,
    SHOWN,
    HIDDEN,
    CLOSED
  };

  NativeManagerTracker() : state_(UNKNOWN), was_shown_(false) {}

  void SetState(DialogState state) {
    state_ = state;
    if (state_ == SHOWN)
      was_shown_ = true;
  }

  DialogState state_;
  bool was_shown_;
};

NativeManagerTracker unused_tracker;

class TestNativeWebContentsModalDialogManager
    : public NativeWebContentsModalDialogManager {
 public:
  TestNativeWebContentsModalDialogManager(
      NativeWebContentsModalDialogManagerDelegate* delegate,
      NativeManagerTracker* tracker)
      : delegate_(delegate),
        tracker_(tracker) {}

  virtual void ManageDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    if (tracker_)
      tracker_->SetState(NativeManagerTracker::NOT_SHOWN);
  }
  virtual void ShowDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    if (tracker_)
      tracker_->SetState(NativeManagerTracker::SHOWN);
  }
  virtual void HideDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    if (tracker_)
      tracker_->SetState(NativeManagerTracker::HIDDEN);
  }
  virtual void CloseDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    if (tracker_)
      tracker_->SetState(NativeManagerTracker::CLOSED);
    delegate_->WillClose(dialog);
  }
  virtual void FocusDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
  }
  virtual void PulseDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
  }
  virtual void HostChanged(WebContentsModalDialogHost* new_host) OVERRIDE {
  }

  void StopTracking() {
    tracker_ = NULL;
  }

 private:
  NativeWebContentsModalDialogManagerDelegate* delegate_;
  NativeManagerTracker* tracker_;

  DISALLOW_COPY_AND_ASSIGN(TestNativeWebContentsModalDialogManager);
};

class WebContentsModalDialogManagerTest
    : public content::RenderViewHostTestHarness {
 public:
  WebContentsModalDialogManagerTest()
      : next_dialog_id(1),
        manager(NULL) {
  }

  virtual void SetUp() {
    content::RenderViewHostTestHarness::SetUp();

    delegate.reset(new TestWebContentsModalDialogManagerDelegate);
    WebContentsModalDialogManager::CreateForWebContents(web_contents());
    manager = WebContentsModalDialogManager::FromWebContents(web_contents());
    manager->SetDelegate(delegate.get());
    test_api.reset(new WebContentsModalDialogManager::TestApi(manager));
  }

  virtual void TearDown() {
    test_api.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  NativeWebContentsModalDialog MakeFakeDialog() {
    // WebContentsModalDialogManager treats the NativeWebContentsModalDialog as
    // an opaque type, so creating fake NativeWebContentsModalDialogs using
    // reinterpret_cast is valid.
    return reinterpret_cast<NativeWebContentsModalDialog>(next_dialog_id++);
  }

  int next_dialog_id;
  scoped_ptr<TestWebContentsModalDialogManagerDelegate> delegate;
  WebContentsModalDialogManager* manager;
  scoped_ptr<WebContentsModalDialogManager::TestApi> test_api;

  DISALLOW_COPY_AND_ASSIGN(WebContentsModalDialogManagerTest);
};

NativeWebContentsModalDialogManager*
WebContentsModalDialogManager::CreateNativeManager(
    NativeWebContentsModalDialogManagerDelegate* native_delegate) {
  NOTREACHED();
  return new TestNativeWebContentsModalDialogManager(native_delegate,
                                                     &unused_tracker);
}

// Test that the dialog is shown immediately when the delegate indicates the web
// contents is visible.
TEST_F(WebContentsModalDialogManagerTest, WebContentsVisible) {
  // Dialog should be shown while WebContents is visible.
  const NativeWebContentsModalDialog dialog = MakeFakeDialog();

  NativeManagerTracker tracker;
  TestNativeWebContentsModalDialogManager* native_manager =
      new TestNativeWebContentsModalDialogManager(manager, &tracker);
  manager->ShowDialogWithManager(dialog,
      scoped_ptr<NativeWebContentsModalDialogManager>(native_manager).Pass());

  EXPECT_EQ(NativeManagerTracker::SHOWN, tracker.state_);
  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_TRUE(tracker.was_shown_);

  native_manager->StopTracking();
}

// Test that the dialog is not shown immediately when the delegate indicates the
// web contents is not visible.
TEST_F(WebContentsModalDialogManagerTest, WebContentsNotVisible) {
  // Dialog should not be shown while WebContents is not visible.
  delegate->set_web_contents_visible(false);

  const NativeWebContentsModalDialog dialog = MakeFakeDialog();

  NativeManagerTracker tracker;
  TestNativeWebContentsModalDialogManager* native_manager =
      new TestNativeWebContentsModalDialogManager(manager, &tracker);
  manager->ShowDialogWithManager(dialog,
      scoped_ptr<NativeWebContentsModalDialogManager>(native_manager).Pass());

  EXPECT_EQ(NativeManagerTracker::NOT_SHOWN, tracker.state_);
  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_FALSE(tracker.was_shown_);

  native_manager->StopTracking();
}

// Test that only the first of multiple dialogs is shown.
TEST_F(WebContentsModalDialogManagerTest, ShowDialogs) {
  const NativeWebContentsModalDialog dialog1 = MakeFakeDialog();
  const NativeWebContentsModalDialog dialog2 = MakeFakeDialog();
  const NativeWebContentsModalDialog dialog3 = MakeFakeDialog();

  NativeManagerTracker tracker1;
  NativeManagerTracker tracker2;
  NativeManagerTracker tracker3;
  TestNativeWebContentsModalDialogManager* native_manager1 =
      new TestNativeWebContentsModalDialogManager(manager, &tracker1);
  TestNativeWebContentsModalDialogManager* native_manager2 =
      new TestNativeWebContentsModalDialogManager(manager, &tracker2);
  TestNativeWebContentsModalDialogManager* native_manager3 =
      new TestNativeWebContentsModalDialogManager(manager, &tracker3);
  manager->ShowDialogWithManager(dialog1,
      scoped_ptr<NativeWebContentsModalDialogManager>(native_manager1).Pass());
  manager->ShowDialogWithManager(dialog2,
      scoped_ptr<NativeWebContentsModalDialogManager>(native_manager2).Pass());
  manager->ShowDialogWithManager(dialog3,
      scoped_ptr<NativeWebContentsModalDialogManager>(native_manager3).Pass());

  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(NativeManagerTracker::SHOWN, tracker1.state_);
  EXPECT_EQ(NativeManagerTracker::NOT_SHOWN, tracker2.state_);
  EXPECT_EQ(NativeManagerTracker::NOT_SHOWN, tracker3.state_);

  native_manager1->StopTracking();
  native_manager2->StopTracking();
  native_manager3->StopTracking();
}

// Test that the dialog is shown/hidden when the WebContents is shown/hidden.
TEST_F(WebContentsModalDialogManagerTest, VisibilityObservation) {
  const NativeWebContentsModalDialog dialog = MakeFakeDialog();

  NativeManagerTracker tracker;
  TestNativeWebContentsModalDialogManager* native_manager =
      new TestNativeWebContentsModalDialogManager(manager, &tracker);
  manager->ShowDialogWithManager(dialog,
      scoped_ptr<NativeWebContentsModalDialogManager>(native_manager).Pass());

  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(NativeManagerTracker::SHOWN, tracker.state_);

  test_api->WebContentsWasHidden();

  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(NativeManagerTracker::HIDDEN, tracker.state_);

  test_api->WebContentsWasShown();

  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(NativeManagerTracker::SHOWN, tracker.state_);

  native_manager->StopTracking();
}

// Test that attaching an interstitial page closes dialogs configured to close.
TEST_F(WebContentsModalDialogManagerTest, InterstitialPage) {
  const NativeWebContentsModalDialog dialog1 = MakeFakeDialog();
  const NativeWebContentsModalDialog dialog2 = MakeFakeDialog();
  const NativeWebContentsModalDialog dialog3 = MakeFakeDialog();

  NativeManagerTracker tracker1;
  NativeManagerTracker tracker2;
  NativeManagerTracker tracker3;
  TestNativeWebContentsModalDialogManager* native_manager1 =
      new TestNativeWebContentsModalDialogManager(manager, &tracker1);
  TestNativeWebContentsModalDialogManager* native_manager2 =
      new TestNativeWebContentsModalDialogManager(manager, &tracker2);
  TestNativeWebContentsModalDialogManager* native_manager3 =
      new TestNativeWebContentsModalDialogManager(manager, &tracker3);
  manager->ShowDialogWithManager(dialog1,
      scoped_ptr<NativeWebContentsModalDialogManager>(native_manager1).Pass());
  manager->ShowDialogWithManager(dialog2,
      scoped_ptr<NativeWebContentsModalDialogManager>(native_manager2).Pass());
  manager->ShowDialogWithManager(dialog3,
      scoped_ptr<NativeWebContentsModalDialogManager>(native_manager3).Pass());

#if defined(OS_WIN) || defined(USE_AURA)
  manager->SetCloseOnInterstitialPage(dialog2, false);
#else
  // TODO(wittman): Remove this section once Mac is changed to close on
  // interstitial pages by default.
  manager->SetCloseOnInterstitialPage(dialog1, true);
  manager->SetCloseOnInterstitialPage(dialog3, true);
#endif

  test_api->DidAttachInterstitialPage();

  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker1.state_);
  EXPECT_EQ(NativeManagerTracker::SHOWN, tracker2.state_);
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker3.state_);
  EXPECT_TRUE(tracker1.was_shown_);
  EXPECT_TRUE(tracker2.was_shown_);
  EXPECT_FALSE(tracker3.was_shown_);

  native_manager2->StopTracking();
}


// Test that the first dialog is always shown, regardless of the order in which
// dialogs are closed.
TEST_F(WebContentsModalDialogManagerTest, CloseDialogs) {
  // The front dialog is always shown regardless of dialog close order.
  const NativeWebContentsModalDialog dialog1 = MakeFakeDialog();
  const NativeWebContentsModalDialog dialog2 = MakeFakeDialog();
  const NativeWebContentsModalDialog dialog3 = MakeFakeDialog();
  const NativeWebContentsModalDialog dialog4 = MakeFakeDialog();

  NativeManagerTracker tracker1;
  NativeManagerTracker tracker2;
  NativeManagerTracker tracker3;
  NativeManagerTracker tracker4;
  TestNativeWebContentsModalDialogManager* native_manager1 =
      new TestNativeWebContentsModalDialogManager(manager, &tracker1);
  TestNativeWebContentsModalDialogManager* native_manager2 =
      new TestNativeWebContentsModalDialogManager(manager, &tracker2);
  TestNativeWebContentsModalDialogManager* native_manager3 =
      new TestNativeWebContentsModalDialogManager(manager, &tracker3);
  TestNativeWebContentsModalDialogManager* native_manager4 =
      new TestNativeWebContentsModalDialogManager(manager, &tracker4);
  manager->ShowDialogWithManager(dialog1,
      scoped_ptr<NativeWebContentsModalDialogManager>(native_manager1).Pass());
  manager->ShowDialogWithManager(dialog2,
      scoped_ptr<NativeWebContentsModalDialogManager>(native_manager2).Pass());
  manager->ShowDialogWithManager(dialog3,
      scoped_ptr<NativeWebContentsModalDialogManager>(native_manager3).Pass());
  manager->ShowDialogWithManager(dialog4,
      scoped_ptr<NativeWebContentsModalDialogManager>(native_manager4).Pass());

  native_manager1->CloseDialog(dialog1);

  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker1.state_);
  EXPECT_EQ(NativeManagerTracker::SHOWN, tracker2.state_);
  EXPECT_EQ(NativeManagerTracker::NOT_SHOWN, tracker3.state_);
  EXPECT_EQ(NativeManagerTracker::NOT_SHOWN, tracker4.state_);

  native_manager3->CloseDialog(dialog3);

  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker1.state_);
  EXPECT_EQ(NativeManagerTracker::SHOWN, tracker2.state_);
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker3.state_);
  EXPECT_EQ(NativeManagerTracker::NOT_SHOWN, tracker4.state_);
  EXPECT_FALSE(tracker3.was_shown_);

  native_manager2->CloseDialog(dialog2);

  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker1.state_);
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker2.state_);
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker3.state_);
  EXPECT_EQ(NativeManagerTracker::SHOWN, tracker4.state_);
  EXPECT_FALSE(tracker3.was_shown_);

  native_manager4->CloseDialog(dialog4);

  EXPECT_FALSE(manager->IsDialogActive());
  EXPECT_FALSE(delegate->web_contents_blocked());
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker1.state_);
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker2.state_);
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker3.state_);
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker4.state_);
  EXPECT_TRUE(tracker1.was_shown_);
  EXPECT_TRUE(tracker2.was_shown_);
  EXPECT_FALSE(tracker3.was_shown_);
  EXPECT_TRUE(tracker4.was_shown_);
}

// Test that CloseAllDialogs does what it says.
TEST_F(WebContentsModalDialogManagerTest, CloseAllDialogs) {
  const int kWindowCount = 4;
  NativeManagerTracker trackers[kWindowCount];
  TestNativeWebContentsModalDialogManager* native_managers[kWindowCount];
  for (int i = 0; i < kWindowCount; i++) {
    native_managers[i] =
        new TestNativeWebContentsModalDialogManager(manager, &(trackers[i]));
    manager->ShowDialogWithManager(MakeFakeDialog(),
    scoped_ptr<NativeWebContentsModalDialogManager>(
        native_managers[i]).Pass());
  }

  for (int i = 0; i < kWindowCount; i++)
    EXPECT_NE(NativeManagerTracker::CLOSED, trackers[i].state_);

  test_api->CloseAllDialogs();

  EXPECT_FALSE(delegate->web_contents_blocked());
  EXPECT_FALSE(manager->IsDialogActive());
  for (int i = 0; i < kWindowCount; i++)
    EXPECT_EQ(NativeManagerTracker::CLOSED, trackers[i].state_);
}

}  // namespace web_modal
