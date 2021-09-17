/****************************************************************************

    ReListDlg Class
    Copyright (C) André Deperrois

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*****************************************************************************/



#pragma once

#include <QDialog>
#include <QDialogButtonBox>
#include <QModelIndex>
#include <QSettings>


class CPTableView;
class ActionDelegate;
class ActionItemModel;

class ReListDlg : public QDialog
{
    Q_OBJECT

    public:
        ReListDlg(QWidget *pParent=nullptr);
        ~ReListDlg();

        void initDialog(QVector<double> ReList, QVector<double> MachList, QVector<double> NCritList);
        QVector<double> const& ReList()    {return m_ReList;}
        QVector<double> const& MachList()  {return m_MachList;}
        QVector<double> const& NCritList() {return m_NCritList;}

    private slots:
        void onDelete();
        void onInsertBefore();
        void onInsertAfter();
        void onOK();
        void onCellChanged(QModelIndex topLeft, QModelIndex botRight);
        void onReTableClicked(QModelIndex index);

    private:
        void fillReModel();
        void setupLayout();
        void sortData();
        void sortRe();
        void showEvent(QShowEvent *pEvent) override;
        void hideEvent(QHideEvent *pEvent) override;
        void keyPressEvent(QKeyEvent *pEvent) override;
        void resizeEvent(QResizeEvent *pEvent) override;
        QSize sizeHint() const override  {return QSize(550,550);}

    private:
        CPTableView *m_pcptReTable;
        ActionItemModel *m_pReModel;
        ActionDelegate *m_pFloatDelegate;

        QVector<double> m_ReList;
        QVector<double> m_MachList;
        QVector<double> m_NCritList;
        QDialogButtonBox *m_pButtonBox;

        QAction *m_pInsertBeforeAct, *m_pInsertAfterAct, *m_pDeleteAct;

    public:
        static QByteArray s_WindowGeometry;
};
