/****************************************************************************

    BatchDlg Class
       Copyright (C) 2003-2017 André Deperrois

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

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QTimer>
#include <QApplication>
#include <QDir>
#include <QDebug>

#include "batchgraphdlg.h"
#include <xdirect/analysis/xfoiltask.h>
#include <xflobjects/objects2d/objects2d.h>
#include <xdirect/xdirect.h>
#include <xflcore/xflevents.h>
#include <xflgraph/containers/graphwt.h>
#include <xflgraph/curve.h>
#include <xflgraph/graph.h>
#include <xflobjects/objects2d/foil.h>
#include <xflobjects/objects2d/polar.h>
#include <xflwidgets/customwts/doubleedit.h>
#include <xflwidgets/customwts/intedit.h>


QByteArray BatchGraphDlg::s_VSplitterSizes;

/**
 * The public constructor.
 */
BatchGraphDlg::BatchGraphDlg(QWidget *pParent) : BatchAbstractDlg(pParent)
{
    QString str = tr("Batch foil analysis");
    setWindowTitle(str);

    m_pXFoilTask = new XFoilTask;
    m_pXFoilTask->m_pParent = this;

    m_SpMin = 0.0;
    m_SpMax = 1.0;
    m_SpInc = 0.5;

    m_bErrors         = false;

    XFoil::setCancel(false);

    setupLayout();
    connectSignals();

    m_pRmsGraph = new Graph;
    m_pGraphWt->setGraph(m_pRmsGraph);
    //    m_pRmsGraph->CopySettings(&Settings::s_RefGraph, false);

    m_pRmsGraph->setXTitle(tr("Iter"));
    m_pRmsGraph->setYTitle("");//Change from bl newton system solution

    m_pRmsGraph->setAuto(true);

    m_pRmsGraph->setXMajGrid(true, QColor(120,120,120),2,1);
    m_pRmsGraph->setYMajGrid(true, QColor(120,120,120),2,1);

    m_pRmsGraph->setXMin(0.0);
    m_pRmsGraph->setXMax(50);
    m_pRmsGraph->setYMin(-1.0);
    m_pRmsGraph->setYMax( 1.0);
    m_pRmsGraph->setScaleType(1);
    m_pRmsGraph->setMargin(40);
}


void BatchGraphDlg::connectSignals()
{
    connect(m_ppbSkipOpp,   SIGNAL(clicked()),         SLOT(onSkipPoint()));
    connect(m_ppbSkipPolar, SIGNAL(clicked()),         SLOT(onSkipPolar()));
    connect(m_rbtype1,      SIGNAL(clicked()),         SLOT(onPolarType()));
    connect(m_rbtype2,      SIGNAL(clicked()),         SLOT(onPolarType()));
    connect(m_rbtype3,      SIGNAL(clicked()),         SLOT(onPolarType()));
    connect(m_rbtype4,      SIGNAL(clicked()),         SLOT(onPolarType()));
    connect(m_pchInitBLOpp, SIGNAL(toggled(bool)),     SLOT(onAnalysisSettings()));
    connect(m_pchInitBL,    SIGNAL(toggled(bool)),     SLOT(onAnalysisSettings()));
    connect(m_pieMaxIter,   SIGNAL(editingFinished()), SLOT(onAnalysisSettings()));

    connectBaseSignals();
}


BatchGraphDlg::~BatchGraphDlg()
{
    if(m_pXFoilTask) delete m_pXFoilTask;
    m_pXFoilTask =  nullptr;
    if(m_pRmsGraph) delete m_pRmsGraph;
    m_pRmsGraph = nullptr;
}

/**
 * Sets up the GUI
 */
