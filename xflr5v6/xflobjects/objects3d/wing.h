/****************************************************************************

    Wing Class
    Copyright (C) 2005-2016 André Deperrois 

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

/** @file This file implements the Wing class.
 */


#pragma once

#include <QVarLengthArray>

#include <xflanalysis/analysis3d_params.h>
#include <xflgeom/geom3d/vector3d.h>
#include <xflobjects/objects2d/foil.h>
#include <xflobjects/objects2d/polar.h>
#include <xflobjects/objects3d/panel.h>
#include <xflobjects/objects3d/surface.h>
#include <xflobjects/objects3d/wingsection.h>
#include <xflobjects/objects3d/pointmass.h>

/**
 * @class Wing
 * @brief  This class defines the wing object, provides the methods for the calculation of the wing geometric properties, and
           provides methods for LLT, VLM and Panel methods.
 *
 * A wing is identified and reference by its name, which is used  by the parent Plane objects and by child Polar and WingOPpp objects.
 *
 * The wing is defined by geometric data at a finite number of span-stations : chord, chord position, foil, dihedral and washout.
 *
 * The panel between two span-stations is a WingSection object. The term "panel" is reserved for the mesh panels used to perform a VLM or Panel analysis. 
 * @todo check variable names for consistency with this rule.
 *
 * The term stations is reserved for the spanwise points used by the LLT, or by the spanwise position of the panels in a VLM or panel analysis.
 * During an LLT or a Panel analysis, the value of the aerodynamic parameters of interest are stored in the member variables of this Wing object.
 * The data is retrieved and stored in an operating point at the end of the analysis.
 * The choice to include the wing-specific methods used in the analysis in this wing class is arbitrary. It simplifies somewhat the implementation
 * of the LLTAnalysis and PanelAnalysisDlg classes, but makes this Wing class more complex.
 *
 * The data is stored in International Standard Units, i.e. meters, kg, and seconds. Angular data is stored in degrees.
*/

class PointMass;
class WPolar;
class Panel;



class Wing
{
    friend class Objects3D;
    friend class Miarex;
    friend class MainFrame;
    friend class WPolar;
    friend class PlaneOpp;
    friend class WingOpp;
    friend class Plane;
    friend class Surface;
    friend class WingDlg;
    friend class PlaneTask;
    friend class LLTAnalysisDlg;
    friend class LLTAnalysis;
    friend class PanelAnalysisDlg;
    friend class PanelAnalysis;
    friend class PlaneDlg;
    friend class ImportWingDlg;
    friend class WPolarDlg;
    friend class StabPolarDlg;
    friend class GL3dWingDlg;
    friend class ManageUFOsDlg;
    friend class InertiaDlg;
    friend class StabViewDlg;
    friend class EditPlaneDlg;
    friend class EditBodyDlg;
    friend class gl3dXflView;
    friend class XMLPlaneReader;
    friend class WingOpp;


    public:

        Wing();
        ~Wing();

        bool importDefinition(QString path_to_file, QString errorMessage);
        bool exportDefinition(QString path_to_file, QString errorMessage);

        void createSurfaces(Vector3d const &T, double XTilt, double YTilt);//generic surface, LLT, VLM or Panel

        int  VLMPanelTotal(bool bThinSurface) const;
        void VLMSetBending();

        void panelComputeOnBody(double QInf, double Alpha, double *Cp, const double *Gamma, double &XCP, double &YCP, double &ZCP,
                                double &GCm, double &VCm, double &ICm, double &GRm, double &GYm, double &VYm, double &IYm,
                                const WPolar *pWPolar, const Vector3d &CoG, const Panel *pPanel);


        void panelComputeViscous(double QInf, WPolar const*pWPolar, double &WingVDrag, bool bViscous, QString &OutString);
        void panelComputeBending(const Panel *pPanel, bool bThinSurface);

        bool isWingPanel(int nPanel, Panel const *pPanel);
        bool isWingNode(int nNode, Panel const *pPanel);

