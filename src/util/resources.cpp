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
#include <lsp-plug.in/fmt/json/dom.h>

namespace lsp
{
    namespace resource
    {
        typedef struct context_t
        {
            lltl::pphash<LSPString, LSPString>  schema;         // XML files (schemas)
            lltl::pphash<LSPString, LSPString>  ui;             // XML files (UI)
            lltl::pphash<LSPString, LSPString>  preset;         // Preset files
            lltl::pphash<LSPString, json::Node> i18n;           // INternationalization data
            lltl::pphash<LSPString, LSPString>  other;          // Other files
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
            lltl::parray<json::Node> i18n;

            // Drop themes
            ctx->schema.values(&paths);
            ctx->schema.flush();
            drop_paths(&paths);

            // Drop UI files
            ctx->ui.values(&paths);
            ctx->ui.flush();
            drop_paths(&paths);

            // Drop preset files
            ctx->preset.values(&paths);
            ctx->preset.flush();
            drop_paths(&paths);

            // Drop internationalization
            ctx->i18n.values(&i18n);
            ctx->i18n.flush();
            for (size_t i=0, n=i18n.size(); i<n; ++i)
            {
                json::Node *node = i18n.get(i);
                if (node != NULL)
                    delete node;
            }
            i18n.flush();

            // Drop other files
            ctx->other.values(&paths);
            ctx->other.flush();
            drop_paths(&paths);
        }

        status_t add_unique_file(lltl::pphash<LSPString, LSPString> *dst, const LSPString *relative, const LSPString *full)
        {
            LSPString *path = dst->get(relative);
            if (path != NULL)
            {
                fprintf(stderr,
                        "Resource file '%s' conflicts with '%s', can not proceed",
                        full->get_native(), path->get_native()
                );
                return STATUS_DUPLICATED;
            }

            if ((path = full->clone()) == NULL)
                return STATUS_NO_MEM;

            if (!dst->create(relative, path))
            {
                delete path;
                return STATUS_NO_MEM;
            }

            return STATUS_OK;
        }

        status_t merge_i18n(LSPString *path, json::Node *dst, const json::Node *src, const LSPString *full)
        {
            status_t res;
            lltl::parray<LSPString> fields;
            json::Object sjo = src;
            json::Object djo = dst;

            // Obtain the whole list of fields
            if ((res = sjo.fields(&fields)) != STATUS_OK)
                return res;
            fields.qsort();

            for (size_t i=0, n=fields.size(); i<n; ++i)
            {
                const LSPString *field = fields.uget(i);
                if (field == NULL)
                {
                    fprintf(stderr, "  file '%s': corrupted JSON object\n", full->get_native());
                    return STATUS_BAD_STATE;
                }

                // Form the path of property
                size_t len = path->length();
                if (len > 0)
                {
                    if (!path->append_ascii("->"))
                        return STATUS_NO_MEM;
                }
                if (!path->append(field))
                    return STATUS_NO_MEM;

                json::Node sjn = sjo.get(field);
                if (sjn.is_object())
                {
                    json::Node obj = djo.get(field);
                    if (!obj.is_object())
                    {
                        obj = json::Object::build();
                        if ((res = djo.set(field, &obj)) != STATUS_OK)
                            return res;
                    }
                    if ((res = merge_i18n(path, &obj, &sjn, full)) != STATUS_OK)
                        return res;
                }
                else if (sjn.is_string())
                {
                    if (djo.contains(field))
                    {
                        fprintf(stderr, "  file '%s': overrided property '%s'\n", full->get_native(), path->get_native());
                        return STATUS_CORRUPTED;
                    }
                    if ((res = djo.set(field, sjn)) != STATUS_OK)
                        return res;
                }
                else
                {
                    fprintf(stderr, "  file '%s': unsupported object type '%s'\n", full->get_native(), sjn.stype());
                    return STATUS_BAD_STATE;
                }

                // Truncate the path
                path->set_length(len);
            }

            return STATUS_OK;
        }