void BatchGraphDlg::setupLayout()
{
    QVBoxLayout *pLeftSideLayout = new QVBoxLayout;
    {
        QGroupBox *pAnalysisTypeGroupBox = new QGroupBox(tr("Analysis Type"));
        {
            QHBoxLayout *pAnalysisTypeLayout = new QHBoxLayout;
            {
                m_rbtype1 = new    QRadioButton(tr("Type 1"));
                m_rbtype2 = new QRadioButton(tr("Type 2"));
                m_rbtype3 = new QRadioButton(tr("Type 3"));
                m_rbtype4 = new QRadioButton(tr("Type 4"));
                pAnalysisTypeLayout->addWidget(m_rbtype1);
                pAnalysisTypeLayout->addWidget(m_rbtype2);
                pAnalysisTypeLayout->addWidget(m_rbtype3);
                pAnalysisTypeLayout->addWidget(m_rbtype4);
            }
            pAnalysisTypeGroupBox->setLayout(pAnalysisTypeLayout);
        }

        QGroupBox *pIterationsBox = new QGroupBox(tr("Iterations control"));
        {
            QHBoxLayout *pConvergenceLayout = new QHBoxLayout;
            {
                QLabel *pMaxIterLab     = new QLabel(tr("Max. iterations"));
                m_pieMaxIter   = new IntEdit(XFoilTask::s_IterLim);
                m_ppbSkipOpp   = new QPushButton(tr("Skip Opp"));
                m_ppbSkipPolar = new QPushButton(tr("Skip Polar"));
                pConvergenceLayout->addWidget(pMaxIterLab);
                pConvergenceLayout->addWidget(m_pieMaxIter);
                pConvergenceLayout->addStretch(1);
                pConvergenceLayout->addWidget(m_ppbSkipOpp);
                pConvergenceLayout->addWidget(m_ppbSkipPolar);
            }
            pIterationsBox->setLayout(pConvergenceLayout);
        }
        pLeftSideLayout->addWidget(m_pFoilBox);
        pLeftSideLayout->addWidget(pAnalysisTypeGroupBox);
        pLeftSideLayout->addWidget(m_pBatchVarsGroupBox);
        pLeftSideLayout->addWidget(m_pTransVarsGroupBox);
        pLeftSideLayout->addWidget(m_pRangeVarsGroupBox);
        pLeftSideLayout->addWidget(pIterationsBox);
        pLeftSideLayout->addStretch(1);
        pLeftSideLayout->addSpacing(20);
        pLeftSideLayout->addWidget(m_pButtonBox);
    }

    QVBoxLayout *pRightSideLayout = new QVBoxLayout;
    {
        QVBoxLayout *pOptionsLayout = new QVBoxLayout;
        {
            m_pchInitBLOpp    = new QCheckBox(tr("Initialize the boundary layer after unconverged points"));
            QHBoxLayout *pSubOptionsLayout = new QHBoxLayout;
            {
                m_pchStoreOpp     = new QCheckBox(tr("Store OpPoints"));
                pSubOptionsLayout->addWidget(m_pchStoreOpp);
                pSubOptionsLayout->addStretch();

            }
            pOptionsLayout->addWidget(m_pchInitBLOpp);
            pOptionsLayout->addWidget(m_pchInitBL);
            pOptionsLayout->addLayout(pSubOptionsLayout);
        }

        m_pRightSplitter = new QSplitter(Qt::Vertical);
        {
            m_pRightSplitter->setChildrenCollapsible(false);

            m_pteTextOutput->setFont(DisplayOptions::tableFont());
            QFontMetrics fm(DisplayOptions::tableFont());
            m_pteTextOutput->setMinimumWidth(57*fm.averageCharWidth());

            m_pGraphWt = new GraphWt;

            m_pRightSplitter->addWidget(m_pteTextOutput);
            m_pRightSplitter->addWidget(m_pGraphWt);
        }

        pRightSideLayout->addLayout(pOptionsLayout);
        pRightSideLayout->addWidget(m_pRightSplitter);
    }

    QHBoxLayout *pMainLayout= new QHBoxLayout;
    {
        pMainLayout->setSpacing(10);
        pMainLayout->addLayout(pLeftSideLayout);
        pMainLayout->addLayout(pRightSideLayout);
        pMainLayout->setStretchFactor(pRightSideLayout,1);
    }
    setLayout(pMainLayout);
}


