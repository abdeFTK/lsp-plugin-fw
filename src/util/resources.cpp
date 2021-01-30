/*
 * Copyright (C) 2021 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2021 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugin-fw
 * Created on: 30 янв. 2021 г.
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

#include <lsp-plug.in/common/types.h>
#include <lsp-plug.in/common/status.h>
#include <lsp-plug.in/lltl/parray.h>
#include <lsp-plug.in/lltl/pphash.h>
#include <lsp-plug.in/lltl/phashset.h>
#include <lsp-plug.in/runtime/LSPString.h>
#include <lsp-plug.in/io/Dir.h>
#include <lsp-plug.in/io/Path.h>
#include <lsp-plug.in/io/PathPattern.h>

namespace lsp
{
    namespace resource
    {
        typedef struct context_t
        {
            lltl::pphash<LSPString, LSPString> themes;      // XML files (themes)
            lltl::pphash<LSPString, LSPString> ui;          // XML files (UI)
            lltl::pphash<LSPString, LSPString> presets;     // Preset files
        } context_t;

        /**
         * File handler function
         * @param ctx context
         * @param relative relative path to the file
         * @param full full path to the file
         * @return status of operation
         */
        typedef status_t (* file_handler_t)(context_t *ctx, const LSPString *relative, const LSPString *full);

        void drop_paths(lltl::parray<LSPString> *paths)
        {
            for (size_t i=0, n=paths->size(); i<n; ++i)
            {
                LSPString *item = paths->uget(i);
                if (item != NULL)
                    delete item;
            }
            paths->flush();
        }

        void destroy_context(context_t *ctx)
        {
            lltl::parray<LSPString> paths;

            // Drop themes
            ctx->themes.values(&paths);
            ctx->themes.flush();
            drop_paths(&paths);

            // Drop UI files
            ctx->ui.values(&paths);
            ctx->ui.flush();
            drop_paths(&paths);

            // Drop preset files
            ctx->presets.values(&paths);
            ctx->presets.flush();
            drop_paths(&paths);
        }

        status_t theme_handler(context_t *ctx, const LSPString *relative, const LSPString *full)
        {
            return STATUS_OK;
        }

        status_t ui_handler(context_t *ctx, const LSPString *relative, const LSPString *full)
        {
            return STATUS_OK;
        }

        status_t preset_handler(context_t *ctx, const LSPString *relative, const LSPString *full)
        {
            return STATUS_OK;
        }

        status_t i18n_handler(context_t *ctx, const LSPString *relative, const LSPString *full)
        {
            return STATUS_OK;
        }

        status_t scan_files(
            const io::Path *base, const LSPString *child, const io::PathPattern *pattern,
            context_t *ctx, file_handler_t handler
        )
        {
            status_t res;
            io::Path path, rel, full;
            io::Dir fd;
            io::fattr_t fa;
            LSPString item, *subdir;
            lltl::parray<LSPString> subdirs;

            // Open directory
            if ((res = path.set(base, child)) != STATUS_OK)
                return res;
            if ((res = fd.open(&path)) != STATUS_OK)
                return STATUS_OK;

            while (true)
            {
                // Read directory entry
                if ((res = fd.reads(&item, &fa)) != STATUS_OK)
                {
                    // Close directory
                    fd.close();

                    // Analyze status
                    if (res != STATUS_EOF)
                    {
                        drop_paths(&subdirs);
                        fprintf(stderr, "Could not read directory entry for %s\n", path.as_native());
                        return res;
                    }
                    break;
                }

                // This should be a directory and not dots
                if (fa.type == io::fattr_t::FT_DIRECTORY)
                {
                    // Add child directory to list
                    if (!io::Path::is_dots(&item))
                    {
                        // Build directory paths
                        if ((res = rel.set(child, &item)) != STATUS_OK)
                            return res;

                        // Create a copy
                        subdir = rel.as_string()->clone();
                        if (subdir == NULL)
                        {
                            drop_paths(&subdirs);
                            return res;
                        }

                        // Add to list
                        if (!subdirs.add(subdir))
                        {
                            delete subdir;
                            drop_paths(&subdirs);
                            return res;
                        }
                    }
                }
                else if (pattern->test(&item))
                {
                    // Build file paths
                    if ((res = rel.set(child, &item)) != STATUS_OK)
                        return res;
                    if ((res = full.set(base, &rel)) != STATUS_OK)
                        return res;

                    // Handle the file
                    printf("  found file: %s\n", rel.as_native());
                    if ((res = handler(ctx, rel.as_string(), full.as_string())) != STATUS_OK)
                        return res;
                }
            }

            // Process each sub-directory
            for (size_t i=0, n=subdirs.size(); i<n; ++i)
            {
                subdir = subdirs.uget(i);
                if ((res = scan_files(base, subdir, pattern, ctx, handler)) != STATUS_OK)
                {
                    drop_paths(&subdirs);
                    return res;
                }
            }

            // Drop sub-directories
            drop_paths(&subdirs);

            return STATUS_OK;
        }

        status_t scan_files(
            const io::Path *base, const char *child, const char *pattern,
            context_t *ctx, file_handler_t handler
        )
        {
            status_t res;
            LSPString cpath;
            io::PathPattern pat;

            if (!cpath.set_utf8(child))
                return STATUS_NO_MEM;
            if ((res = pat.set(pattern)) != STATUS_OK)
                return res;

            return scan_files(base, &cpath, &pat, ctx, handler);
        }

        status_t scan_resources(const LSPString *path, context_t *ctx)
        {
            status_t res;

            // Compute the base path
            io::Path base, full;
            if ((res = base.set(path, "res/main")) != STATUS_OK)
                return res;
            if ((res = full.set(path, &base)) != STATUS_OK)
                return res;

            printf("Scanning directory %s\n", full.as_native());

            // Lookup for specific resources
            if ((res = scan_files(&base, "themes", "*.xml", ctx, theme_handler)) != STATUS_OK)
                return res;
            if ((res = scan_files(&base, "ui", "*.xml", ctx, ui_handler)) != STATUS_OK)
                return res;
            if ((res = scan_files(&base, "presets", "*.preset", ctx, preset_handler)) != STATUS_OK)
                return res;
            if ((res = scan_files(&base, "i18n", "*.json", ctx, i18n_handler)) != STATUS_OK)
                return res;

            return STATUS_OK;
        }

        status_t lookup_path(const char *path, context_t *ctx)
        {
            status_t res;
            io::Dir fd;
            io::Path dir, resdir;
            io::PathPattern pat;
            io::fattr_t fa;
            LSPString pattern, item, resource, *respath;
            lltl::parray<LSPString> matched;

            // Prepare search structures
            if ((res = dir.set(path)) != STATUS_OK)
                return res;
            if ((res = dir.canonicalize()) != STATUS_OK)
                return res;
            if ((res = dir.remove_last(&pattern)) != STATUS_OK)
                return res;

            // Check if there is no pattern
            if (pattern.length() <= 0)
            {
                if ((res = scan_resources(dir.as_string(), ctx)) != STATUS_OK)
                    return res;
            }

            // Parse pattern
            if ((res = pat.set(&pattern)) != STATUS_OK)
                return res;

            // Scan directory for entries
            if ((res = fd.open(&dir)) != STATUS_OK)
                return res;

            while (true)
            {
                // Read directory entry
                if ((res = fd.reads(&item, &fa)) != STATUS_OK)
                {
                    // Close directory
                    fd.close();

                    // Analyze status
                    if (res != STATUS_EOF)
                    {
                        drop_paths(&matched);
                        fprintf(stderr, "Could not read directory entry for %s\n", dir.as_native());
                        return res;
                    }
                    break;
                }

                // This should be a directory and not dots
                if ((fa.type != io::fattr_t::FT_DIRECTORY) &&
                    (io::Path::is_dots(&item)))
                    continue;

                // Match the directory to the pattern
                if (!pattern.match(&item))
                    continue;

                // We found a child resource directory, add to list
                if ((res = resdir.set(&dir, &item)) != STATUS_OK)
                {
                    drop_paths(&matched);
                    return res;
                }

                // Create a copy
                respath = resdir.as_string()->clone();
                if (respath == NULL)
                {
                    drop_paths(&matched);
                    return res;
                }

                // Add to list
                if (!matched.add(respath))
                {
                    delete respath;
                    drop_paths(&matched);
                    return res;
                }
            }

            // Now we have the full list of matched directories
            for (size_t i=0, n=matched.size(); i<n; ++i)
            {
                respath = matched.uget(i);
                printf("Scanning directory %s for resources\n", respath->get_native());
                if ((res = scan_resources(respath, ctx)) != STATUS_OK)
                {
                    drop_paths(&matched);
                    return res;
                }
            }

            drop_paths(&matched);

            return STATUS_OK;
        }

        status_t build_repository(const char *destdir, const char *const *paths, size_t npaths)
        {
            context_t ctx;
            status_t res;

            for (size_t i=0; i<npaths; ++i)
            {
                if ((res = lookup_path(paths[i], &ctx)) != STATUS_OK)
                {
                    destroy_context(&ctx);
                    return res;
                }
            }

            return STATUS_OK;
        }
    }
}

#ifndef LSP_IDE_DEBUG
int main(int argc, const char **argv)
{
    if (argc < 2)
        fprintf(stderr, "required destination path");
    lsp::status_t res = lsp::resource::build_repository(argv[1], &argv[2], argc - 2);
    if (res != lsp::STATUS_OK)
        fprintf(stderr, "Error while generating build files, code=%d", int(res));

    return res;
}
#endif /* LSP_IDE_DEBUG */

