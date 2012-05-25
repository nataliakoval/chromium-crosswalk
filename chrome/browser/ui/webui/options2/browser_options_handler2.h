// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_BROWSER_OPTIONS_HANDLER2_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_BROWSER_OPTIONS_HANDLER2_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/printing/cloud_print/cloud_print_setup_handler.h"
#include "chrome/browser/search_engines/template_url_service_observer.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/ui/select_file_dialog.h"
#include "chrome/browser/ui/webui/options2/options_ui2.h"
#include "ui/base/models/table_model_observer.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/system/pointer_device_observer.h"
#else
#include "chrome/browser/prefs/pref_set_observer.h"
#endif  // defined(OS_CHROMEOS)

class AutocompleteController;
class CloudPrintSetupHandler;
class CustomHomePagesTableModel;
class TemplateURLService;

namespace options2 {

// Chrome browser options page UI handler.
class BrowserOptionsHandler
    : public OptionsPageUIHandler,
      public CloudPrintSetupHandlerDelegate,
      public ProfileSyncServiceObserver,
      public SelectFileDialog::Listener,
      public ShellIntegration::DefaultWebClientObserver,
#if defined(OS_CHROMEOS)
      public chromeos::system::PointerDeviceObserver::Observer,
#endif
      public TemplateURLServiceObserver {
 public:
  BrowserOptionsHandler();
  virtual ~BrowserOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* values) OVERRIDE;
  virtual void InitializeHandler() OVERRIDE;
  virtual void InitializePage() OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // ProfileSyncServiceObserver implementation.
  virtual void OnStateChanged() OVERRIDE;

  // ShellIntegration::DefaultWebClientObserver implementation.
  virtual void SetDefaultWebClientUIState(
      ShellIntegration::DefaultWebClientUIState state) OVERRIDE;

  // TemplateURLServiceObserver implementation.
  virtual void OnTemplateURLServiceChanged() OVERRIDE;

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // SelectFileDialog::Listener implementation
  virtual void FileSelected(const FilePath& path,
                            int index,
                            void* params) OVERRIDE;

  // CloudPrintSetupHandler::Delegate implementation.
  virtual void OnCloudPrintSetupClosed() OVERRIDE;

#if defined(OS_CHROMEOS)
  // PointerDeviceObserver::Observer implementation.
  virtual void TouchpadExists(bool exists) OVERRIDE;
  virtual void MouseExists(bool exists) OVERRIDE;
#endif

  // Makes this the default browser. Called from WebUI.
  void BecomeDefaultBrowser(const base::ListValue* args);

  // Sets the search engine at the given index to be default. Called from WebUI.
  void SetDefaultSearchEngine(const base::ListValue* args);

  // Enables/disables Instant.
  void EnableInstant(const base::ListValue* args);
  void DisableInstant(const base::ListValue* args);

  // Enables/disables auto-launching of Chrome on computer startup.
  void ToggleAutoLaunch(const base::ListValue* args);

  // Checks (on the file thread) whether the user is in the auto-launch trial
  // and whether Chrome is set to auto-launch at login. Gets a reply on the UI
  // thread (see CheckAutoLaunchCallback). A weak pointer to this is passed in
  // as a parameter to avoid the need to lock between this function and the
  // destructor. |profile_path| is the full path to the current profile.
  void CheckAutoLaunch(base::WeakPtr<BrowserOptionsHandler> weak_this,
                       const FilePath& profile_path);

  // Sets up (on the UI thread) the necessary bindings for toggling auto-launch
  // (if the user is part of the auto-launch and makes sure the HTML UI knows
  // whether Chrome will auto-launch at login.
  void CheckAutoLaunchCallback(bool is_in_auto_launch_group,
                               bool will_launch_at_login);

  // Returns the string ID for the given default browser state.
  int StatusStringIdForState(ShellIntegration::DefaultWebClientState state);

  // Gets the current default browser state, and asynchronously reports it to
  // the WebUI page.
  void UpdateDefaultBrowserState();

  // Updates the UI with the given state for the default browser.
  void SetDefaultBrowserUIString(int status_string_id);

  // Loads the possible default search engine list and reports it to the WebUI.
  void AddTemplateUrlServiceObserver();

  // Creates a list of dictionaries where each dictionary is of the form:
  //   profileInfo = {
  //     name: "Profile Name",
  //     iconURL: "chrome://path/to/icon/image",
  //     filePath: "/path/to/profile/data/on/disk",
  //     isCurrentProfile: false
  //   };
  scoped_ptr<ListValue> GetProfilesInfoList();

  // Sends an array of Profile objects to javascript.
  void SendProfilesInfo();

  // Asynchronously opens a new browser window to create a new profile.
  // |args| is not used.
  void CreateProfile(const base::ListValue* args);

  void ObserveThemeChanged();
  void ThemesReset(const base::ListValue* args);
#if defined(TOOLKIT_GTK)
  void ThemesSetGTK(const base::ListValue* args);
#endif

#if defined(OS_CHROMEOS)
  void UpdateAccountPicture();
#endif

  // Callback for the "selectDownloadLocation" message. This will prompt the
  // user for a destination folder using platform-specific APIs.
  void HandleSelectDownloadLocation(const ListValue* args);

  // Callback for the "autoOpenFileTypesResetToDefault" message. This will
  // remove all auto-open file-type settings.
  void HandleAutoOpenButton(const ListValue* args);

  // Callback for the "metricsReportingCheckboxAction" message. This is called
  // if the user toggles the metrics reporting checkbox.
  void HandleMetricsReportingCheckbox(const ListValue* args);

  // Callback for the "defaultFontSizeAction" message. This is called if the
  // user changes the default font size. |args| is an array that contains
  // one item, the font size as a numeric value.
  void HandleDefaultFontSize(const ListValue* args);