void BatchGraphDlg::showEvent(QShowEvent *pEvent)
{
    if(s_VSplitterSizes.length()>0) m_pRightSplitter->restoreState(s_VSplitterSizes);
    BatchAbstractDlg::showEvent(pEvent);
}


void BatchGraphDlg::hideEvent(QHideEvent *pEvent)
{
    s_VSplitterSizes  = m_pRightSplitter->saveState();
    BatchAbstractDlg::hideEvent(pEvent);
}


/**
 * Used in the case of Type 4 analysis, to loop over the specified aoa range.
 * For each aoa, initializes the XFoilTask with the specified Re range, and launches the task.
 */
void BatchGraphDlg::alphaLoop()
{
    QString str;

    int nAlpha = int(qAbs((m_SpMax-m_SpMin)*1.000/m_SpInc));//*1.0001 to make sure upper limit is included

    for (int iAlpha=0; iAlpha<=nAlpha; iAlpha++)
    {
        if(m_bCancel) break;

        double alphadeg = m_SpMin + iAlpha*m_SpInc;
        str = QString("Alpha = %1\n").arg(alphadeg,0,'f',2);
        outputMsg(str);

        Polar *pCurPolar = Objects2d::createPolar(m_pFoil, xfl::FIXEDAOAPOLAR, alphadeg, s_Mach, s_ACrit, s_XTop, s_XBot);

        if(!pCurPolar) return;

        m_pXFoilTask->setReRange(s_ReMin, s_ReMax, s_ReInc);
        m_pXFoilTask->initializeXFoilTask(m_pFoil, pCurPolar, XDirect::s_bViscous, s_bInitBL, s_bFromZero);

        m_pXFoilTask->run();

        m_bErrors = m_bErrors || m_pXFoilTask->m_bErrors;

        if(s_pXDirect->m_bPolarView)
        {
            s_pXDirect->createPolarCurves();
            s_pXDirect->updateView();
        }

        if(m_bCancel)
        {
            str = tr("Analysis interrupted")+"\n";
            outputMsg(str);
            break;
        }

        qApp->processEvents();
    }//end Re loop
}


/**
 * Cleans up the GUI and paramters at the end of the Analysis.
 */
void BatchGraphDlg::cleanUp()
{
    resetCurves();
    m_ppbSkipOpp->setEnabled(false);
    m_ppbSkipPolar->setEnabled(false);

    BatchAbstractDlg::cleanUp();
}


/**
 * Initializes the GUI with the stored values.
 */
