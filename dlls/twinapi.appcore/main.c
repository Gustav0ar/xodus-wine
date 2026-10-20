/*
 * Copyright (C) 2023 Mohamad Al-Jaf
 * Copyright (C) 2026 Gustavo Rosa
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdlib.h>

#include "initguid.h"
#include "private.h"
#include "wine/list.h"

WINE_DEFAULT_DEBUG_CHANNEL(twinapi);

struct _APPCONSTRAIN_REGISTRATION
{
    struct list entry;
    PAPPCONSTRAIN_CHANGE_ROUTINE routine;
    void *context;
};

static SRWLOCK constrained_lock = SRWLOCK_INIT;
static struct list constrained_registrations = LIST_INIT(constrained_registrations);

/***********************************************************************
 *           RegisterAppConstrainedChangeNotification (twinapi.appcore.@)
 */
ULONG WINAPI RegisterAppConstrainedChangeNotification( PAPPCONSTRAIN_CHANGE_ROUTINE routine, void *context, PAPPCONSTRAIN_REGISTRATION *reg )
{
    PAPPCONSTRAIN_REGISTRATION registration;

    TRACE( "routine %p, context %p, reg %p.\n", routine, context, reg );
    if (!reg) return ERROR_INVALID_PARAMETER;
    *reg = NULL;
    if (!routine) return ERROR_INVALID_PARAMETER;
    if (!(registration = malloc( sizeof(*registration) ))) return ERROR_NOT_ENOUGH_MEMORY;
    registration->routine = routine;
    registration->context = context;

    /* Desktop processes remain unconstrained; do not synthesize notifications. */
    AcquireSRWLockExclusive( &constrained_lock );
    list_add_tail( &constrained_registrations, &registration->entry );
    ReleaseSRWLockExclusive( &constrained_lock );
    *reg = registration;
    return ERROR_SUCCESS;
}

/***********************************************************************
 *           RegisterAppStateChangeNotification (twinapi.appcore.@)
 */
ULONG WINAPI RegisterAppStateChangeNotification( PAPPSTATE_CHANGE_ROUTINE routine, void *context, PAPPSTATE_REGISTRATION *reg )
{
    FIXME( "routine %p, context %p, reg %p - stub.\n", routine, context, reg );
    return ERROR_CALL_NOT_IMPLEMENTED;
}

/***********************************************************************
 *           UnregisterAppConstrainedChangeNotification (twinapi.appcore.@)
 */
void WINAPI UnregisterAppConstrainedChangeNotification( PAPPCONSTRAIN_REGISTRATION reg )
{
    PAPPCONSTRAIN_REGISTRATION registration, found = NULL;

    TRACE( "reg %p.\n", reg );
    AcquireSRWLockExclusive( &constrained_lock );
    LIST_FOR_EACH_ENTRY( registration, &constrained_registrations,
                         struct _APPCONSTRAIN_REGISTRATION, entry )
    {
        if (registration != reg) continue;
        list_remove( &registration->entry );
        found = registration;
        break;
    }
    ReleaseSRWLockExclusive( &constrained_lock );
    free( found );
}

/***********************************************************************
 *           UnregisterAppStateChangeNotification (twinapi.appcore.@)
 */
void WINAPI UnregisterAppStateChangeNotification( PAPPSTATE_REGISTRATION reg )
{
    FIXME( "reg %p - stub.\n", reg );
}

BOOL WINAPI DllMain( HINSTANCE instance, DWORD reason, void *reserved )
{
    PAPPCONSTRAIN_REGISTRATION registration, next;

    if (reason == DLL_PROCESS_DETACH && !reserved)
    {
        LIST_FOR_EACH_ENTRY_SAFE( registration, next, &constrained_registrations,
                                 struct _APPCONSTRAIN_REGISTRATION, entry )
        {
            list_remove( &registration->entry );
            free( registration );
        }
    }
    return TRUE;
}

HRESULT WINAPI DllGetClassObject( REFCLSID clsid, REFIID riid, void **out )
{
    FIXME( "clsid %s, riid %s, out %p stub!\n", debugstr_guid(clsid), debugstr_guid(riid), out );
    return CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT WINAPI DllGetActivationFactory( HSTRING classid, IActivationFactory **factory )
{
    const WCHAR *buffer = WindowsGetStringRawBuffer( classid, NULL );

    TRACE( "class %s, factory %p.\n", debugstr_hstring(classid), factory );

    *factory = NULL;

    if (!wcscmp( buffer, RuntimeClass_Windows_Security_ExchangeActiveSyncProvisioning_EasClientDeviceInformation ))
        IActivationFactory_QueryInterface( client_device_information_factory, &IID_IActivationFactory, (void **)factory );
    else if (!wcscmp( buffer, RuntimeClass_Windows_System_Profile_AnalyticsInfo ))
        IActivationFactory_QueryInterface( analytics_info_factory, &IID_IActivationFactory, (void **)factory );
    else if (!wcscmp( buffer, RuntimeClass_Windows_System_UserProfile_AdvertisingManager ))
        IActivationFactory_QueryInterface( advertising_manager_factory, &IID_IActivationFactory, (void **)factory );
    else if (!wcscmp( buffer, RuntimeClass_Windows_UI_ViewManagement_ApplicationView ))
        IActivationFactory_QueryInterface( application_view_factory, &IID_IActivationFactory, (void **)factory );
    else if (!wcscmp( buffer, RuntimeClass_Windows_ApplicationModel_DataTransfer_DataTransferManager ))
        IActivationFactory_QueryInterface( data_transfer_manager_statics_factory, &IID_IActivationFactory, (void **)factory );

    if (*factory) return S_OK;
    return CLASS_E_CLASSNOTAVAILABLE;
}
