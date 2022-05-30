/*
 * Copyright (C) 2021 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2021 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugin-fw
 * Created on: 18 дек. 2021 г.
 *
 * lsp-plugin-fw is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugin-fw is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugin-fw. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef LSP_PLUG_IN_PLUG_FW_WRAP_VST2_MAIN_WINNT_H_
#define LSP_PLUG_IN_PLUG_FW_WRAP_VST2_MAIN_WINNT_H_

#include <lsp-plug.in/plug-fw/version.h>

#ifndef LSP_PLUG_IN_VST2_MAIN_IMPL
    #error "This header should not be included directly"
#endif /* LSP_PLUG_IN_VST2_MAIN_IMPL */

#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/common/static.h>
#include <lsp-plug.in/common/status.h>
#include <lsp-plug.in/common/types.h>
#include <lsp-plug.in/plug-fw/wrap/vst2/defs.h>
#include <lsp-plug.in/plug-fw/wrap/vst2/main.h>

#include <lsp-plug.in/3rdparty/steinberg/vst2.h>

#include <lsp-plug.in/stdlib/stdio.h>


// System libraries
#include <sys/types.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <libloaderapi.h>

#include <lsp-plug.in/runtime/LSPString.h>
#include <lsp-plug.in/io/Path.h>

namespace lsp
{
    namespace vst2
    {
        static void *coreDllInstance = NULL;
        static vst2::create_instance_t factory = NULL;

        static vst2::create_instance_t get_main_function(const version_t *required)
        {
            if (factory != NULL)
                return factory;

            HMODULE currentDllModuleHandle = NULL;
            // Static local variable to get address of current DLL library
            static int lpMod = 0;
            WCHAR modulePath[MAX_PATH];
            WCHAR* coreDllDirPath = NULL;
            WCHAR* coreDllFileName = NULL;

            // Get current DLL library file path
            BOOL status = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                                (LPCWSTR) &lpMod,
                                                &currentDllModuleHandle);
            if (status == 0) {
                lsp_trace("Failed to get current module handle: %ld", GetLastError());
                return NULL;
            }
            GetModuleFileNameW(currentDllModuleHandle, modulePath, MAX_PATH);

            // Core library directory part
            LSPString strDir;
            strDir.append_utf16(modulePath);
            io::Path dirPath;
            dirPath.set(&strDir);
            dirPath.parent();
            coreDllDirPath = dirPath.as_string()->clone_utf16();

            // Core library filename part
            char* ptr = NULL;
            asprintf(&ptr, "%s-%d.%d.%d.dll", "lsp-plugins-vst2", required->major, required->minor, required->micro);
            LSPString strFile;
            strFile.append_utf8(ptr);
            coreDllFileName = strFile.clone_utf16();
            
            // Load core library
            SetDllDirectoryW(coreDllDirPath);
            coreDllInstance = LoadLibraryW(coreDllFileName); 
            SetDllDirectoryW(NULL);

            if (coreDllInstance != NULL) {
                lsp_debug("coreDllInstance LOADED");
                // Fetch version function
                module_version_t vf = reinterpret_cast<module_version_t>(GetProcAddress((HINSTANCE)coreDllInstance, LSP_VERSION_FUNC_NAME));
                if (vf == NULL) {
                    lsp_trace("version function %s not found: %ld", LSP_VERSION_FUNC_NAME, GetLastError());
                } else {
                    // Check package version
                    const version_t *ret = vf();
                    if ((ret == NULL) || (ret->branch == NULL)) {
                        lsp_trace("No version or bad version returned, ignoring binary", ret);
                    } else if ( (ret->major != required->major) ||
                             (ret->minor != required->minor) ||
                             (ret->micro != required->micro) ||
                             (strcmp(ret->branch, required->branch) != 0) ) {
                        lsp_trace("wrong version %d.%d.%d '%s' returned, expected %d.%d.%d '%s', ignoring binary",
                                ret->major, ret->minor, ret->micro, ret->branch,
                                required->major, required->minor, required->micro, required->branch
                            );
                    } else {
                        // Get core library entry point
                        vst2::create_instance_t f = reinterpret_cast<vst2::create_instance_t>(GetProcAddress((HINSTANCE)coreDllInstance, VST_MAIN_FUNCTION_STR));     
                        factory = f;
                        if (f == NULL) {
                            lsp_trace("function %s not found: %ld", VST_MAIN_FUNCTION_STR, GetLastError());
                        }
                    }
                }
            } else {
                lsp_trace("Load core library error: %ld", GetLastError());
            }

            // Return factory instance (if present)
            return factory;
        }

        void free_core_library()
        {
            if (coreDllInstance != NULL)
            {
                FreeLibrary((HINSTANCE)coreDllInstance);
                coreDllInstance = NULL;
            }
            factory = NULL;
        }

        static StaticFinalizer finalizer(free_core_library);
    } /* namespace vst2 */
} /* namespace lsp */


// The main function
VST_MAIN(callback)
{
    IF_DEBUG( lsp::debug::redirect("lsp-vst2-loader.log"); );

    // Get VST Version of the Host
    if (!callback (NULL, audioMasterVersion, 0, 0, NULL, 0.0f))
    {
        lsp_error("audioMastercallback failed request");
        return 0;  // old version
    }

    // Check that we need to instantiate the factory
    lsp_trace("Getting factory for plugin %s", VST2_PLUGIN_UID);

    static const lsp::version_t version =
    {
        LSP_PLUGIN_PACKAGE_MAJOR,
        LSP_PLUGIN_PACKAGE_MINOR,
        LSP_PLUGIN_PACKAGE_MICRO,
        LSP_PLUGIN_PACKAGE_BRANCH
    };

    lsp::vst2::create_instance_t f = lsp::vst2::get_main_function(&version);

    // Create effect
    AEffect *effect     = NULL;

    if (f != NULL)
        effect = f(VST2_PLUGIN_UID, callback);
    else
        lsp_error("Could not find VST core library");

    // Return VST AEffect structure
    return effect;
}


#endif /* LSP_PLUG_IN_PLUG_FW_WRAP_VST2_MAIN_WINNT_H_ */