        void getFoils(Foil **pFoil0, Foil **pFoil1, double y, double &t);
        void duplicate(const Wing *pWing);
        void computeChords(int NStation=0);
        void computeChords(int NStation, double *chord, double *offset, double *twist);
        void computeGeometry();
        void computeVolumeInertia(Vector3d &CoG, double &CoGIxx, double &CoGIyy, double &CoGIzz, double &CoGIxz) const;
        void computeBodyAxisInertia();

        bool intersectWing(Vector3d O,  Vector3d U, Vector3d &I) const;

        void scaleSweep(double NewSweep);
        void scaleTwist(double NewTwist);
        void scaleSpan(double NewSpan);
        void scaleChord(double NewChord);
        void scaleArea(double newArea);
        void scaleAR(double newAR);
        void scaleTR(double newTR);

        void surfacePoint(double xRel, double ypos, xfl::enumSurfacePosition pos, Vector3d &Point, Vector3d &PtNormal) const;

        bool isWingOut() const {return m_bWingOut;}

        bool const &isFin() const {return m_bIsFin;}
        void setFin(bool bFin) {m_bIsFin = bFin;}

        bool const &isDoubleFin() const {return m_bDoubleFin;}
        void setDoubleFin(bool bDouble) {m_bDoubleFin=bDouble;}

        bool const &isSymFin()    const {return m_bSymFin;}
        void setSymFin(bool bSym) {m_bSymFin=bSym;}

        void insertSection(int iSection);
        void appendWingSection() {m_Section.append(WingSection());}
        void appendWingSection(double Chord, double Twist, double Pos, double Dihedral, double Offset, int NXPanels, int NYPanels,
                               xfl::enumPanelDistribution XPanelDist, xfl::enumPanelDistribution YPanelDist, QString const &RightFoilName, QString const&LeftFoilName);
        void removeWingSection(int const iSection);

        void clearWingSections() {m_Section.clear();}
        void clearPointMasses() {m_PointMass.clear();}
        void clearSurfaces() {m_Surface.clear();}

        //access methods
        int NWingSection()  const {return m_Section.count();}

        void setNXPanels(int iSec, int nx) {if(iSec>=0 && iSec<m_Section.size()) m_Section[iSec].m_NXPanels=nx;}
        void setNYPanels(int iSec, int ny) {if(iSec>=0 && iSec<m_Section.size()) m_Section[iSec].m_NYPanels=ny;}

        void setXPanelDist(int iSec, xfl::enumPanelDistribution dist) {if(iSec>=0 && iSec<m_Section.size()) m_Section[iSec].m_XPanelDist = dist;}
        void setYPanelDist(int iSec, xfl::enumPanelDistribution dist) {if(iSec>=0 && iSec<m_Section.size()) m_Section[iSec].m_YPanelDist = dist;}

        bool isWingFoil(Foil const*pFoil) const;
        double rootChord()     const {return m_Section.first().m_Chord;}
        double rootOffset()    const {return m_Section.first().m_Offset;}
        double tipChord()      const {return m_Section.last().m_Chord;}
        double tipTwist()      const {return m_Section.last().m_Twist;}
        double tipOffset()     const {return m_Section.last().m_Offset;}
        double tipPos()        const {return m_Section.last().m_YPosition;}
        double planformSpan()  const {return m_PlanformSpan;}
        double projectedSpan() const {return m_ProjectedSpan;}


        double Offset(int iSection)    const {return m_Section.at(iSection).m_Offset;}
        double Dihedral(int iSection)  const {return m_Section.at(iSection).m_Dihedral;}
        double Chord(int iSection)     const {return m_Section.at(iSection).m_Chord;}
        double Twist(int iSection)     const {return m_Section.at(iSection).m_Twist;}
        double YPosition(int iSection) const {return m_Section.at(iSection).m_YPosition;}

