/****************************************************************************

    xflr5 v6
    Copyright (C) Andre Deperrois 
    GNU General Public License v3

*****************************************************************************/

#pragma once

#include <QDate>
#include <QComboBox>
#include <QLabel>
#include <QCheckBox>

#include <xfl3d/controls/light.h>
#include <xfl3d/testgl/gl3dtestglview.h>
#include <xfl3d/testgl/spaceobject.h>
#include <xflgeom/geom3d/vector3d.h>
#include <xflgraph/graph.h>

class IntEdit;
class DoubleEdit;
class GraphWt;

class gl3dSagittarius : public gl3dTestGLView
{
    Q_OBJECT

    public:
        gl3dSagittarius(QWidget *pParent = nullptr);
        ~gl3dSagittarius() override;

        static void loadSettings(QSettings &settings);
        static void saveSettings(QSettings &settings);

    private slots:
        void onMoveStars();
        void onRestart();
        void onStarSelection();

    private:
        void keyPressEvent(QKeyEvent *pEvent) override;

        void glRenderView() override;
        void glMake3dObjects() override;
        void makeStars();
        Planet const &selectedStar() const;

    private:

        QDate m_Started, m_Current;
        bool m_bResetStars;

        QVector<Planet> m_Star;

        QTimer m_Timer;

        IntEdit *m_pieSteps;

        DoubleEdit *m_pdeDt;

        QLabel *m_plabInfo;
        QCheckBox *m_pchMultiThread;
        QComboBox *m_pcbStar;

        GraphWt *m_pGraphDistWt;
        Graph m_GraphDist;

        GraphWt *m_pGraphVelWt;
        Graph m_GraphVel;

        QVector<QOpenGLBuffer> m_vboEllipse;
        QOpenGLBuffer m_vboEllipseFan;
        QOpenGLBuffer m_vboTrail;

        Light m_RefLight;

        static bool s_bMultithread;
        static int s_nStepsPerDay;
        static double s_dt;
};