void BatchGraphDlg::initDialog()
{
    if(!m_pFoil) return;

    m_prbFoil1->setChecked(s_bCurrentFoil);
    m_prbFoil2->setChecked(!s_bCurrentFoil);
    m_ppbFoilList->setEnabled(!s_bCurrentFoil);

    if(s_bAlpha)
    {
        m_SpMin     = s_AlphaMin;
        m_SpMax     = s_AlphaMax;
        m_SpInc     = s_AlphaInc;
    }
    else
    {
        m_SpMin     = s_ClMin;
        m_SpMax     = s_ClMax;
        m_SpInc     = s_ClInc;
    }

    if(s_ReMin<=0.0) s_ReMin = qAbs(s_ReInc);

    if(s_PolarType!=xfl::FIXEDAOAPOLAR)
    {
        m_pdeReMin->setDigits(0);
        m_pdeReMax->setDigits(0);
        m_pdeReDelta->setDigits(0);

        m_pdeSpecMin->setDigits(3);
        m_pdeSpecMax->setDigits(3);
        m_pdeSpecDelta->setDigits(3);

        m_pdeReMin->setValue(s_ReMin);
        m_pdeReMax->setValue(s_ReMax);
        m_pdeReDelta->setValue(s_ReInc);
        m_pdeSpecMin->setValue(m_SpMin);
        m_pdeSpecMax->setValue(m_SpMax);
        m_pdeSpecDelta->setValue(m_SpInc);
        m_prbRange2->setEnabled(true);
    }
    else
    {
        m_pdeReMin->setDigits(1);
        m_pdeReMax->setDigits(1);
        m_pdeReDelta->setDigits(1);

        m_pdeSpecMin->setDigits(0);
        m_pdeSpecMax->setDigits(0);
        m_pdeSpecDelta->setDigits(0);
        m_pdeReMin->setValue(m_SpMin);
        m_pdeReMax->setValue(m_SpMax);
        m_pdeReDelta->setValue(m_SpInc);
        m_pdeSpecMin->setValue(s_ReMin);
        m_pdeSpecMax->setValue(s_ReMax);
        m_pdeSpecDelta->setValue(s_ReInc);
        m_prbRange2->setEnabled(false);
    }


    m_pdeMach->setValue(  s_Mach);
    m_pdeACrit->setValue( s_ACrit);
    m_pdeXTopTr->setValue(s_XTop);
    m_pdeXBotTr->setValue(s_XBot);

    if(s_bAlpha) m_prbAlpha->setChecked(true);
    else         m_prbCl->setChecked(true);
    onAcl();

    if     (s_PolarType==xfl::FIXEDSPEEDPOLAR)  m_rbtype1->setChecked(true);
    else if(s_PolarType==xfl::FIXEDLIFTPOLAR)   m_rbtype2->setChecked(true);
    else if(s_PolarType==xfl::RUBBERCHORDPOLAR) m_rbtype3->setChecked(true);
    else if(s_PolarType==xfl::FIXEDAOAPOLAR)    m_rbtype4->setChecked(true);
    onPolarType();


    if(!s_bFromList)  m_prbRange1->setChecked(!s_bFromList);
    else              m_prbRange2->setChecked(s_bFromList);
    onRange();

    if(s_bFromZero)  m_pchFromZero->setChecked(true);
    else             m_pchFromZero->setChecked(false);

    m_pchInitBL->setChecked(s_bInitBL);
    m_pchInitBLOpp->setChecked(XFoilTask::s_bAutoInitBL);
    m_pchStoreOpp->setChecked(OpPoint::bStoreOpp());

    m_ppbSkipOpp->setEnabled(false);
    m_ppbSkipPolar->setEnabled(false);

    resetCurves();

    outputFoilList();
}


/**
 * The user has changed the type of Polar. Updates and initializes the GUI accordingly.
 */
void BatchGraphDlg::onPolarType()
{
    if(m_rbtype1->isChecked())
    {
        m_plabReType->setText(tr("Reynolds ="));
        m_plabMaType->setText(tr("Mach ="));
        m_ppbEditList->setEnabled(true);
        s_PolarType = xfl::FIXEDSPEEDPOLAR;
    }
    else if(m_rbtype2->isChecked())
    {
        m_plabReType->setText(tr("Re.sqrt(Cl) ="));
        m_plabMaType->setText(tr("Ma.sqrt(Cl) ="));
        m_ppbEditList->setEnabled(true);
        s_PolarType = xfl::FIXEDLIFTPOLAR;
    }
    else if(m_rbtype3->isChecked())
    {
        m_plabReType->setText(tr("Re.Cl ="));
        m_plabMaType->setText(tr("Mach ="));
        m_ppbEditList->setEnabled(true);
        s_PolarType = xfl::RUBBERCHORDPOLAR;
    }
    else if(m_rbtype4->isChecked())
    {
        m_plabReType->setText(tr("Alpha ="));
        m_plabMaType->setText(tr("Mach ="));
        m_ppbEditList->setEnabled(false);
        m_prbAlpha->setChecked(true);
        s_PolarType = xfl::FIXEDAOAPOLAR;
    }

    if(s_PolarType!=xfl::FIXEDAOAPOLAR)
    {
        m_pdeReMin->setDigits(0);
        m_pdeReMax->setDigits(0);
        m_pdeReDelta->setDigits(0);
        m_pdeSpecMin->setDigits(3);
        m_pdeSpecMax->setDigits(3);
        m_pdeSpecDelta->setDigits(3);
        m_prbAlpha->setEnabled(true);
        m_prbCl->setEnabled(true);
        m_prbRange2->setEnabled(true);
        onAcl();

        m_pdeReMin->setValue(s_ReMin);
        m_pdeReMax->setValue(s_ReMax);
        m_pdeReDelta->setValue(s_ReInc);

        m_pdeSpecMin->setValue(m_SpMin);
        m_pdeSpecMax->setValue(m_SpMax);
        m_pdeSpecDelta->setValue(m_SpInc);
    }
    else
    {
        m_pdeReMin->setDigits(2);
        m_pdeReMax->setDigits(2);
        m_pdeReDelta->setDigits(2);
        m_pdeSpecMin->setDigits(0);
        m_pdeSpecMax->setDigits(0);
        m_pdeSpecDelta->setDigits(0);
        m_plabSpecVar->setText(tr("Reynolds ="));
        m_prbAlpha->setEnabled(false);
        m_prbCl->setEnabled(false);

        m_prbRange1->setChecked(true);
        m_prbRange2->setEnabled(false);

        m_pdeReMin->setValue(m_SpMin);
        m_pdeReMax->setValue(m_SpMax);
        m_pdeReDelta->setValue(m_SpInc);

        m_pdeSpecMin->setValue(s_ReMin);
        m_pdeSpecMax->setValue(s_ReMax);
        m_pdeSpecDelta->setValue(s_ReInc);
    }
}