        double Length(int iSection)    const {return m_Section.at(iSection).m_Length;}
        double YProj(int iSection)     const {return m_Section.at(iSection).m_YProj;}
        double ZPosition(int iSection) const {return m_Section.at(iSection).m_ZPos;}
        int NXPanels(int iSection)     const {return m_Section.at(iSection).m_NXPanels;}
        int NYPanels(int iSection)     const {return m_Section.at(iSection).m_NYPanels;}
        xfl::enumPanelDistribution XPanelDist(int iSection) const {return m_Section.at(iSection).m_XPanelDist;}
        xfl::enumPanelDistribution YPanelDist(int iSection) const {return m_Section.at(iSection).m_YPanelDist;}
        QString const & rightFoilName(int iSection) const {return m_Section.at(iSection).m_RightFoilName;}
        QString const & leftFoilName( int iSection) const {return m_Section.at(iSection).m_LeftFoilName;}


        void setYPosition(int iSec, double ypos) {if(iSec>=0 && iSec<MAXSPANSTATIONS) m_Section[iSec].m_YPosition=ypos;}
        void setChord(int iSec, double c)        {if(iSec>=0 && iSec<m_Section.size()) m_Section[iSec].m_Chord=c;}
        void setTwist(int iSec, double t)        {if(iSec>=0 && iSec<m_Section.size()) m_Section[iSec].m_Twist=t;}
        void setDihedral(int iSec, double d)     {if(iSec>=0 && iSec<m_Section.size()) m_Section[iSec].m_Dihedral=d;}
        void setOffset(int iSec, double o)       {if(iSec>=0 && iSec<m_Section.size()) m_Section[iSec].m_Offset=o;}
        void setLength(int iSec, double l)       {if(iSec>=0 && iSec<m_Section.size()) m_Section[iSec].m_Length=l;}
        void setYProj(int iSec, double y)        {if(iSec>=0 && iSec<m_Section.size()) m_Section[iSec].m_YProj=y;}
        void setZPosition(int iSec, double z)    {if(iSec>=0 && iSec<m_Section.size()) m_Section[iSec].m_ZPos=z;}


        double yrel(double SpanPos) const;
        double getChord(double yob) const;
        double getOffset(double yob) const;
        double getDihedral(double yob) const;
        double getTwist(double y) const;
        double averageSweep() const;

        double volumeMass() const {return m_VolumeMass;}
        void setVolumeMass(double m) {m_VolumeMass=m;}

        double totalMass() const;
        double C4(double yob) const;
        double zPos(double y) const;


        void setSymetric(bool bSymetric) {m_bSymetric=bSymetric;}
        bool isSymetric() const {return m_bSymetric;}

        int nFlaps() const {return m_nFlaps;}
        void setNFlaps(int nf) {m_nFlaps=nf;}

        static double minPanelSize() {return s_MinPanelSize;}

        Vector3d CoG() const {return m_CoG;}

        QString const &name() const {return m_Name;}
        void setWingName(QString name) {m_Name=name;}

        QString const& wingDescription() const {return m_Description;}
        void setWingDescription(QString desc) {m_Description=desc;}

        void setRightFoilName(int iSec, QString foilname) {if(iSec>=0 && iSec<m_Section.size()) m_Section[iSec].m_RightFoilName=foilname;}
        void setLeftFoilName( int iSec, QString foilname) {if(iSec>=0 && iSec<m_Section.size()) m_Section[iSec].m_LeftFoilName=foilname;}

        QColor const & color() const {return m_Color;}
        void setColor(QColor const &colour) {m_Color= colour;}

        xfl::enumWingType const &wingType() const {return m_WingType;}
        void setWingType(xfl::enumWingType type) {m_WingType=type;}

        void getTextureUV(int iSurf, double *leftV, double *rightV, double &leftU, double &rightU, int nPoints) const;

        int NStations() const {return m_NStation;}

        bool serializeWingWPA(QDataStream &ar, bool bIsStoring);
        bool serializeWingXFL(QDataStream &ar, bool bIsStoring);


        // STL Output Functions
        void exportSTLBinaryTriangle(QDataStream &outStream, Vector3d N, Vector3d Pt0, Vector3d Pt1, Vector3d Pt2, float unit);
        void exportSTLBinary(QDataStream &outStream, int CHORDPANELS, int SPANPANELS, float unit);
        void exportSTLTextTriangle(QTextStream &outStream, Vector3d N, Vector3d Pt0, Vector3d Pt1, Vector3d Pt2);
        void exportSTLText(QTextStream &outStream, int CHORDPANELS, int SPANPANELS);