        status_t schema_handler(context_t *ctx, const LSPString *relative, const LSPString *full)
        {
            return add_unique_file(&ctx->schema, relative, full);
        }

        status_t ui_handler(context_t *ctx, const LSPString *relative, const LSPString *full)
        {
            return add_unique_file(&ctx->ui, relative, full);
        }

        status_t preset_handler(context_t *ctx, const LSPString *relative, const LSPString *full)
        {
            return add_unique_file(&ctx->preset, relative, full);
        }

        status_t other_handler(context_t *ctx, const LSPString *relative, const LSPString *full)
        {
            return add_unique_file(&ctx->other, relative, full);
        }

        status_t i18n_handler(context_t *ctx, const LSPString *relative, const LSPString *full)
        {
            status_t res;
            LSPString path;
            json::Node *node = new json::Node();
            if (node == NULL)
                return STATUS_NO_MEM;

            if ((res = json::dom_load(full, node, json::JSON_LEGACY, "UTF-8")) != STATUS_OK)
            {
                fprintf(stderr, "  file '%s': failed to load, error code=%d\n", full->get_native(), int(res));
                delete node;
                return res;
            }

            if (!node->is_object())
            {
                fprintf(stderr, "  file '%s': not a JSON object\n", full->get_native());
                delete node;
                return STATUS_CORRUPTED;
            }

            // Check that node previously existed
            json::Node *dst = ctx->i18n.get(relative);
            if (dst == NULL)
                res = (ctx->i18n.create(relative, node)) ? STATUS_OK : STATUS_NO_MEM;
            else
                res = merge_i18n(&path, dst, node, full);

            return res;
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

                        printf("  found  dir: %s\n", rel.as_native());
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
            const io::Path *base, const LSPString *child, const char *pattern,
            context_t *ctx, file_handler_t handler
        )
        {
            status_t res;
            io::PathPattern pat;

            if ((res = pat.set(pattern)) != STATUS_OK)
                return res;

            return scan_files(base, child, &pat, ctx, handler);
        }

        status_t scan_resources(const LSPString *path, context_t *ctx)
        {
            status_t res;
            LSPString child;

            // Compute the base path
            io::Path base, full;
            if ((res = base.set(path, "res/main")) != STATUS_OK)
                return res;

            // Lookup for specific resources
            io::Dir dir;
            if ((res = dir.open(&base)) != STATUS_OK)
                return STATUS_OK;

            while ((res = dir.read(&child)) == STATUS_OK)
            {
                if (io::Path::is_dots(&child))
                    continue;

                if (child.equals_ascii("schema"))
                    res = scan_files(&base, &child, "*.xml", ctx, schema_handler);
                else if (child.equals_ascii("ui"))
                    res = scan_files(&base, &child, "*.xml", ctx, ui_handler);
                else if (child.equals_ascii("preset"))
                    res = scan_files(&base, &child, "*.preset", ctx, preset_handler);
                else if (child.equals_ascii("i18n"))
                    res = scan_files(&base, &child, "*.json", ctx, i18n_handler);
                else
                    res = scan_files(&base, &child, "*", ctx, other_handler);

                if (res != STATUS_OK)
                    return res;
            }
            if (res == STATUS_EOF)
                res     = STATUS_OK;

            ssize_t xres = dir.close();
            return (res != STATUS_OK) ? res : xres;
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
            if ((res = dir.get_last(&pattern)) != STATUS_OK)
                return res;

            // Check if there is no pattern
            if (pattern.length() <= 0)
            {
                printf("Scanning directory '%s' for resources\n", dir.as_native());
                if ((res = scan_resources(dir.as_string(), ctx)) != STATUS_OK)
                    return res;
                return STATUS_OK;
            }

            // Update path and pattern
            if ((res = dir.remove_last()) != STATUS_OK)
                return res;
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
                if ((fa.type != io::fattr_t::FT_DIRECTORY) ||
                    (io::Path::is_dots(&item)))
                    continue;

                // Match the directory to the pattern
                if (!pat.test(&item))
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
                printf("Scanning directory '%s' for resources\n", respath->get_native());
                if ((res = scan_resources(respath, ctx)) != STATUS_OK)
                {
                    drop_paths(&matched);
                    return res;
                }
            }

