/****************************************************************************

    BatchDlg Class
    Copyright (C) 2003-2019 André Deperrois

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
/**
* @file BatchDlg
* This file implements the interface to manage a batch analysis of Foils.
*/
#pragma once

#include <QDialog>
#include <QCheckBox>
#include <QFile>
#include <QPushButton>
#include <QLabel>
#include <QRadioButton>
#include <QTextEdit>
#include <QSplitter>

#include "batchabstractdlg.h"
#include <xflcore/core_enums.h>



class IntEdit;
class DoubleEdit;
class XFoilTask;
class XFoilTaskEvent;
class Foil;
class Polar;
class Graph;
class GraphWt;
class XDirect;

/**
* @class BatchDlg
* This class provides an interface to manage a batch analysis of Foils.
*/

class BatchGraphDlg : public BatchAbstractDlg
{
    Q_OBJECT
    friend class XDirect;
    friend class MainFrame;

    public:
        BatchGraphDlg(QWidget *pParent=nullptr);
        ~BatchGraphDlg() override;

        void customEvent(QEvent * pEvent) override;
        void showEvent(  QShowEvent *pEvent) override;
        void hideEvent(  QHideEvent *pEvent) override;

    private:
        void alphaLoop();
        void cleanUp() override;
        void setupLayout();
        void initDialog() override;
        void readParams() override;
        void ReLoop();
        void resetCurves();

        void analyze();
        void outputMsg(QString &msg);
        void connectSignals();

    private slots:
        void onAnalyze() override;
        void onPolarType();

        void onSkipPoint();
        void onSkipPolar();
        void onProgress();
        void onAnalysisSettings();

    private:
        QRadioButton *m_rbtype1, *m_rbtype2, *m_rbtype3, *m_rbtype4;


        IntEdit *m_pieMaxIter;

        QCheckBox *m_pchInitBLOpp;
        QCheckBox *m_pchStoreOpp;
        QPushButton *m_ppbSkipOpp, *m_ppbSkipPolar;
        GraphWt *m_pGraphWt;

        QSplitter *m_pRightSplitter;

        double m_SpMin, m_SpMax, m_SpInc;  /**< The range of specified aoa, Cl or Re values, depending on the type of Polar and on the user-specified input method.>*/

        bool m_bErrors;                /**< true if the analysis has generated errors>*/


        Graph *m_pRmsGraph;           /**< a pointer to the output graph >*/

        XFoilTask *m_pXFoilTask;       /**< A pointer to the instance of the XFoilTask associated to this batch analysis.
                                            The task is unique to the instance of this class, and re-used each time a new analysis is launched.>*/

        static QByteArray s_VSplitterSizes;
};