        // 3D Printing Functions
        void exportSTLTriangle3dPrintable(QDataStream &outStreamData, QTextStream &outStreamText, bool binaryOut,
                                          Vector3d Pt0, Vector3d Pt1, Vector3d Pt2, Vector3d N, Vector3d offset, float unit,
                                          bool reverse=false, double forceFlat = DBL_MAX);


        typedef enum {STDMESH, PRINTABLE, RIBSONLY, MOLD} printOutputStyle;
        typedef enum {SPARCUTOUT, SPARPRINTED} sparType;
        typedef enum {OUTERFACE = 0, INNERFACE = 1} faceType;

        struct sparStruct {
            Vector3d pL;            // p0 be to the left of p1.
            Vector3d pR;
            double radius;
            sparType type = SPARCUTOUT;   // BRACECUTOUT, BRACEPRINTED
            int shape = 0;   // 0 = cylindrical (currently only cylindrical braces are supported)
            int vertexCount = 25;    // setting to 3 = triangular brace, 4 = square brace, lots = a cylinder
        };
        struct rdhStruct {  // resin Drainage Hole data
            double rdhLoc;
            Vector3d start[2];  // 0 = outer, 1=inner skin
            Vector3d end[2];
            Vector3d tip[2];
            int startIc[2]={-1,-1};
            int endIc[2]= {-1,-1};
            int tipIc[2]= {-1,-1};
        };
        Vector3d foilXZIntersection(Vector3d A, Vector3d B, Vector3d C, Vector3d D);
        void generateSecondSkinFoilPoints(QVector<Vector3d> &PtPrimaryTop, QVector<Vector3d> &NormalPrimaryTop,
                                          QVector<Vector3d> &PtPrimaryBot, QVector<Vector3d> &NormalPrimaryBot,
                                          QVector<Vector3d> &PtSecondTop, QVector<Vector3d> &NormalSecondTop,
                                          QVector<Vector3d> &PtSecondBot, QVector<Vector3d> &NormalSecondBot,
                                          QVector<double> &skinThicknessTop, QVector<double>  &skinThicknessBot,
                                          int outputStyle);
        QVector<Vector3d> generateSparPoints(sparStruct spar, double y);
        void generateDrainageHoles(QVector<Vector3d> &PtLeftOuter, QVector<Vector3d> &PtRightOuter,
                                                      QVector<Vector3d> &PtLeftInner, QVector<Vector3d> &PtRightInner,
                                                      QVector<rdhStruct> &resinDrainageHoles, double resinDrainageHoleWH,
                                                      double tau);

        uint32_t stitchWingSurface(QDataStream &outStreamData, QTextStream &outStreamText, bool &binaryOut,
                                 QVector<Vector3d> &PtLeft, QVector<Vector3d> &NormalA,
                                 QVector<Vector3d> &PtRight, QVector<Vector3d> &NormalB,
                                 double tau, double tauA, double tauB, Vector3d &offset, float& unit, bool reverse);
        uint32_t stitchWingSurfaceDrained(QDataStream &outStreamData, QTextStream &outStreamText, bool &binaryOut,
                                 QVector<Vector3d> &PtLeft, QVector<Vector3d> &NormalA,
                                 QVector<Vector3d> &PtRight, QVector<Vector3d> &NormalB,
                                 QVector<rdhStruct> &resinDrainageHoles, double resinDrainageHoleWH, faceType outer,
                                 double tau, double tauA, double tauB, Vector3d &offset, float& unit, bool reverse);
        uint32_t stitchFoilFace(QDataStream &outStreamData, QTextStream &outStreamText, bool &binaryOut, bool bRightCap,
                           QVector<Vector3d> &PtTopLeft, QVector<Vector3d> &PtBotLeft,
                           QVector<Vector3d> &PtTopRight, QVector<Vector3d> &PtBotRight,
                           double tau, Vector3d &offset, float& unit, double forceFlat = DBL_MAX);
        uint32_t stitchFoilFaceSpars(QDataStream &outStreamData, QTextStream &outStreamText, bool &binaryOut, bool bRightCap,
                                 QVector<Vector3d> &PtTopLeft, QVector<Vector3d> &PtBotLeft,
                                 QVector<Vector3d> &PtTopRight, QVector<Vector3d> &PtBotRight,
                                 QVector<sparStruct> spars, double y, double tau, Vector3d &offset, float& unit, double forceFlat = DBL_MAX);
        uint32_t stitchFoilFaceComplex(QDataStream &outStreamData, QTextStream &outStreamText, bool &binaryOut, bool bRightCap,
                                 QVector<Vector3d> &PtTopLeft, QVector<Vector3d> &PtBotLeft,
                                 QVector<Vector3d> &PtTopRight, QVector<Vector3d> &PtBotRight,
                                 QVector<sparStruct> spars, double y, double tau, Vector3d &offset, float& unit);
        uint32_t stitchSkinEdge(QDataStream &outStreamData, QTextStream &outStreamText, bool &binaryOut, bool bRightCap, int outputStyle,
                                 QVector<Vector3d> &PtPrimaryTopLeft, QVector<Vector3d> &PtPrimaryBotLeft,
                                 QVector<Vector3d> &PtPrimaryTopRight, QVector<Vector3d> &PtPrimaryBotRight,
                                 QVector<Vector3d> &PtSecondTopLeft, QVector<Vector3d> &PtSecondBotLeft,
                                 QVector<Vector3d> &PtSecondTopRight, QVector<Vector3d> &PtSecondBotRight,
                                 double tau, Vector3d &offset, float& unit);

