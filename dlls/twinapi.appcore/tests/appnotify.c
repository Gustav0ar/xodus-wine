/*
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

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "appnotify.h"
#include "wine/test.h"

struct notification_api
{
    ULONG (WINAPI *register_change)(PAPPCONSTRAIN_CHANGE_ROUTINE, void *,
                                   PAPPCONSTRAIN_REGISTRATION *);
    void (WINAPI *unregister_change)(PAPPCONSTRAIN_REGISTRATION);
};

static void __cdecl notification_changed( BOOLEAN state, void *context )
{
}

static DWORD WINAPI registration_thread( void *context )
{
    struct notification_api *api = context;
    unsigned int i;
    PAPPCONSTRAIN_REGISTRATION registration;
    ULONG status;

    for (i = 0; i < 64; ++i)
    {
        registration = NULL;
        status = api->register_change( notification_changed, context, &registration );
        ok( status == ERROR_SUCCESS, "Got status %lu.\n", status );
        ok( registration != NULL, "Expected a registration handle.\n" );
        if (!status && registration) api->unregister_change( registration );
    }
    return 0;
}

static void test_constrained_notifications(void)
{
    const char *module_name = "api-ms-win-core-psm-appnotify-l1-1-1.dll";
    struct notification_api api;
    PAPPCONSTRAIN_REGISTRATION registrations[32] = {0};
    HANDLE threads[4];
    HMODULE module;
    unsigned int i, j, count = 0;
    DWORD wait;
    ULONG status;

    if (!(module = LoadLibraryA( module_name )))
    {
        win_skip( "%s is unavailable.\n", module_name );
        return;
    }
    api.register_change = (void *)GetProcAddress( module, "RegisterAppConstrainedChangeNotification" );
    api.unregister_change =
        (void *)GetProcAddress( module, "UnregisterAppConstrainedChangeNotification" );
    if (!api.register_change || !api.unregister_change)
    {
        win_skip( "Notification functions are unavailable.\n" );
        goto free_module;
    }

    for (i = 0; i < ARRAY_SIZE(registrations); ++i)
    {
        status = api.register_change( notification_changed, NULL, &registrations[i] );
        ok( status == ERROR_SUCCESS, "Got status %lu.\n", status );
        if (status) goto unregister;
        ok( registrations[i] != NULL, "Expected a registration handle.\n" );
        for (j = 0; j < i; ++j)
            ok( registrations[i] != registrations[j],
                "Registrations %u and %u share a handle.\n", i, j );
    }

    /* Remove alternating entries before testing concurrent register/unregister. */
    for (i = 0; i < ARRAY_SIZE(registrations); i += 2)
    {
        api.unregister_change( registrations[i] );
        registrations[i] = NULL;
    }
    for (i = 0; i < ARRAY_SIZE(threads); ++i)
    {
        HANDLE thread = CreateThread( NULL, 0, registration_thread, &api, 0, NULL );
        ok( thread != NULL, "CreateThread failed, error %lu.\n", GetLastError() );
        if (thread) threads[count++] = thread;
    }
    if (count)
    {
        wait = WaitForMultipleObjects( count, threads, TRUE, INFINITE );
        ok( wait == WAIT_OBJECT_0, "Got wait result %#lx.\n", wait );
        for (i = 0; i < count; ++i) CloseHandle( threads[i] );
    }

unregister:
    for (i = 0; i < ARRAY_SIZE(registrations); ++i)
        if (registrations[i]) api.unregister_change( registrations[i] );
free_module:
    FreeLibrary( module );
}

START_TEST(appnotify)
{
    test_constrained_notifications();
}