  // Callback for the "defaultZoomFactorAction" message. This is called if the
  // user changes the default zoom factor. |args| is an array that contains
  // one item, the zoom factor as a numeric value.
  void HandleDefaultZoomFactor(const ListValue* args);

  // Callback for the "Check for server certificate revocation" checkbox. This
  // is called if the user toggles the "Check for server certificate revocation"
  // checkbox.
  void HandleCheckRevocationCheckbox(const ListValue* args);

  // Callback for the "Use SSL 3.0" checkbox. This is called if the user toggles
  // the "Use SSL 3.0" checkbox.
  void HandleUseSSL3Checkbox(const ListValue* args);

  // Callback for the "Use TLS 1.0" checkbox. This is called if the user toggles
  // the "Use TLS 1.0" checkbox.
  void HandleUseTLS1Checkbox(const ListValue* args);

#if !defined(OS_CHROMEOS)
  // Callback for the "showNetworkProxySettings" message. This will invoke
  // an appropriate dialog for configuring proxy settings.
  void ShowNetworkProxySettings(const ListValue* args);
#endif

#if !defined(USE_NSS)
  // Callback for the "showManageSSLCertificates" message. This will invoke
  // an appropriate certificate management action based on the platform.
  void ShowManageSSLCertificates(const ListValue* args);
#endif

  // Callback for the Cloud Print manage button. This will open a new
  // tab pointed at the management URL.
  void ShowCloudPrintManagePage(const ListValue* args);

  // Register localized values used by Cloud Print
  void RegisterCloudPrintValues(DictionaryValue* values);

#if !defined(OS_CHROMEOS)
  // Callback for the Sign in to Cloud Print button. This will start
  // the authentication process.
  void ShowCloudPrintSetupDialog(const ListValue* args);

  // Callback for the Disable Cloud Print button. This will sign out
  // of cloud print.
  void HandleDisableCloudPrintConnector(const ListValue* args);

  // Pings the service to send us it's current notion of the enabled state.
  void RefreshCloudPrintStatusFromService();

  // Setup the enabled or disabled state of the cloud print connector
  // management UI.
  void SetupCloudPrintConnectorSection();

  // Remove cloud print connector section if cloud print connector management
  //  UI is disabled.
  void RemoveCloudPrintConnectorSection();
#endif

#if defined(OS_CHROMEOS)
  // Called when the accessibility checkbox values are changed.
  // |args| will contain the checkbox checked state as a string
  // ("true" or "false").
  void SpokenFeedbackChangeCallback(const base::ListValue* args);
  void HighContrastChangeCallback(const base::ListValue* args);
  void ScreenMagnifierChangeCallback(const base::ListValue* args);
  void VirtualKeyboardChangeCallback(const base::ListValue* args);
#endif

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
  // Sets up the checked state for the "Continue running background apps..."
  // checkbox.
  void SetupBackgroundModeSettings();

  // Callback for the "Continue running background apps..." checkbox.
  void HandleBackgroundModeCheckbox(const ListValue* args);
#endif

  // Setup the checked state for the metrics reporting checkbox.
  void SetupMetricsReportingCheckbox();

  // Setup the visibility for the metrics reporting setting.
  void SetupMetricsReportingSettingVisibility();

  // Setup the visibility for the password generation setting.
  void SetupPasswordGenerationSettingVisibility();

  // Setup the font size selector control.
  void SetupFontSizeSelector();

  // Setup the page zoom selector control.
  void SetupPageZoomSelector();

  // Setup the visibility of the reset button.
  void SetupAutoOpenFileTypes();

  // Setup the proxy settings section UI.
  void SetupProxySettingsSection();

  // Setup the checked state for SSL related checkboxes.
  void SetupSSLConfigSettings();

#if defined(OS_CHROMEOS)
  // Setup the accessibility features for ChromeOS.
  void SetupAccessibilityFeatures();
#endif

  // Returns a newly created dictionary with a number of properties that
  // correspond to the status of sync.
  scoped_ptr<DictionaryValue> GetSyncStateDictionary();

  scoped_refptr<ShellIntegration::DefaultBrowserWorker> default_browser_worker_;

  StringPrefMember homepage_;
  BooleanPrefMember default_browser_policy_;

  TemplateURLService* template_url_service_;  // Weak.

  // Used to get |weak_ptr_| to self for use on the File thread.
  base::WeakPtrFactory<BrowserOptionsHandler> weak_ptr_factory_for_file_;
  // Used to post update tasks to the UI thread.
  base::WeakPtrFactory<BrowserOptionsHandler> weak_ptr_factory_for_ui_;

  // True if the multiprofiles switch is enabled.
  bool multiprofile_;

  scoped_refptr<SelectFileDialog> select_folder_dialog_;

#if !defined(OS_CHROMEOS)
  BooleanPrefMember enable_metrics_recording_;
  StringPrefMember cloud_print_connector_email_;
  BooleanPrefMember cloud_print_connector_enabled_;
  bool cloud_print_connector_ui_enabled_;
  scoped_ptr<CloudPrintSetupHandler> cloud_print_setup_handler_;
#endif

  // SSLConfigService prefs.
  BooleanPrefMember rev_checking_enabled_;

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
  BooleanPrefMember background_mode_enabled_;
#endif

  StringPrefMember auto_open_files_;
  IntegerPrefMember default_font_size_;
  DoublePrefMember default_zoom_level_;

#if !defined(OS_CHROMEOS)
  scoped_ptr<PrefSetObserver> proxy_prefs_;
#endif  // !defined(OS_CHROMEOS)

  DISALLOW_COPY_AND_ASSIGN(BrowserOptionsHandler);
};

}  // namespace options2

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_BROWSER_OPTIONS_HANDLER2_H_