        uint32_t stitchSpar(QDataStream &outStreamData, QTextStream &outStreamText, bool &binaryOut,
                  sparStruct spars, double yLeft, double yRight, Vector3d &offset, float& unit);
        uint32_t stitchDrainageHole(QDataStream &outStreamData, QTextStream &outStreamText, bool &binaryOut,
                  QVector<rdhStruct> &resinDrainageHoles,
                  Vector3d &offset, float& unit, bool reverse);
        uint32_t exportSTL3dPrintable(QDataStream &outStreamData, QTextStream &outStreamText, bool binaryOut,
                                  int CHORDPANELS, int SPANPANELS,
                                  printOutputStyle outputStyle, float unit);


        Foil* foil(const QString &strFoilName);

        double IntegralC2(double y1, double y2, double c1, double c2) const;
        double IntegralCy(double y1, double y2, double c1, double c2) const;

        double mac() const {return m_MAChord;}

        int surfaceCount() const {return m_Surface.size();}
        Surface const *surface(int is) const {if (is>=0 && is<m_Surface.size()) return &m_Surface.at(is); else return nullptr;}
        Surface *surface(int is) {if (is>=0 && is<m_Surface.size()) return &m_Surface[is]; else return nullptr;}

        void setPanelCoun(int n) {m_nPanels=n;}
        void setFirstPanelIndex(int i) {m_FirstPanelIndex=i;}
        int firstPanelIndex() const {return m_FirstPanelIndex;}
        int nPanels() const {return m_nPanels;}

        static double getInterpolatedVariable(int nVar, Foil *pFoil0, Foil *pFoil1, double Re, double Cl, double Tau, bool &bOutRe, bool &bError);
        static double getPlrPointFromCl(Foil *pFoil, double Re, double Cl, int PlrVar, bool &bOutRe, bool &bError);

    //__________________________Variables_______________________
    private:
        QString m_Name;                           /**< the Wing's name; this name is used to identify the wing and as a reference for child Polar and WingOpp objects. */
        QString m_Description;                 /**< a text field for the description of the Wing */
        QColor m_Color;                        /**< the Wing's display color */

        xfl::enumWingType m_WingType;  /** Defines the type of wing on the plane : main, second, elevator, fin, other */

        static double s_MinPanelSize;      /**< wing minimum panel size; panels of less length are ignored */

        bool m_bWingOut;                 /**< true if the wing OpPoint is outside the flight envelope of the available Type 1 polar mesh */
        bool m_bSymetric;                 /**< true if the wing's geometry is symetric */
        bool m_bIsFin;                   /**< true if this wing describes a fin */