/**
 * The user has requested the launch or the cancellation of the analysis.
 * If the analysis is running, cancels the XFoilTask and returns.
 * If not, initializes the XFoilTask and launches it.
 */
void BatchGraphDlg::onAnalyze()
{
    if(m_bIsRunning)
    {
        qDebug()<<"cancelling";
        XFoil::setCancel(true);
        XFoilTask::s_bCancel = true;
        m_bCancel = true;
        return;
    }
    qDebug()<<"onanalyyyze";


    m_bCancel    = false;
    m_bIsRunning = true;
    m_bErrors    = false;

    //read data specification
    readParams();

    //set output file
    QString FileName = QDir::tempPath() + "/XFLR5.log";
    m_pXFile = new QFile(FileName);
    if (!m_pXFile->open(QIODevice::WriteOnly | QIODevice::Text)) m_pXFile = nullptr;
    setFileHeader();

    //initialize XFoil task
    m_pXFoilTask->m_OutStream.setDevice(m_pXFile);

    if(s_bAlpha) m_pXFoilTask->setSequence(true,  s_AlphaMin, s_AlphaMax, s_AlphaInc);
    else         m_pXFoilTask->setSequence(false, s_ClMin, s_ClMax, s_ClInc);

    m_pXFoilTask->setReRange(s_ReMin, s_ReMax, s_ReInc);

    //prepare button state for analysis
    m_ppbAnalyze->setText(tr("Cancel"));
    m_ppbSkipOpp->setEnabled(true);
    m_ppbSkipPolar->setEnabled(true);
    m_pteTextOutput->clear();
    m_pButtonBox->button(QDialogButtonBox::Close)->setEnabled(false);
    m_ppbAnalyze->setFocus();

    analyze();
}


/**
 * The user has requested to interrupt the analysis of the current aoa.
 */
void BatchGraphDlg::onSkipPoint()
{
    XFoilTask::s_bSkipOpp = true;
}


/**
 * The user has requested to skip the current analysis of the current polar.
 */
void BatchGraphDlg::onSkipPolar()
{
    XFoilTask::s_bSkipOpp   = true;
    XFoilTask::s_bSkipPolar = true;
}


/**
 * Reads the data from the GUI
 */
void BatchGraphDlg::readParams()
{
    BatchAbstractDlg::readParams();

    if(s_PolarType==xfl::FIXEDAOAPOLAR)
    {
        m_SpInc = m_pdeReDelta->value();
        m_SpMax = m_pdeReMax->value();
        m_SpMin = m_pdeReMin->value();

        s_ReInc = m_pdeSpecDelta->value();
        s_ReMax = m_pdeSpecMax->value();
        s_ReMin = m_pdeSpecMin->value();
    }

    if(s_ReMin<=0.0) s_ReMin = fabs(s_ReInc);
    if(s_ReMax<=0.0) s_ReMax = fabs(s_ReMax);
    m_SpInc = qAbs(m_SpInc);

    OpPoint::setStoreOpp(m_pchStoreOpp->isChecked());
    XFoilTask::s_bAutoInitBL = m_pchInitBLOpp->isChecked();
}


