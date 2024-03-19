/*
 * Copyright 2022-2023 Blueberry d.o.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <coretypes/intfs.h>
#include <opendaq/permission_manager.h>
#include <opendaq/permission_manager_ptr.h>
#include <opendaq/permission_manager_private.h>
#include <opendaq/permission_manager_private_ptr.h>
#include <coretypes/weakrefptr.h>

BEGIN_NAMESPACE_OPENDAQ

class PermissionManagerImpl : public ImplementationOfWeak<IPermissionManager, IPermissionManagerPrivate>
{
public:
    explicit PermissionManagerImpl(const PermissionManagerPtr& parent);

    ErrCode INTERFACE_FUNC setPermissionConfig(IPermissionConfig* permissionConfig) override;
    ErrCode INTERFACE_FUNC isAuthorized(IUser* user, Permission permission, Bool* authorizedOut) override;

protected:
    ErrCode INTERFACE_FUNC setParent(IPermissionManager* parentManager) override;
    ErrCode INTERFACE_FUNC addChildManager(IPermissionManager* childManager) override;
    ErrCode INTERFACE_FUNC removeChildManager(IPermissionManager* childManager) override;
    ErrCode INTERFACE_FUNC getPermissionConfig(IPermissionConfig** permisisonConfigOut) override;
    ErrCode INTERFACE_FUNC updateInheritedPermissions() override;

private:
    void updateChildPermissions();
    PermissionManagerPrivatePtr getParentManager();

    WeakRefPtr<IPermissionManager> parent;
    DictPtr<IPermissionManager, Bool> children;
    PermissionConfigPtr config;
    PermissionConfigPtr localConfig;
};

END_NAMESPACE_OPENDAQ