            drop_paths(&matched);

            return STATUS_OK;
        }

        status_t export_files(const io::Path *dst, lltl::pphash<LSPString, LSPString> *files)
        {
            io::Path df;
            status_t res;
            wssize_t nbytes;
            lltl::parray<LSPString> flist;

            if (!files->keys(&flist))
                return STATUS_NO_MEM;

            flist.qsort();

            for (size_t i=0, n=flist.size(); i<n; ++i)
            {
                const LSPString *name = flist.uget(i);
                const LSPString *source = files->get(name);

                if ((name == NULL) || (source == NULL))
                    return STATUS_BAD_STATE;
                if ((res = df.set(dst, name)) != STATUS_OK)
                    return res;

                printf("  copying %s -> %s\n", source->get_native(), df.as_native());

                if ((res = df.mkparent(true)) != STATUS_OK)
                {
                    fprintf(stderr, "Could not create directory for file: %s\n", df.as_native());
                    return res;
                }
                if ((nbytes = io::File::copy(source, &df)) < 0)
                {
                    fprintf(stderr, "Could not create file: %s\n", df.as_native());
                    return -nbytes;
                }
            }

            return STATUS_OK;
        }

        status_t export_i18n(const io::Path *dst, lltl::pphash<LSPString, json::Node> *files)
        {
            io::Path df;
            status_t res;
            lltl::parray<LSPString> flist;
            json::serial_flags_t settings;

            if (!files->keys(&flist))
                return STATUS_NO_MEM;

            flist.qsort();

            json::init_serial_flags(&settings);
            settings.version    = json::JSON_LEGACY;
            settings.ident      = '\t';
            settings.padding    = 1;
            settings.separator  = true;
            settings.multiline  = true;

            for (size_t i=0, n=flist.size(); i<n; ++i)
            {
                const LSPString *name = flist.uget(i);
                const json::Node *node = files->get(name);

                if ((name == NULL) || (node == NULL))
                    return STATUS_BAD_STATE;
                if ((res = df.set(dst, name)) != STATUS_OK)
                    return res;

                printf("  writing i18n file %s\n", df.as_native());

                if ((res = df.mkparent(true)) != STATUS_OK)
                {
                    fprintf(stderr, "Could not create directory for file: %s\n", df.as_native());
                    return res;
                }
                if ((res = json::dom_save(&df, node, &settings, "UTF-8")) != STATUS_OK)
                {
                    fprintf(stderr, "Could not write file: %s\n", df.as_native());
                    return res;
                }
            }

            return STATUS_OK;
        }

        status_t build_repository(const char *destdir, const char *const *paths, size_t npaths)
        {
            context_t ctx;
            status_t res;

            // Scan for resource path
            for (size_t i=0; i<npaths; ++i)
            {
                if ((res = lookup_path(paths[i], &ctx)) != STATUS_OK)
                {
                    // Destroy context and return error
                    destroy_context(&ctx);
                    return res;
                }
            }

            // Export all resources
            printf("Generating resource tree");

            io::Path dst;
            res = dst.set(destdir);
            if (res == STATUS_OK)
                res = export_files(&dst, &ctx.schema);
            if (res == STATUS_OK)
                res = export_files(&dst, &ctx.ui);
            if (res == STATUS_OK)
                res = export_files(&dst, &ctx.preset);
            if (res == STATUS_OK)
                res = export_files(&dst, &ctx.other);
            if (res == STATUS_OK)
                res = export_i18n(&dst, &ctx.i18n);

            // Destroy context
            destroy_context(&ctx);

            return res;
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