/**
 * For Type 1, 2 and 3 Polar objects
 * Loops through all the specified Relist, and for each element of the list:
 *  - creates a Polar object
 *  - initializes the XFoilTask object
 *  - launches the XFoilTask whcih will loop over the specified aoa or Cl range
 */
void BatchGraphDlg::ReLoop()
{
    QString str;

    int iRe, nRe;
    double Reynolds =0, Mach = 0, NCrit = 9.0;

    if(!s_bFromList) nRe = int(qAbs((s_ReMax-s_ReMin)/s_ReInc));
    else             nRe = XDirect::s_ReList.count()-1;

    for (iRe=0; iRe<=nRe; iRe++)
    {
        if(!s_bFromList)
        {
            Reynolds = s_ReMin + iRe *s_ReInc;
            Mach  = s_Mach;
            NCrit  = s_ACrit;
        }
        else
        {
            Reynolds = XDirect::s_ReList[iRe];
            Mach     = XDirect::s_MachList[iRe];
            NCrit    = XDirect::s_NCritList[iRe];
        }
        str = QString("Re=%1   Ma=%2   Nc=%3\n").arg(Reynolds,8,'f',0).arg(Mach,5,'f',3).arg(NCrit,5,'f',2);
        outputMsg(str);

        Polar *pCurPolar = Objects2d::createPolar(m_pFoil, xfl::FIXEDSPEEDPOLAR, Reynolds, s_Mach, s_ACrit, s_XTop, s_XBot);
        if(!pCurPolar) return;

        m_pXFoilTask->initializeXFoilTask(m_pFoil, pCurPolar, XDirect::s_bViscous, s_bInitBL, s_bFromZero);
        m_pXFoilTask->run();

        m_bErrors = m_bErrors || m_pXFoilTask->m_bErrors;
        str = "\n";
        outputMsg(str);

        if(s_pXDirect->m_bPolarView)
        {
            s_pXDirect->createPolarCurves();
            s_pXDirect->updateView();
        }

        if(m_bCancel)
        {
            str = tr("Analysis interrupted")+"\n";
            outputMsg(str);
            break;
        }
    }//end Re loop
}



/**
 * Clears the content of the Graph's Curve, and resets the scales.
 */
void BatchGraphDlg::resetCurves()
{
    m_pRmsGraph->deleteCurves();
    Curve *pCurve0 = m_pRmsGraph->addCurve();
    Curve *pCurve1 = m_pRmsGraph->addCurve();
    pCurve0->setName("rms");
    pCurve1->setName("max");
    pCurve1->setStipple(0);
    m_pRmsGraph->setAutoX(false);
    m_pRmsGraph->setXMin(0.0);
    m_pRmsGraph->setXMax(double(XFoilTask::s_IterLim));
    m_pRmsGraph->setXUnit(int(double(XFoilTask::s_IterLim)/5.0));
    m_pRmsGraph->setYMin(-1.0);
    m_pRmsGraph->setYMax( 1.0);
    m_pRmsGraph->setX0(0.0);
    m_pRmsGraph->setY0(0.0);

//    m_pXFoilTask->setGraphPointers(&pCurve0->m_x, &pCurve0->m_y, &pCurve1->m_x, &pCurve1->m_y);
}



/**
 * Performs the analysis.
 * Creates and launches a QTimer to update the output at ergular intervals.
 * Launches the ReLoop() or the AlphaLoop() depending on the Polar type.
 * At the end of the analysis, performs a CleanUp() sequence.
 */
