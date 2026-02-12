/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 *
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "Icon.h"

#include "GraphicsContext.h"
#include "IntRect.h"
#include <WebCore/NotImplemented.h>

#include <Bitmap.h>
#include <NodeInfo.h>
#include <View.h>
#include <wtf/text/CString.h>

namespace WebCore {

Icon::~Icon()
{
    delete (BBitmap*)m_icon;
}

RefPtr<Icon> Icon::createIconForFiles(const Vector<String>& filenames)
{
    if (filenames.isEmpty())
        return nullptr;

    // Just use the first file for the icon
    BEntry entry(filenames[0].utf8().data());
    if (entry.InitCheck() != B_OK)
        return nullptr;

    BNode node(&entry);
    if (node.InitCheck() != B_OK)
        return nullptr;

    BNodeInfo nodeInfo(&node);
    if (nodeInfo.InitCheck() != B_OK)
        return nullptr;

    // Use a reasonable icon size (32x32)
    BBitmap* iconBitmap = new BBitmap(BRect(0, 0, 31, 31), B_RGBA32);
    if (nodeInfo.GetTrackerIcon(iconBitmap, B_LARGE_ICON) != B_OK) {
        delete iconBitmap;
        return nullptr;
    }

    return adoptRef(new Icon((PlatformIcon)iconBitmap));
}

void Icon::paint(GraphicsContext& context, const FloatRect& rect)
{
    if (!m_icon)
        return;

    BView* view = context.platformContext();
    view->PushState();
    view->SetDrawingMode(B_OP_ALPHA);
    view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_COMPOSITE);
    view->DrawBitmap((BBitmap*)m_icon, BRect(rect));
    view->PopState();
}

} // namespace WebCore
