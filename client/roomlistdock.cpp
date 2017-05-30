/**************************************************************************
 *                                                                        *
 * Copyright (C) 2015 Felix Rohrbach <kde@fxrh.de>                        *
 *                                                                        *
 * This program is free software; you can redistribute it and/or          *
 * modify it under the terms of the GNU General Public License            *
 * as published by the Free Software Foundation; either version 3         *
 * of the License, or (at your option) any later version.                 *
 *                                                                        *
 * This program is distributed in the hope that it will be useful,        *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 * GNU General Public License for more details.                           *
 *                                                                        *
 * You should have received a copy of the GNU General Public License      *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 *                                                                        *
 **************************************************************************/

#include "roomlistdock.h"

#include <QtCore/QSettings>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyledItemDelegate>

#include "models/roomlistmodel.h"
#include "quaternionroom.h"

class RoomListItemDelegate : public QStyledItemDelegate
{
    public:
        explicit RoomListItemDelegate(QObject* parent = nullptr)
            : QStyledItemDelegate(parent)
            , highlightColor(QSettings()
                             .value("UI/highlight_color", QColor("orange"))
                             .value<QColor>())
        { }

        void paint(QPainter *painter, const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    private:
        QColor highlightColor;
};

void RoomListItemDelegate::paint(QPainter* painter,
         const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem o { option };

    if (index.data(RoomListModel::HasUnreadRole).toBool())
        o.font.setBold(true);

    if (index.data(RoomListModel::HighlightCountRole).toInt() > 0)
    {
        // Highlighting the text may not work out on monochrome colour schemes,
        // hence duplicating with italic font.
        o.palette.setColor(QPalette::Text, highlightColor);
        o.font.setItalic(true);
    }

    QStyledItemDelegate::paint(painter, o, index);
}

RoomListDock::RoomListDock(QWidget* parent)
    : QDockWidget("Rooms", parent)
{
    setObjectName("RoomsDock");
    model = new RoomListModel(this);
    view = new QListView();
    view->setModel(model);
    view->setItemDelegate(new RoomListItemDelegate(this));
    connect( view, &QListView::activated, this, &RoomListDock::rowSelected );
    connect( view, &QListView::clicked, this, &RoomListDock::rowSelected);
    setWidget(view);

    contextMenu = new QMenu(this);
    joinAction = new QAction(tr("Join Room"), this);
    connect(joinAction, &QAction::triggered, this, &RoomListDock::menuJoinSelected);
    contextMenu->addAction(joinAction);
    leaveAction = new QAction(tr("Leave Room"), this);
    connect(leaveAction, &QAction::triggered, this, &RoomListDock::menuLeaveSelected);
    contextMenu->addAction(leaveAction);
    markAsReadAction = new QAction(tr("Mark room as read"), this);
    connect(markAsReadAction, &QAction::triggered, this, &RoomListDock::menuMarkReadSelected);
    contextMenu->addAction(markAsReadAction);

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &RoomListDock::showContextMenu);
}

void RoomListDock::addConnection(QMatrixClient::Connection* connection)
{
    model->addConnection(connection);
}

void RoomListDock::rowSelected(const QModelIndex& index)
{
    if (index.isValid())
        emit roomSelected( model->roomAt(index.row()) );
}

void RoomListDock::showContextMenu(const QPoint& pos)
{
    auto room = getCurrentRoom();
    if(!room)
        return;

    using QMatrixClient::JoinState;
    auto state = room->joinState();
    joinAction->setEnabled(state == JoinState::Leave || state == JoinState::Invite);
    if (state == JoinState::Invite || state == JoinState::Join)
    {
        leaveAction->setEnabled(true);
        markAsReadAction->setEnabled(room->hasUnreadMessages());
    }
    else
    {
        leaveAction->setEnabled(false);
        markAsReadAction->setEnabled(false);
    }

    contextMenu->popup(mapToGlobal(pos));
}

QuaternionRoom* RoomListDock::getCurrentRoom() const
{
    QModelIndex index = view->currentIndex();
    return !index.isValid() ? nullptr : model->roomAt(index.row());
}

void RoomListDock::menuJoinSelected()
{
    // The user has been invited to the room
    if (auto room = getCurrentRoom())
    {
        Q_ASSERT(room->connection());
        room->connection()->joinRoom(room->id());
    }
}

void RoomListDock::menuLeaveSelected()
{
    if (auto room = getCurrentRoom())
        room->leaveRoom();
}

void RoomListDock::menuMarkReadSelected()
{
    if (auto room = getCurrentRoom())
        room->markAllMessagesAsRead();
}