void BatchGraphDlg::analyze()
{
    QString strong;

    m_ppbAnalyze->setText(tr("Cancel"));

    //create a timer to update the output at regular intervals
    QTimer *pTimer = new QTimer;
    connect(pTimer, SIGNAL(timeout()), this, SLOT(onProgress()));
    pTimer->setInterval(XDirect::s_TimeUpdateInterval);
    pTimer->start();

    if(s_bCurrentFoil)
    {
        if(s_PolarType!=xfl::FIXEDAOAPOLAR) ReLoop();
        else                                alphaLoop();
    }
    else
    {
        for(int i=0; i<m_FoilList.count();i++)
        {
            m_pFoil = Objects2d::foil(m_FoilList.at(i));

            strong = tr("Analyzing ")+m_pFoil->name()+("\n");
            outputMsg(strong);
            if(s_PolarType!=xfl::FIXEDAOAPOLAR) ReLoop();
            else                                  alphaLoop();
            strong = "\n\n";
            outputMsg(strong);
            if(m_bCancel) break;
        }
    }

    pTimer->stop();
    delete pTimer;

    onProgress();

    if(!m_bCancel)
    {
        if(m_bErrors)
        {
            strong = tr(" ...some points are unconverged") + "\n";
            outputMsg(strong);
        }
        strong = tr("Analysis completed")+"\n";
        outputMsg(strong);
    }

    m_pXFoilTask->m_OutStream.flush();

    onProgress();

    cleanUp();
}


/**
 * The slot called by the QTimer for the display of the progress of the analysis.
 * Gets the message from the XFoilTask and copies it to the output text window.
 * Updates the graph output.
 */
void BatchGraphDlg::onProgress()
{
    m_pGraphWt->update();

    if(m_pXFoilTask->m_OutMessage.length())
    {
        m_pteTextOutput->insertPlainText(m_pXFoilTask->m_OutMessage);
        m_pteTextOutput->ensureCursorVisible();
    }
    m_pXFoilTask->m_OutMessage.clear();
}

/**
 * Sends the specified message to the output.
 * The message is output through the XFoil task, to ensure it is in correct output order.
 */
void BatchGraphDlg::outputMsg(QString &msg)
{
    if(!msg.length()) return;
    m_pXFoilTask->traceLog(msg);
}


/**
 * The user has modified the settings for the analysis.
 * Read the values and update the variables.
 */
void BatchGraphDlg::onAnalysisSettings()
{
    s_bInitBL = m_pchInitBL->isChecked();
    XFoilTask::s_bAutoInitBL = m_pchInitBLOpp->isChecked();
    XFoilTask::s_IterLim = m_pieMaxIter->value();
    m_pRmsGraph->setXMax(XFoilTask::s_IterLim);
    m_pRmsGraph->setAutoXUnit();
    update();
}


void BatchGraphDlg::customEvent(QEvent * pEvent)
{
    if(pEvent->type() == XFOIL_END_TASK_EVENT)
    {
    }
    else if(pEvent->type() == XFOIL_ITER_EVENT)
    {
        XFoilIterEvent *pIterEvent = dynamic_cast<XFoilIterEvent*>(pEvent);
        Curve *pCurve0 = m_pRmsGraph->curve(0);
        Curve *pCurve1 = m_pRmsGraph->curve(1);
        if(pCurve0) pCurve0->appendPoint(pIterEvent->m_x0, pIterEvent->m_y0);
        if(pCurve1) pCurve1->appendPoint(pIterEvent->m_x1, pIterEvent->m_y1);
        m_pGraphWt->update();
    }
    else if(pEvent->type() == XFOIL_END_OPP_EVENT)
    {
        XFoilOppEvent *pOppEvent = dynamic_cast<XFoilOppEvent*>(pEvent);
        if(OpPoint::bStoreOpp())  Objects2d::insertOpPoint(pOppEvent->theOpPoint());
        else                      delete pOppEvent->theOpPoint();

        m_pRmsGraph->resetYLimits();
        Curve *pCurve0 = m_pRmsGraph->curve(0);
        Curve *pCurve1 = m_pRmsGraph->curve(1);
        if(pCurve0) pCurve0->clear();
        if(pCurve1) pCurve1->clear();
        update();
    }
}