        bool m_bDoubleFin;               /**< true if the wing describes a double fin symetric about the y=0 plane */
        bool m_bSymFin;                  /**< true if the wing describes a double fin symetric about the z=0 plane */
    //    double m_bDoubleSymFin;          /**< true if the fin is both double and symetric */

        int m_NStation;                  /**< the number of stations for wing calculation; either the number of points of LLT, or the number of spanwise panels  */
    //    int m_nNodes;                    /**< the number of nodes of the panel mesh */
    //    int m_AVLIndex;                  /**< a random identification number needed to export to AVL */

        int m_nFlaps;                    /**< the number of T.E. flaps, numbered from left wing to right wing; for a main wing this number is even*/
        QVector<double> m_FlapMoment;      /**< the flap moments resulting from the panel of VLM analysis */

        double m_VolumeMass;             /**< the mass of the Wing's structure, excluding point masses */

        double m_GChord;                 /**< the mean geometric chord */
        double m_yMac;                   /**< the mean aerodynamic chord span position  -  @todo meaning and calculation are unclear*/
        double m_CDi;                    /**< the wing's induced drag coefficient for the current calculation */
        double m_CDv;                    /**< the wing's viscous drag coefficient for the current calculation */
        double m_WingCL;                     /**< the lift coefficient */
        double m_Maxa;                   /**< the convergence crtierion on the difference of induced angle at any span bewteen two LLT iterations*/
        double m_ICm;                    /**< the induced par of the pitching moment coefficient */
        double m_GCm;                    /**< the total pitching moment coefficient */
        double m_VCm;                    /**< the viscous part of the pitching moment coefficient */
        double m_GYm;                    /**< the total yawing moment coefficient */
        double m_IYm;                    /**< the induced part of the yawing moment coefficient */
        double m_VYm;                    /**< the viscous part of the yawing moment coefficient */
        double m_GRm;                    /**< the geometric rolling moment coefficient */

        Vector3d m_CP;                    /**< the centre of pressure's position */


        /**<  Span Coefficients  resulting from VLM or LLT calculation @todo replace with QVarLenArray<double>*/
        double m_Ai[MAXSPANSTATIONS+1];            /**< the induced angles, in degrees */
        double m_Cl[MAXSPANSTATIONS+1];            /**< the lift coefficient at stations */
        double m_ICd[MAXSPANSTATIONS+1];           /**< the induced drag coefficient at stations */
        double m_PCd[MAXSPANSTATIONS+1];           /**< the viscous drag coefficient at stations */
        double m_Re[MAXSPANSTATIONS+1];            /**< the Reynolds number at stations */
        double m_XTrTop[MAXSPANSTATIONS+1];        /**< the upper transition location at stations */
        double m_XTrBot[MAXSPANSTATIONS+1];        /**< the lower transition location at stations */
        double m_Cm[MAXSPANSTATIONS+1];            /**< the total pitching moment coefficient at stations */
        double m_CmPressure[MAXSPANSTATIONS+1];     /**< the pitching moment coefficient at stations w.r.t. the chord's quarter point */
        double m_XCPSpanRel[MAXSPANSTATIONS+1];    /**< the relative position of the strip's center of pressure at stations as a % of the local chord length*/
        double m_XCPSpanAbs[MAXSPANSTATIONS+1];    /**< the absolute position of the strip's center of pressure pos at stations */
        double m_Chord[MAXSPANSTATIONS+1];         /**< the chord at stations */
        double m_Offset[MAXSPANSTATIONS+1];        /**< the offset at LLT stations */
        double m_Twist[MAXSPANSTATIONS+1];         /**< the twist at LLT stations */
        double m_StripArea[MAXSPANSTATIONS+1];     /**< the area of each chordwise strip */
        double m_BendingMoment[MAXSPANSTATIONS+1]; /**< the bending moment at stations */
        double m_SpanPos[MAXSPANSTATIONS+1];       /**< the span positions of LLT stations */

        QVarLengthArray<double> m_xHinge;           /**< the chorwise position of flap hinges */
        QVarLengthArray<double> m_xPanel;           /**< the chorwise distribution of VLM panels */

