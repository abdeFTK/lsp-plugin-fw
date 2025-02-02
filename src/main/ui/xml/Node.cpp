/*
 * Copyright (C) 2021 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2021 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugin-fw
 * Created on: 10 апр. 2021 г.
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

#include <private/ui/xml/Node.h>
#include <private/ui/xml/NodeFactory.h>

namespace lsp
{
    namespace ui
    {
        namespace xml
        {
            Node::Node(UIContext *ctx, Node *parent)
            {
                pContext        = ctx;
                pParent         = parent;
            }

            Node::~Node()
            {
            }

            status_t Node::lookup(Node **child, const LSPString *name)
            {
                *child  = NULL;
                status_t res;
                if (!name->starts_with_ascii("ui:"))
                    return STATUS_OK;

                // Try to instantiate proper node handler
                for (NodeFactory *f = NodeFactory::root(); f != NULL; f   = f->next())
                {
                    if ((res = f->create(child, pContext, this, name)) == STATUS_OK)
                        return res;
                    if (res != STATUS_NOT_FOUND)
                        return res;
                }

                lsp_error("Unknown meta-tag: <%s>", name->get_native());
                return STATUS_BAD_FORMAT;
            }

            status_t Node::enter(const LSPString * const *atts)
            {
                return STATUS_OK;
            }

            status_t Node::start_element(const LSPString *name, const LSPString * const *atts)
            {
                return STATUS_OK;
            }

            status_t Node::end_element(const LSPString *name)
            {
                return STATUS_OK;
            }

            status_t Node::completed(Node *child)
            {
                return STATUS_OK;
            }

            status_t Node::leave()
            {
                if (pParent == NULL)
                    return STATUS_OK;
                return pParent->completed(this);
            }

        }
    }
}


