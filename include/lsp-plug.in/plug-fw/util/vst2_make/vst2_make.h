/*
 * Copyright (C) 2021 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2021 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugin-fw
 * Created on: 26 дек. 2021 г.
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

#ifndef LSP_PLUG_IN_PLUG_FW_UTIL_VST2_MAKE_VST2_MAKE_H_
#define LSP_PLUG_IN_PLUG_FW_UTIL_VST2_MAKE_VST2_MAKE_H_

#include <lsp-plug.in/plug-fw/version.h>

#include <lsp-plug.in/common/status.h>
#include <lsp-plug.in/common/types.h>
#include <lsp-plug.in/io/Path.h>
#include <lsp-plug.in/lltl/parray.h>
#include <lsp-plug.in/plug-fw/meta/types.h>
#include <lsp-plug.in/plug-fw/plug.h>

namespace lsp
{
    namespace vst2_make
    {
        /**
         * Enumerate list of VST2 plugins
         * @param list list to store metadata of VST2 plugins
         * @return status of operation
         */
        status_t enumerate_plugins(lltl::parray<meta::plugin_t> *list);

        /**
         * Generate C++ wrapping file
         * @param file file to generate
         * @param fname file name printed at the header (optional)
         * @param meta plugin metadata
         * @return status of operation
         */
        status_t gen_cpp_file(const io::Path *file, const char *fname, const meta::plugin_t *meta);

        /**
         * Generate temporary C++ file and replace the existing file with it if file checksums do not match
         * @param file file to generate
         * @param meta plugin metadata
         * @return status of operation
         */
        status_t gen_source_file(const io::Path *file, const io::Path *temp, const meta::plugin_t *meta);

        /**
         * Generate file name for the plugin wrapper
         * @param fname output file name
         * @param meta plugin metadata
         * @return status of operation
         */
        status_t make_filename(LSPString *fname, const meta::plugin_t *meta);

        /**
         * Generate makefile for compilation of all C++ files associated with plugins
         * @param base the target diretory to save make file
         * @param list list of all plugins
         * @return status of operation
         */
        status_t gen_makefile(const io::Path *base, lltl::parray<meta::plugin_t> *list);

        /**
         * Generate all necessary files to build VST2 plugin wrappers
         * @param out_dir output directory to generate files
         * @return status of operation
         */
        status_t generate_files(const char *out_dir);

        /**
         * Execute main function of the utility
         * @param argc number of arguments
         * @param argv list of arguments
         * @return status of operation
         */
        status_t main(int argc, const char **argv);
    } /* vst2_make */
} /* lsp */



#endif /* LSP_PLUG_IN_PLUG_FW_UTIL_VST2_MAKE_VST2_MAKE_H_ */