        Vector3d m_Vd[MAXSPANSTATIONS];             /**< the downwash vector at span stations */
        Vector3d m_F[MAXSPANSTATIONS];              /**< the lift vector at span stations */

        // 3d printing variables
        static constexpr double mm = 0.001;
        static constexpr double M_PID = 3.14159265358979323846;
        static constexpr double M_2PI = M_PID * 2.0;


    public:
        QVector<WingSection> m_Section;            /**< the array of wing sections. A WingSection extends between a foil and the next. */
        QVector<PointMass> m_PointMass;            /**< the array of PointMass objects associated to this Wing object*/

        QVector<Surface> m_Surface;                /**< the array of Surface objects associated to the wing */

        double m_MAChord;                          /**< the wing's mean aerodynamic chord */
        double m_PlanformSpan;                     /**< the planform span, i.e. if the dihedral was 0 at each junction */
        double m_ProjectedSpan;                    /**< the span projected on the xy plane defined by z=0 */
        double m_PlanformArea;                     /**< the planform wing area, i.e. if the dihedral was 0 at each junction */
        double m_ProjectedArea;                    /**< the wing area projected on the xy plane defined by z=0; */
        double m_AR;                               /**< the wing's aspect ratio */
        double m_TR;                               /**< the wing's taper ratio */
        double m_CoGIxx;                           /**< the Ixx component of the inertia tensor, calculated at the CoG */
        double m_CoGIyy;                           /**< the Ixx component of the inertia tensor, calculated at the CoG */
        double m_CoGIzz;                           /**< the Ixx component of the inertia tensor, calculated at the CoG */
        double m_CoGIxz;                           /**< the Ixx component of the inertia tensor, calculated at the CoG */
        Vector3d m_CoG;                            /**< the position of the CoG */

        int m_FirstPanelIndex;                          /**< the index of this wing's first panel in the array of panels */
        int m_nPanels;                             /**< the number of mesh panels on this Wing; dependant on the polar type */

        static QVector<Foil*> *s_poaFoil;
        static QVector<Polar*> *s_poaPolar;



        // 3D Printing Variables

        // When proven working and useful these should be editable in the dialog box code to enable user modification
//        Vector3d printerSize = { 0.132, 0.074, 0.130 };       // Maximum printbounds (ie size of the printer)

    //    bool ribsOnly = false;                // generate only ribs (useful for 2d printing or lasercutting)
    //    bool ribAlignmentMarkers = false;     // add lines on the ribs to help vertical/horizontal alignment

        double skinThickness = 0.0*mm;        // TODO: adjust the skin thickness depending on the force that each panel experiences
                                            // (but I'm not sure how to access that data from this )

        double skinStrainReliefRadius = 0.5*mm;
        double skinStrainReliefPoints = 5;

        double partZSpacing = 1.1;     // the maximum z-height of the rib is multiplied by this to allow a gap between parts.
                                        // decrease/increase to shink/grow the gap between rib divisions


        QVector<double> ribLocations;       // This means use could manually set them or autospace them as desired
        double ribSpacing = 50*mm;          // if ribLocations haven't been set then this will autoset them to even spacing
        double ribThickness = 2*mm;

        //int numberOfRibs = (surf.spanLength()/(double)ribSpacing);

    //    bool resinDrainageHoles = false;    // (not yet implemented) add triangular projections/cutouts at top and bottom of wing to allow resin drainage (and/or alignment keys)
    //    double resinDrainageHoleClearance = 0.0f;  // how much gap to leave between hole and peg?
    //    bool alignmentPegs = false;    // (not yet implemented) add pyramidal projections/cutouts  at inner surface of leading/trailing edge to improve alignment
    //    bool numberOfAlignmentPegs = false;    // keys will be distributed around the wing
    //    double alignmentPegClearance = 0.0f;  // how much gap to leave between hole and peg?

        double sparCutoutClearance = 0.0f;
    //    double sparCollarHeight = 0.0f;         // how long should the tube be?
    //    double sparCollarThickness = 0.0f;      // how thick should the tube wall be?



        QVector<sparStruct> spars;      // shift to being a property of the wing


};


