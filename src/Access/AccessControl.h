#pragma once

#include <Access/MultipleAccessStorage.h>
#include <Common/SettingsChanges.h>
#include <Common/ZooKeeper/Common.h>
#include <boost/container/flat_set.hpp>
#include <Access/UsersConfigAccessStorage.h>

#include <memory>


namespace Poco
{
    namespace Net
    {
        class IPAddress;
    }
    namespace Util
    {
        class AbstractConfiguration;
    }
}

namespace DB
{
class ContextAccess;
struct ContextAccessParams;
struct User;
using UserPtr = std::shared_ptr<const User>;
class EnabledRoles;
class RoleCache;
class EnabledRowPolicies;
class RowPolicyCache;
class EnabledQuota;
class QuotaCache;
struct QuotaUsage;
struct SettingsProfilesInfo;
class EnabledSettings;
class SettingsProfilesCache;
class SettingsProfileElements;
class ClientInfo;
class ExternalAuthenticators;
struct Settings;


/// Manages access control entities.
class AccessControl : public MultipleAccessStorage
{
public:
    AccessControl();
    ~AccessControl() override;

    void setUpFromMainConfig(const Poco::Util::AbstractConfiguration & config_, const String & config_path_,
                             const zkutil::GetZooKeeper & get_zookeeper_function_);

    /// Parses access entities from a configuration loaded from users.xml.
    /// This function add UsersConfigAccessStorage if it wasn't added before.
    void setUsersConfig(const Poco::Util::AbstractConfiguration & users_config_);

    /// Adds UsersConfigAccessStorage.
    void addUsersConfigStorage(const Poco::Util::AbstractConfiguration & users_config_);

    void addUsersConfigStorage(const String & storage_name_,
                               const Poco::Util::AbstractConfiguration & users_config_);

    void addUsersConfigStorage(const String & users_config_path_,
                               const String & include_from_path_,
                               const String & preprocessed_dir_,
                               const zkutil::GetZooKeeper & get_zookeeper_function_ = {});

    void addUsersConfigStorage(const String & storage_name_,
                               const String & users_config_path_,
                               const String & include_from_path_,
                               const String & preprocessed_dir_,
                               const zkutil::GetZooKeeper & get_zookeeper_function_ = {});

    void reloadUsersConfigs();
    void startPeriodicReloadingUsersConfigs();
    void stopPeriodicReloadingUsersConfigs();
    /// Loads access entities from the directory on the local disk.
    /// Use that directory to keep created users/roles/etc.
    void addDiskStorage(const String & directory_, bool readonly_ = false);
    void addDiskStorage(const String & storage_name_, const String & directory_, bool readonly_ = false);

    /// Adds MemoryAccessStorage which keeps access entities in memory.
    void addMemoryStorage();
    void addMemoryStorage(const String & storage_name_);

    /// Adds LDAPAccessStorage which allows querying remote LDAP server for user info.
    void addLDAPStorage(const String & storage_name_, const Poco::Util::AbstractConfiguration & config_, const String & prefix_);

    void addReplicatedStorage(const String & storage_name,
                              const String & zookeeper_path,
                              const zkutil::GetZooKeeper & get_zookeeper_function);

    /// Adds storages from <users_directories> config.
    void addStoragesFromUserDirectoriesConfig(const Poco::Util::AbstractConfiguration & config,
                                              const String & key,
                                              const String & config_dir,
                                              const String & dbms_dir,
                                              const String & include_from_path,
                                              const zkutil::GetZooKeeper & get_zookeeper_function);

    /// Adds storages from the main config.
    void addStoragesFromMainConfig(const Poco::Util::AbstractConfiguration & config,
                                   const String & config_path,
                                   const zkutil::GetZooKeeper & get_zookeeper_function);

    /// Sets the default profile's name.
    /// The default profile's settings are always applied before any other profile's.
    void setDefaultProfileName(const String & default_profile_name);

