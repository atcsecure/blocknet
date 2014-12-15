//*****************************************************************************
//*****************************************************************************

#include "userdelegate.h"
#include "usersmodel.h"

#include <QPainter>
#include <QDebug>

//*****************************************************************************
//*****************************************************************************
UserDelegate::UserDelegate()
{
}

//*****************************************************************************
//*****************************************************************************
QSize UserDelegate::sizeHint(const QStyleOptionViewItem & option,
                                      const QModelIndex & index) const
{
    QRectF  rect = option.rect;
    rect.adjust(8, 0, -8, 0);

    // calc size for label
    QRectF rectForLabel;
    {
        QString label = index.data(UsersModel::roleLabel).toString();

        QFont f;
        f.setPixelSize(fontSizeForLabel);

        QFontMetricsF fm(f);
        rectForLabel = fm.boundingRect(rect, Qt::AlignBottom, label);
    }

    // size for addr
    QRectF rectForAddr;
    {
        QString addr = index.data(UsersModel::roleAddress).toString();

        QFont f;
        f.setPixelSize(fontSizeForAddr);

        QFontMetricsF fm(f);
        rectForAddr = fm.boundingRect(rect, Qt::AlignBottom | Qt::TextWordWrap, addr);
    }


    QSize size;// = QStyledItemDelegate::sizeHint(option, index);

    size.setHeight(rectForLabel.height() + rectForAddr.height());
    size.setWidth(rect.width());

    return size;
}

//*****************************************************************************
//*****************************************************************************
void UserDelegate::paint(QPainter * painter,
                            const QStyleOptionViewItem & option,
                            const QModelIndex & index) const
{
    // QStyledItemDelegate::paint(painter, option, index);

    if (!index.isValid())
    {
        return;
    }

    QRect rect(option.rect);

    bool isRead   = index.data(UsersModel::roleIsRead).toBool();
    QString label = index.data(UsersModel::roleLabel).toString();
    QString addr  = index.data(UsersModel::roleAddress).toString();

    // background
    if (option.state & QStyle::State_Selected)
    {
        QBrush bb(QColor(Qt::lightGray));
        painter->fillRect(rect, bb);
    }

    // draw grid
    {
        painter->setPen(Qt::SolidLine);
        painter->setPen(QColor(Qt::lightGray));

        painter->drawLine(QLine(rect.bottomLeft(), rect.bottomRight()));
    }

    // font size
    QFont f = painter->font();

    // ajust rect
    rect.adjust(8, 0, -8, 0);

    painter->setPen(QColor(Qt::black));

    // draw label
    {
        f.setPixelSize(fontSizeForLabel);
        painter->setFont(f);
        painter->drawText(rect, isRead ? label : ("* " + label));
    }

    // draw addr
    {
        painter->setPen(QColor(Qt::darkGray));

        rect.adjust(0, fontSizeForLabel, 0, 0);

        f.setPixelSize(fontSizeForAddr);
        painter->setFont(f);
        painter->drawText(rect, addr);
    }
}
