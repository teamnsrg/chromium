// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/chrome_component_updater_configurator.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/sys_string_conversions.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/component_updater_utils.h"
#include "chrome/browser/component_updater/recovery_improved_component_installer.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/update_client/chrome_update_query_params_delegate.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/component_updater_command_line_config_policy.h"
#include "components/component_updater/configurator_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/update_query_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

#if defined(OS_WIN)
#include "base/enterprise_util.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/google_update_settings.h"
#endif

namespace component_updater {

namespace {

class ChromeConfigurator : public update_client::Configurator {
 public:
  ChromeConfigurator(const base::CommandLine* cmdline,
                     PrefService* pref_service);

  // update_client::Configurator overrides.
  int InitialDelay() const override;
  int NextCheckDelay() const override;
  int OnDemandDelay() const override;
  int UpdateDelay() const override;
  std::vector<GURL> UpdateUrl() const override;
  std::vector<GURL> PingUrl() const override;
  std::string GetProdId() const override;
  base::Version GetBrowserVersion() const override;
  std::string GetChannel() const override;
  std::string GetBrand() const override;
  std::string GetLang() const override;
  std::string GetOSLongName() const override;
  base::flat_map<std::string, std::string> ExtraRequestParams() const override;
  std::string GetDownloadPreference() const override;
  scoped_refptr<network::SharedURLLoaderFactory> URLLoaderFactory()
      const override;
  std::unique_ptr<service_manager::Connector> CreateServiceManagerConnector()
      const override;
  bool EnabledDeltas() const override;
  bool EnabledComponentUpdates() const override;
  bool EnabledBackgroundDownloader() const override;
  bool EnabledCupSigning() const override;
  PrefService* GetPrefService() const override;
  update_client::ActivityDataService* GetActivityDataService() const override;
  bool IsPerUserInstall() const override;
  std::vector<uint8_t> GetRunActionKeyHash() const override;
  std::string GetAppGuid() const override;
  std::unique_ptr<update_client::ProtocolHandlerFactory>
  GetProtocolHandlerFactory() const override;
  update_client::RecoveryCRXElevator GetRecoveryCRXElevator() const override;

 private:
  friend class base::RefCountedThreadSafe<ChromeConfigurator>;

  ConfiguratorImpl configurator_impl_;
  PrefService* pref_service_;  // This member is not owned by this class.

  ~ChromeConfigurator() override {}
};

// Allows the component updater to use non-encrypted communication with the
// update backend. The security of the update checks is enforced using
// a custom message signing protocol and it does not depend on using HTTPS.
ChromeConfigurator::ChromeConfigurator(const base::CommandLine* cmdline,
                                       PrefService* pref_service)
    : configurator_impl_(ComponentUpdaterCommandLineConfigPolicy(cmdline),
                         false),
      pref_service_(pref_service) {
  DCHECK(pref_service_);
}

int ChromeConfigurator::InitialDelay() const {
  return configurator_impl_.InitialDelay();
}

int ChromeConfigurator::NextCheckDelay() const {
  return configurator_impl_.NextCheckDelay();
}

int ChromeConfigurator::OnDemandDelay() const {
  return configurator_impl_.OnDemandDelay();
}

int ChromeConfigurator::UpdateDelay() const {
  return configurator_impl_.UpdateDelay();
}

std::vector<GURL> ChromeConfigurator::UpdateUrl() const {
  return configurator_impl_.UpdateUrl();
}

std::vector<GURL> ChromeConfigurator::PingUrl() const {
  return configurator_impl_.PingUrl();
}

std::string ChromeConfigurator::GetProdId() const {
  return update_client::UpdateQueryParams::GetProdIdString(
      update_client::UpdateQueryParams::ProdId::CHROME);
}

base::Version ChromeConfigurator::GetBrowserVersion() const {
  return configurator_impl_.GetBrowserVersion();
}

std::string ChromeConfigurator::GetChannel() const {
  return chrome::GetChannelName();
}

std::string ChromeConfigurator::GetBrand() const {
  std::string brand;
  google_brand::GetBrand(&brand);
  return brand;
}

std::string ChromeConfigurator::GetLang() const {
  return ChromeUpdateQueryParamsDelegate::GetLang();
}

std::string ChromeConfigurator::GetOSLongName() const {
  return configurator_impl_.GetOSLongName();
}

base::flat_map<std::string, std::string>
ChromeConfigurator::ExtraRequestParams() const {
  return configurator_impl_.ExtraRequestParams();
}

std::string ChromeConfigurator::GetDownloadPreference() const {
#if defined(OS_WIN)
  // This group policy is supported only on Windows and only for enterprises.
  return base::IsMachineExternallyManaged()
             ? base::SysWideToUTF8(
                   GoogleUpdateSettings::GetDownloadPreference())
             : std::string();
#else
  return std::string();
#endif
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeConfigurator::URLLoaderFactory() const {
  SystemNetworkContextManager* system_network_context_manager =
      g_browser_process->system_network_context_manager();
  // Manager will be null if called from InitializeForTesting.
  if (!system_network_context_manager)
    return nullptr;
  return system_network_context_manager->GetSharedURLLoaderFactory();
}

std::unique_ptr<service_manager::Connector>
ChromeConfigurator::CreateServiceManagerConnector() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->Clone();
}

bool ChromeConfigurator::EnabledDeltas() const {
  return configurator_impl_.EnabledDeltas();
}

bool ChromeConfigurator::EnabledComponentUpdates() const {
  return pref_service_->GetBoolean(prefs::kComponentUpdatesEnabled);
}

bool ChromeConfigurator::EnabledBackgroundDownloader() const {
  return configurator_impl_.EnabledBackgroundDownloader();
}

bool ChromeConfigurator::EnabledCupSigning() const {
  return configurator_impl_.EnabledCupSigning();
}

PrefService* ChromeConfigurator::GetPrefService() const {
  return pref_service_;
}

update_client::ActivityDataService* ChromeConfigurator::GetActivityDataService()
    const {
  return nullptr;
}

bool ChromeConfigurator::IsPerUserInstall() const {
  return component_updater::IsPerUserInstall();
}

std::vector<uint8_t> ChromeConfigurator::GetRunActionKeyHash() const {
  return configurator_impl_.GetRunActionKeyHash();
}

std::string ChromeConfigurator::GetAppGuid() const {
#if defined(OS_WIN)
  return install_static::UTF16ToUTF8(install_static::GetAppGuid());
#else
  return configurator_impl_.GetAppGuid();
#endif
}

std::unique_ptr<update_client::ProtocolHandlerFactory>
ChromeConfigurator::GetProtocolHandlerFactory() const {
  return configurator_impl_.GetProtocolHandlerFactory();
}

update_client::RecoveryCRXElevator ChromeConfigurator::GetRecoveryCRXElevator()
    const {
#if defined(GOOGLE_CHROME_BUILD) && defined(OS_WIN)
  return base::BindOnce(&RunRecoveryCRXElevated);
#else
  return {};
#endif
}

}  // namespace

void RegisterPrefsForChromeComponentUpdaterConfigurator(
    PrefRegistrySimple* registry) {
  // The component updates are enabled by default, if the preference is not set.
  registry->RegisterBooleanPref(prefs::kComponentUpdatesEnabled, true);
}

scoped_refptr<update_client::Configurator>
MakeChromeComponentUpdaterConfigurator(
    const base::CommandLine* cmdline,
    PrefService* pref_service) {
  return base::MakeRefCounted<ChromeConfigurator>(cmdline, pref_service);
}

}  // namespace component_updater