    /// Sets prefixes which should be used for custom settings.
    /// This function also enables custom prefixes to be used.
    void setCustomSettingsPrefixes(const Strings & prefixes);
    void setCustomSettingsPrefixes(const String & comma_separated_prefixes);
    bool isSettingNameAllowed(const std::string_view name) const;
    void checkSettingNameIsAllowed(const std::string_view name) const;

    /// Allows users without password (by default it's allowed).
    void setNoPasswordAllowed(const bool allow_no_password_);
    bool isNoPasswordAllowed() const;

    /// Allows users with plaintext password (by default it's allowed).
    void setPlaintextPasswordAllowed(const bool allow_plaintext_password_);
    bool isPlaintextPasswordAllowed() const;

    /// Enables logic that users without permissive row policies can still read rows using a SELECT query.
    /// For example, if there two users A, B and a row policy is defined only for A, then
    /// if this setting is true the user B will see all rows, and if this setting is false the user B will see no rows.
    void setEnabledUsersWithoutRowPoliciesCanReadRows(bool enable) { users_without_row_policies_can_read_rows = enable; }
    bool isEnabledUsersWithoutRowPoliciesCanReadRows() const { return users_without_row_policies_can_read_rows; }

    UUID authenticate(const Credentials & credentials, const Poco::Net::IPAddress & address) const;
    void setExternalAuthenticatorsConfig(const Poco::Util::AbstractConfiguration & config);

    std::shared_ptr<const ContextAccess> getContextAccess(
        const UUID & user_id,
        const std::vector<UUID> & current_roles,
        bool use_default_roles,
        const Settings & settings,
        const String & current_database,
        const ClientInfo & client_info) const;

    std::shared_ptr<const ContextAccess> getContextAccess(const ContextAccessParams & params) const;

    std::shared_ptr<const EnabledRoles> getEnabledRoles(
        const std::vector<UUID> & current_roles,
        const std::vector<UUID> & current_roles_with_admin_option) const;

    std::shared_ptr<const EnabledRowPolicies> getEnabledRowPolicies(
        const UUID & user_id,
        const boost::container::flat_set<UUID> & enabled_roles) const;

    std::shared_ptr<const EnabledRowPolicies> tryGetDefaultRowPolicies(const UUID & user_id) const;

    std::shared_ptr<const EnabledQuota> getEnabledQuota(
        const UUID & user_id,
        const String & user_name,
        const boost::container::flat_set<UUID> & enabled_roles,
        const Poco::Net::IPAddress & address,
        const String & forwarded_address,
        const String & custom_quota_key) const;

    std::vector<QuotaUsage> getAllQuotasUsage() const;

    std::shared_ptr<const EnabledSettings> getEnabledSettings(
        const UUID & user_id,
        const SettingsProfileElements & settings_from_user,
        const boost::container::flat_set<UUID> & enabled_roles,
        const SettingsProfileElements & settings_from_enabled_roles) const;

    std::shared_ptr<const SettingsProfilesInfo> getSettingsProfileInfo(const UUID & profile_id);

    const ExternalAuthenticators & getExternalAuthenticators() const;

private:
    class ContextAccessCache;
    class CustomSettingsPrefixes;

    std::unique_ptr<ContextAccessCache> context_access_cache;
    std::unique_ptr<RoleCache> role_cache;
    std::unique_ptr<RowPolicyCache> row_policy_cache;
    std::unique_ptr<QuotaCache> quota_cache;
    std::unique_ptr<SettingsProfilesCache> settings_profiles_cache;
    std::unique_ptr<ExternalAuthenticators> external_authenticators;
    std::unique_ptr<CustomSettingsPrefixes> custom_settings_prefixes;
    std::atomic_bool allow_plaintext_password = true;
    std::atomic_bool allow_no_password = true;
    std::atomic_bool users_without_row_policies_can_read_rows = false;
};

}
