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

#include <QDebug>
#include <QFile>
#include <QRandomGenerator>

#include <xflcore/xflcore.h>
#include <xflobjects/objects3d/wing.h>
#include <xflobjects/objects3d/wpolar.h>
#include <xflobjects/objects3d/surface.h>
#include <xflobjects/objects3d/panel.h>
#include <xflobjects/objects3d/pointmass.h>
#include <xflobjects/objects_global.h>
#include <xflobjects/objects2d/polar.h>

bool sortSecondSkinFoilPointsTop(Vector3d p0, Vector3d p1) { return p0.x < p1.x; }
bool sortSecondSkinFoilPointsBot(Vector3d p0, Vector3d p1) { return p0.x < p1.x; }


double Wing::s_MinPanelSize = 0.0001;
QVector<Foil *> *Wing::s_poaFoil  = nullptr;
QVector<Polar*> *Wing::s_poaPolar = nullptr;

/**
 * The public constructor.
 */
Wing::Wing()
{
    memset(m_Ai,    0, sizeof(m_Ai));
    memset(m_Twist, 0, sizeof(m_Twist));
    memset(m_Cl,    0, sizeof(m_Cl));
    memset(m_PCd,   0, sizeof(m_PCd));
    memset(m_ICd,   0, sizeof(m_ICd));
    memset(m_Cm,    0, sizeof(m_Cm));
    memset(m_CmPressure,  0, sizeof(m_CmPressure));
    memset(m_XCPSpanAbs, 0, sizeof(m_XCPSpanAbs));
    memset(m_XCPSpanRel, 0, sizeof(m_XCPSpanRel));
    memset(m_Re,     0, sizeof(m_Re));
    memset(m_Chord,  0, sizeof(m_Chord));
    memset(m_Offset, 0, sizeof(m_Offset));
    memset(m_XTrTop, 0, sizeof(m_XTrTop));
    memset(m_XTrBot, 0, sizeof(m_XTrBot));
    memset(m_BendingMoment, 0, sizeof(m_BendingMoment));

    memset(m_SpanPos, 0, sizeof(m_SpanPos));
    memset(m_StripArea, 0, sizeof(m_StripArea));

    m_xHinge.clear();
    m_xHinge.insert(0, 1000, 0);
    m_xPanel.clear();
    m_xPanel.insert(0, 1000, 0);

    m_CoG.set(0.0,0.0,0.0);
    m_CoGIxx = m_CoGIyy = m_CoGIzz = m_CoGIxz = 0.0;
    m_VolumeMass = 0.0;

    clearPointMasses();

    m_bIsFin        = false;
    m_bDoubleFin    = false;
    m_bSymFin       = false;
    m_bSymetric     = true;
    m_bWingOut      = false;

    m_Name        = QObject::tr("Wing Name");
    m_WingType        = xfl::MAINWING;
    m_Description = "";

    QColor clr;
    clr.setHsv(QRandomGenerator::global()->bounded(360),
               QRandomGenerator::global()->bounded(55)+30,
               QRandomGenerator::global()->bounded(55)+150);
    m_Color.setRed(  clr.red());
    m_Color.setGreen(clr.green());
    m_Color.setBlue( clr.blue());

    m_WingCL            = 0.0;
    m_CDv               = 0.0;
    m_CDi               = 0.0;
    m_GYm               = 0.0;
    m_IYm               = 0.0;
    m_GCm               = 0.0;
    m_GRm               = 0.0;
    m_ICm               = 0.0;
    m_VCm               = 0.0;
    m_VYm               = 0.0;

    m_CP.set(0.0, 0.0, 0.0);

    m_Maxa = 0.0;
    //    m_AVLIndex = -(int)(qrand()/10000);//improbable value...

    m_nPanels   = 0;
    m_NStation  = 0;

    m_AR         = 0.0;// Aspect ratio
    m_TR         = 0.0;// Taper ratio
    m_GChord     = 0.0;// mean geometric chord
    m_MAChord    = 0.0;// mean aero chord
    m_yMac       = 0.0;
    m_ProjectedArea = 0.0;
    m_ProjectedSpan = 0.0;

    m_nFlaps =  0;
    clearWingSections();
    appendWingSection(.180, .0, 0.0, 0.0, 0.000, 13, 19, xfl::COSINE, xfl::INVERSESINE, "", "");
    appendWingSection(.110, .0, 1.0, 0.0, 0.070, 13, 5,  xfl::COSINE,     xfl::UNIFORM, "", "");

    computeGeometry();

    double length = Length(0);
    for (int is=0; is<m_Section.size(); is++)
    {
        length += Length(is);
        setYPosition(is, length);
        setXPanelDist(is,  xfl::COSINE);
    }
}


/** The public destructor */
Wing::~Wing()
{
    clearWingSections();
    clearPointMasses();
    clearSurfaces();
}


/**
 * Imports the wing geometry from a text file.
 * @param path_to_file the path to the filename as a QString
 */
bool Wing::importDefinition(QString path_to_file, QString errorMessage)
{
    QFile fp(path_to_file);
    double ypos(0), chord(0), offset(0), dihedral(0), twist(0);
    int nx(0), ny(0), px(0), py(0);
    xfl::enumPanelDistribution x_pan_dist(xfl::UNIFORM);
    xfl::enumPanelDistribution y_pan_dist(xfl::UNIFORM);
    char right_buff[512];
    char left_buff[512];

    unsigned counter = 0;


    try{
        if (!fp.open(QIODevice::ReadOnly))
        {
            errorMessage = "Could not open the file for reading";
            return false;
        }
        else {
            QTextStream infile(&fp);
            clearWingSections();
            this->m_Name = infile.readLine();
            while (true)
            {
                counter++;
                infile >> ypos >> chord >> offset >> dihedral >> twist >> nx >> ny;

                infile >> px >> py;

                if(px ==2)         x_pan_dist  = xfl::INVERSESINE;
                else if(px ==  1)  x_pan_dist  = xfl::COSINE;
                else if(px == -2)  x_pan_dist  = xfl::SINE;
                else               x_pan_dist  = xfl::UNIFORM;

                if(py ==2)         y_pan_dist  = xfl::INVERSESINE;
                else if(py ==  1)  y_pan_dist  = xfl::COSINE;
                else if(py == -2)  y_pan_dist  = xfl::SINE;
                else               y_pan_dist  = xfl::UNIFORM;

                infile >> right_buff >> left_buff;

                if (infile.atEnd())
                {
                    fp.close();
                    break;
                }
                //Append the sections convert from mm to m
                appendWingSection(chord,
                                  twist,
                                  ypos,
                                  dihedral,
                                  offset,
                                  nx,
                                  ny,
                                  x_pan_dist,
                                  y_pan_dist,
                                  QString(QString(QLatin1String(right_buff)).replace(QString("/_/"), QString(" "))),
                                  QString(QString(QLatin1String(left_buff)).replace(QString("/_/"), QString(" ")))
                                  );

            }
        }

        //Build the Geometry
        computeGeometry();
        double length = Length(0);
        for (int is=0; is<m_Section.size(); is++)
        {
            length += Length(is);
            setYPosition(is, length);
            setXPanelDist(is,  xfl::COSINE);
        }
    }
    catch (std::iostream::failure e)
    {
        errorMessage = "Unable to import wing definition\n";
        return false;
    }
    return true;
}


/**
 * Exports the wing geometry to a text file.
 * @param path_to_file the path to the filename as a QString
 */
bool Wing::exportDefinition(QString path_to_file, QString errorMessage)
{
    try{
        QFile fp(path_to_file);
        if (!fp.open(QIODevice::WriteOnly)) {
            errorMessage = "Could not open the file for writing";
            return false;
        } else {
            QTextStream out_file(&fp);
            //Iterate the wing sections are write out the data...
            out_file << this->m_Name;

#if QT_VERSION >= 0x050F00
            out_file << Qt::endl;
#else
            out_file << endl;
#endif

            for (int is=0; is<m_Section.size(); is++)
            {
                out_file << YPosition(is) << " " << Chord(is) << " " << Offset(is) \
                         << " " << Dihedral(is) << " " << Twist(is) << " " << NXPanels(is) \
                         << " " << NYPanels(is) << " ";

                switch(XPanelDist(is))
                {
                    case xfl::COSINE:
                        out_file <<  1;
                        break;
                    case xfl::SINE:
                        out_file <<  2;
                        break;
                    case xfl::INVERSESINE:
                        out_file << -2;
                        break;
                    default:
                        out_file <<  0; //XFLR5::UNIFORM
                        break;
                }

                out_file << " " ;

                switch(YPanelDist(is))
                {
                    case xfl::COSINE:
                        out_file <<  1;
                        break;
                    case xfl::SINE:
                        out_file <<  2;
                        break;
                    case xfl::INVERSESINE:
                        out_file << -2;
                        break;
                    default:
                        out_file <<  0; //XFLR5::UNIFORM
                        break;
                }


                if(rightFoilName(is).isEmpty()){
                    out_file  << " " << "/_/";
                } else {
                    QString rightfoilname = rightFoilName(is);
                    out_file  << " " << rightfoilname.replace(QString(" "), QString("/_/")).toLatin1().data();
                }
                if(leftFoilName(is).isEmpty()) {
                    out_file  << " " << "/_/";
                } else {
                    QString leftfoilname = leftFoilName(is);
                    out_file  << " " << leftfoilname.replace(QString(" "), QString("/_/")).toLatin1().data();
                }

#if QT_VERSION >= 0x050F00
            out_file << Qt::endl;
#else
            out_file << endl;
#endif
            }
            fp.close();
        }
    }
    catch (std::iostream::failure e){
        errorMessage = "Unable to import wing definition\n";
        return false;
    }
    return true;
}


/**
 * Calculates the properties of the wing based on the input data.
 * Stores the results in the member variables.
 * Enables the user to see the properties of the wing in real time as the geometry is modified.
 */
void Wing::computeGeometry()
{
    double surface(0);
    double xysurface(0);
    setLength(0, 0.0);
    m_Section[0].m_YProj  = YPosition(0);
    for (int is=1; is<NWingSection(); is++)
        m_Section[is].m_Length = YPosition(is) - YPosition(is-1);
    for (int is=1; is<NWingSection(); is++)
    {
        m_Section[is].m_YProj = YProj(is-1) + Length(is) * cos(Dihedral(is-1)*PI/180.0);
    }

    m_PlanformSpan  = 2.0 * tipPos();
    m_ProjectedSpan = 0.0;
    m_MAChord = 0.0;
    m_yMac    = 0.0;

    for (int is=0; is<NWingSection()-1; is++)
    {
//        Foil const*pFoilA = foil(rightFoil(is));
//        Foil const*pFoilB = foil(rightFoil(is+1));
        surface   += Length(is+1)*(Chord(is)+Chord(is+1))/2.0;//m2
        xysurface += (Length(is+1)*(Chord(is)+Chord(is+1))/2.0)*cos(Dihedral(is)*PI/180.0);
        m_ProjectedSpan += Length(is+1)*cos(Dihedral(is)*PI/180.0);

        m_MAChord += IntegralC2(YPosition(is), YPosition(is+1), Chord(is), Chord(is+1));
        m_yMac    += IntegralCy(YPosition(is), YPosition(is+1), Chord(is), Chord(is+1));
    }

    m_ProjectedSpan *=2.0;
    if(!m_bIsFin || m_bSymFin || m_bDoubleFin)
    {
        m_PlanformArea    = 2.0 * surface;
        m_ProjectedArea = 2.0 * xysurface;
        m_MAChord = m_MAChord * 2.0 / m_PlanformArea;
        m_yMac    = m_yMac    * 2.0 / m_PlanformArea;

        m_GChord  = m_PlanformArea/m_PlanformSpan;
        m_AR      = m_PlanformSpan*m_PlanformSpan/m_PlanformArea;
    }
    else
    {
        m_PlanformArea = surface;
        m_ProjectedArea = xysurface;
        m_MAChord = m_MAChord / m_PlanformArea;
        m_yMac    = m_yMac    / m_PlanformArea;

        m_GChord  = m_PlanformArea/m_PlanformSpan*2.0;
        m_AR      = m_PlanformSpan*m_PlanformSpan/m_PlanformArea/2.0;
    }
    if(tipChord()>0.0)  m_TR = tipChord()/rootChord();
    else                m_TR = 99999.0;

    //calculate the number of flaps
    m_nFlaps = 0;
    double MinPanelSize = 0.0;
    if(s_MinPanelSize>0.0) MinPanelSize = s_MinPanelSize;
    else                   MinPanelSize = m_PlanformSpan;

    for (int is=1; is<NWingSection(); is++)
    {
        Foil const*pFoilA = foil(rightFoilName(is-1));
        Foil const*pFoilB = foil(rightFoilName(is));
        if(pFoilA && pFoilB && (!m_bIsFin || (m_bIsFin && m_bSymFin) || (m_bIsFin && m_bDoubleFin)))
        {
            if(pFoilA->m_bTEFlap && pFoilB->m_bTEFlap && qAbs(YPosition(is)-YPosition(is-1))>MinPanelSize)
                m_nFlaps++;
        }
        pFoilA = foil(leftFoilName(is-1));
        pFoilB = foil(leftFoilName(is));
        if(pFoilA && pFoilB)
        {
            if(pFoilA->m_bTEFlap && pFoilB->m_bTEFlap && qAbs(YPosition(is)-YPosition(is-1))>MinPanelSize)
                m_nFlaps++;
        }
    }
}


#define NXSTATIONS 20
#define NYSTATIONS 40

/**
* Calculates and returns the inertia properties of the structure based on the object's mass and on the existing geometry
* The mass is assumed to have been set previously.
* Mass = mass of the structure, excluding point masses
* @param  &CoG a reference to the CoG point, as a result of the calculation
* @param  &CoGIxx xx axis component of the inertia tensor, calculated at the CoG
* @param  &CoGIyy yy axis component of the inertia tensor, calculated at the CoG
* @param  &CoGIzz zz axis component of the inertia tensor, calculated at the CoG
* @param  &CoGIxz xz axis component of the inertia tensor, calculated at the CoG
*/
void Wing::computeVolumeInertia(Vector3d &CoG, double &CoGIxx, double &CoGIyy, double &CoGIzz, double &CoGIxz) const
{
    QVector<double> ElemVolume(NXSTATIONS*NYSTATIONS*m_Surface.size()); // Qt initializes to zero
    QVector<Vector3d> PtVolume(NXSTATIONS*NYSTATIONS*m_Surface.size());

    double rho(0), LocalSpan(0), LocalVolume(0);
    double LocalChord(0),  LocalArea(0),  tau(0);
    double LocalChord1(0), LocalArea1(0), tau1(0);
    double xrel(0), xrel1(0), yrel(0), ElemArea(0);
    Vector3d ATop, ABot, CTop, CBot, PointNormal, Diag1, Diag2;
    Vector3d PtC4, Pt, Pt1, N;
    CoG.set(0.0, 0.0, 0.0);
    CoGIxx = CoGIyy = CoGIzz = CoGIxz = 0.0;

    //sanity check
    //    Vector3d CoGCheck(0.0,0.0,0.0);
    double CoGIxxCheck(0), CoGIyyCheck(0), CoGIzzCheck(0), CoGIxzCheck(0);
    double recalcMass = 0.0;
    double recalcVolume = 0.0;
    double checkVolume = 0.0;

//    computeGeometry();

    //the mass density is assumed to be homogeneous

    //the local mass is proportional to the chord x foil area
    //the foil's area is interpolated

    //we consider the whole wing, i.e. all left and right surfaces
    //note : in AVL documentation, each side is considered separately

    //first get the CoG - necessary for future application of Huygens/Steiner theorem
    int p = 0;

    for (int j=0; j<m_Surface.size(); j++)
    {
        Surface const &surf = m_Surface.at(j);
        LocalSpan = surf.m_Length/double(NYSTATIONS);
        for (int k=0; k<NYSTATIONS; k++)
        {
            tau  = double(k)   / double(NYSTATIONS);
            tau1 = double(k+1) / double(NYSTATIONS);
            yrel = (tau+tau1)/2.0;

            surf.getSection(tau,  LocalChord,  LocalArea,  Pt);
            surf.getSection(tau1, LocalChord1, LocalArea1, Pt1);
            LocalVolume = (LocalArea+LocalArea1)/2.0 * LocalSpan;
            PtC4.x = (Pt.x + Pt1.x)/2.0;
            PtC4.y = (Pt.y + Pt1.y)/2.0;
            PtC4.z = (Pt.z + Pt1.z)/2.0;

//            CoGCheck += LocalVolume * PtC4;
            for(int l=0; l<NXSTATIONS; l++)
            {
                //browse mid-section
                xrel  = 1.0 - 1.0/2.0 * (1.0-cos(double(l)  *PI /double(NXSTATIONS)));
                xrel1 = 1.0 - 1.0/2.0 * (1.0-cos(double(l+1)*PI /double(NXSTATIONS)));

                surf.getSurfacePoint(xrel,  xrel, yrel,  xfl::TOPSURFACE, ATop, N);
                surf.getSurfacePoint(xrel,  xrel, yrel,  xfl::BOTSURFACE, ABot, N);
                surf.getSurfacePoint(xrel1, xrel1, yrel, xfl::TOPSURFACE, CTop, N);
                surf.getSurfacePoint(xrel1, xrel1, yrel, xfl::BOTSURFACE, CBot, N);
                PtVolume[p] = (ATop+ABot+CTop+CBot)/4.0;
                Diag1 = ATop - CBot;
                Diag2 = ABot - CTop;
                PointNormal = Diag1 * Diag2;
                ElemArea = PointNormal.norm()/2.0;
                if(ElemArea>0.0) ElemVolume[p] = ElemArea * LocalSpan;
                else
                {
                    //no area, means that the foils have not yet been defined for this surface
                    // so just count a unit volume, temporary
                    ElemVolume[p] = 1.0;

                }
                checkVolume += ElemVolume.at(p);
                CoG.x += ElemVolume.at(p) * PtVolume.at(p).x;
                CoG.y += ElemVolume.at(p) * PtVolume.at(p).y;
                CoG.z += ElemVolume.at(p) * PtVolume.at(p).z;
                p++;
            }
        }
    }

    if(checkVolume>PRECISION) rho = m_VolumeMass/checkVolume;
    else                      rho = 0.0;

    if(checkVolume>0.0)  CoG *= 1.0/ checkVolume;
    else                 CoG.set(0.0, 0.0, 0.0);


    // CoG is the new origin for inertia calculation
    p=0;
    for (int j=0; j<m_Surface.size(); j++)
    {
        Surface const &surf = m_Surface.at(j);
        LocalSpan = surf.m_Length/double(NYSTATIONS);
        for (int k=0; k<NYSTATIONS; k++)
        {
            tau  = double(k)     / double(NYSTATIONS);
            tau1 = double(k+1) / double(NYSTATIONS);
            surf.getSection(tau,  LocalChord,  LocalArea,  Pt);
            surf.getSection(tau1, LocalChord1, LocalArea1, Pt1);

            LocalVolume = (LocalArea+LocalArea1)/2.0 * LocalSpan;

            PtC4.x = (Pt.x + Pt1.x)/2.0;
            PtC4.y = (Pt.y + Pt1.y)/2.0;
            PtC4.z = (Pt.z + Pt1.z)/2.0;

            CoGIxxCheck += LocalVolume*rho * ( (PtC4.y-CoG.y)*(PtC4.y-CoG.y) + (PtC4.z-CoG.z)*(PtC4.z-CoG.z) );
            CoGIyyCheck += LocalVolume*rho * ( (PtC4.x-CoG.x)*(PtC4.x-CoG.x) + (PtC4.z-CoG.z)*(PtC4.z-CoG.z) );
            CoGIzzCheck += LocalVolume*rho * ( (PtC4.x-CoG.x)*(PtC4.x-CoG.x) + (PtC4.y-CoG.y)*(PtC4.y-CoG.y) );
            CoGIxzCheck += LocalVolume*rho * ( (PtC4.x-CoG.x)*(PtC4.z-CoG.z) );

            recalcMass   += LocalVolume*rho;
            recalcVolume += LocalVolume;

            for(int l=0; l<NXSTATIONS; l++)
            {
                //browse mid-section
                CoGIxx += ElemVolume.at(p)*rho * ( (PtVolume.at(p).y-CoG.y)*(PtVolume.at(p).y-CoG.y) + (PtVolume.at(p).z-CoG.z)*(PtVolume.at(p).z-CoG.z));
                CoGIyy += ElemVolume.at(p)*rho * ( (PtVolume.at(p).x-CoG.x)*(PtVolume.at(p).x-CoG.x) + (PtVolume.at(p).z-CoG.z)*(PtVolume.at(p).z-CoG.z));
                CoGIzz += ElemVolume.at(p)*rho * ( (PtVolume.at(p).x-CoG.x)*(PtVolume.at(p).x-CoG.x) + (PtVolume.at(p).y-CoG.y)*(PtVolume.at(p).y-CoG.y));
                CoGIxz += ElemVolume.at(p)*rho * ( (PtVolume.at(p).x-CoG.x)*(PtVolume.at(p).z-CoG.z) );
                p++;
            }
        }
    }
}


/**
* Calculates the inertia tensor in geometrical (body) axis :
*  - adds the volume inertia AND the inertia of point masses of all components
*  - the body axis is the frame in which the geometry has been defined
*  - the origin is the Wing's CoG, taking into account all masses
*/
void Wing::computeBodyAxisInertia()
{
    Vector3d LA, VolumeCoG;
    double Ixx(0), Iyy(0), Izz(0), Ixz(0);

    //Get the volume inertia properties in the volume CoG frame of reference
    computeGeometry();
    computeVolumeInertia(VolumeCoG, Ixx, Iyy, Izz, Ixz);
    double TotalMass = m_VolumeMass;

    m_CoG = VolumeCoG *m_VolumeMass;

    // add point masses
    for(int im=0; im<m_PointMass.size(); im++)
    {
        PointMass const &pm = m_PointMass.at(im);

        TotalMass += pm.mass();
        m_CoG     += pm.position() * pm.mass();
    }

    if(TotalMass>0.0) m_CoG = m_CoG/TotalMass;
    else                m_CoG.set(0.0,0.0,0.0);

    // The CoG position is now available, so calculate the inertia w.r.t the CoG
    // using Huygens theorem
    //LA is the displacement vector from the centre of mass to the new axis
    LA = m_CoG-VolumeCoG;
    m_CoGIxx = Ixx + m_VolumeMass * (LA.y*LA.y+ LA.z*LA.z);
    m_CoGIyy = Iyy + m_VolumeMass * (LA.x*LA.x+ LA.z*LA.z);
    m_CoGIzz = Izz + m_VolumeMass * (LA.x*LA.x+ LA.y*LA.y);
    m_CoGIxz = Ixz + m_VolumeMass *  LA.x*LA.z;

    //add the contribution of point masses to total inertia
    for(int im=0; im<m_PointMass.size(); im++)
    {
        PointMass const &pm = m_PointMass.at(im);
        LA = pm.position() - m_CoG;
        m_CoGIxx += pm.mass() * (LA.y*LA.y + LA.z*LA.z);
        m_CoGIyy += pm.mass() * (LA.x*LA.x + LA.z*LA.z);
        m_CoGIzz += pm.mass() * (LA.x*LA.x + LA.y*LA.y);
        m_CoGIxz += pm.mass() * (LA.x*LA.z);
    }
}


/**
 * Constructs the surface objects based on the WingSection data.
 * The position and orientation are defined in the plane object
 * The surfaces are constructed from root to tip, and re-ordered from left tip to right tip
 * One surface object for each of the wing's panels
 * A is the surface's left side, B is the right side
 * @param T the translation to be appied to the wing geometry
 * @param XTilt  the rotation in degrees around the x-axis; used in the case of fins
 * @param YTilt  the rotation in degrees arouns the y-axis; used for wing or elevator tilt
 */
void Wing::createSurfaces(Vector3d const &T, double XTilt, double YTilt)
{
    Vector3d PLA, PTA, PLB, PTB, offset, T1;
    Vector3d Trans(T.x, 0.0, T.z);
    Vector3d O(0.0,0.0,0.0);

    QVector<Vector3d> VNormal(NWingSection());
    QVector<Vector3d> VNSide(NWingSection());

    double MinPanelSize = 0.0;
    if(s_MinPanelSize>0.0) MinPanelSize = s_MinPanelSize;

    m_nPanels = 0;

    //define the normal to each surface
    int nSurf=0;
    VNormal[0].set(0.0, 0.0, 1.0);
    VNSide[0].set(0.0, 0.0, 1.0);

    for(int is=0; is<NWingSection()-1;is++)
    {
        double panelLength = qAbs(YPosition(is)-YPosition(is+1));

//        if (panelLength < MinPanelSize ||  panelLength<planformSpan()/1000.0/2.0)
        if (panelLength < MinPanelSize)
        {
        }
        else
        {
            VNormal[nSurf].set(0.0, 0.0, 1.0);
            VNormal[nSurf].rotateX(O, Dihedral(is));
            nSurf++;
        }
    }

    clearSurfaces();
    for(int jss=0; jss<nSurf; jss++)
    {
        m_Surface.append(Surface());
    }

    if(!m_bIsFin || (m_bIsFin && m_bSymFin) || (m_bIsFin && m_bDoubleFin))
    {
        for(int jss=0; jss<nSurf; jss++)
        {
            m_Surface.append(Surface());
        }
    }

    if(nSurf<=0)  return;
    int NSurfaces = nSurf;

    //define the normals at panel junctions : average between the normals of the two connecting surfaces
    for(int jss=0; jss<nSurf; jss++)
    {
        VNSide[jss+1] = VNormal[jss]+VNormal[jss+1];
        VNSide[jss+1].normalize();
    }

    //we start with the center panel, moving towards the left wing tip
    //however, the calculations are written for surfaces ordered from left tip to right tip,
    //so we number them the opposite way
    nSurf = 0;
    int iSurf = NSurfaces-1;


    for(int jss=0; jss<NWingSection()-1; jss++)
    {
        double panelLength = qAbs(YPosition(jss)-YPosition(jss+1));
//        if (panelLength < MinPanelSize ||  panelLength<planformSpan()/1000.0/2.0)
        if (panelLength < MinPanelSize)
        {
        }
        else
        {
            Surface &surf = m_Surface[iSurf];
            surf.m_pFoilA   = foil(leftFoilName(jss+1));
            surf.m_pFoilB   = foil(leftFoilName(jss));

            surf.m_Length   =  YPosition(jss+1) - YPosition(jss);

            PLA.x =  Offset(jss+1);         PLB.x =  Offset(jss);
            PLA.y = -YPosition(jss+1);      PLB.y = -YPosition(jss);
            PLA.z =  0.0;                   PLB.z =  0.0;
            PTA.x =  PLA.x+Chord(jss+1);    PTB.x = PLB.x+Chord(jss);
            PTA.y =  PLA.y;                 PTB.y = PLB.y;
            PTA.z =  0.0;                   PTB.z =  0.0;

            surf.setCornerPoints(PLA, PTA, PLB, PTB);
            surf.setNormal(); // is (0,0,1)

            surf.rotateX(surf.m_LB, -Dihedral(jss));
            surf.NormalA.set(VNSide[nSurf+1].x, -VNSide[nSurf+1].y, VNSide[nSurf+1].z);
            surf.NormalB.set(VNSide[nSurf].x,   -VNSide[nSurf].y,   VNSide[nSurf].z);

            surf.m_TwistA   =  Twist(jss+1);
            surf.m_TwistB   =  Twist(jss);
            surf.setTwist();

            if(jss>0 && iSurf<m_Surface.count()-1)
            {
                //translate the surface to the left tip of the previous surface
                T1 = m_Surface[iSurf+1].m_LA - surf.m_LB;
                surf.translate(0.0,T1.y,T1.z);
                //                m_Surface[is].m_LB = m_Surface[is+1].m_LA;
                //                m_Surface[is].m_TB = m_Surface[is+1].m_TA;
            }

            nSurf++;

            surf.m_NXPanels = NXPanels(jss);
            surf.m_NYPanels = NYPanels(jss);


            //AVL coding + invert XFLR5::SINE and -sine for left wing
            surf.m_XDistType = XPanelDist(jss);
            if(YPanelDist(jss) == xfl::SINE)              surf.m_YDistType = xfl::INVERSESINE;
            else if(YPanelDist(jss) ==  xfl::COSINE)      surf.m_YDistType =  xfl::COSINE;
            else if(YPanelDist(jss) == xfl::INVERSESINE)  surf.m_YDistType =  xfl::SINE;
            else                                            surf.m_YDistType =  xfl::UNIFORM;

            surf.createXPoints();
            surf.setFlap();
            surf.init();
            surf.m_bIsLeftSurf   = true;
            surf.m_bIsInSymPlane = false;
            surf.m_innerSection = jss;
            surf.m_outerSection = jss+1;

            --iSurf;
        }
    }
    m_Surface[NSurfaces-1].m_bIsCenterSurf = true;//previous left center surface

    // we only need a right wing in the following cases
    //   - if it's an 'ordinary wing'
    //   - if it's a fin, symetrical about the fuselage x-axis
    //   - if it's a symetrical double fin
    if(!m_bIsFin || (m_bIsFin && m_bSymFin) || (m_bIsFin && m_bDoubleFin))
    {
        m_Surface[NSurfaces].m_bIsCenterSurf   = true;//next right center surface
        iSurf = nSurf;
        for (int jss=0; jss<NWingSection()-1; jss++)
        {
            double panelLength = qAbs(YPosition(jss)-YPosition(jss+1));
            if (panelLength < MinPanelSize)
            {
            }
            else
            {
                Surface &surf = m_Surface[iSurf];

                surf.m_pFoilA   = foil(rightFoilName(jss));
                surf.m_pFoilB   = foil(rightFoilName(jss+1));

                surf.m_Length   =  YPosition(jss+1) - YPosition(jss);

                PLA.x = Offset(jss);        PLB.x = Offset(jss+1);
                PLA.y = YPosition(jss);     PLB.y = YPosition(jss+1);
                PLA.z = 0.0;                PLB.z = 0.0;
                PTA.x = PLA.x+Chord(jss);   PTB.x = PLB.x+Chord(jss+1);
                PTA.y = PLA.y;              PTB.y = PLB.y;
                PTA.z = 0.0;                PTB.z = 0.0;

                surf.setCornerPoints(PLA, PTA, PLB, PTB);
                surf.setNormal(); // is (0,0,1)

                surf.rotateX(surf.m_LA, Dihedral(jss));
                surf.NormalA.set(VNSide[iSurf-nSurf].x,   VNSide[iSurf-nSurf].y,   VNSide[iSurf-nSurf].z);
                surf.NormalB.set(VNSide[iSurf-nSurf+1].x, VNSide[iSurf-nSurf+1].y, VNSide[iSurf-nSurf+1].z);

                surf.m_TwistA   =  Twist(jss);
                surf.m_TwistB   =  Twist(jss+1);
                surf.setTwist();

                if(jss>0 && iSurf>0)
                {
                    //translate the surface to the left tip of the previous surface and merge points
                    T1 = m_Surface[iSurf-1].m_LB - surf.m_LA ;
                    surf.translate(0.0, T1.y, T1.z);
                    //                    m_Surface[is].m_LA = m_Surface[is-1].m_LB;
                    //                    m_Surface[is].m_TA = m_Surface[is-1].m_TB;
                }

                surf.m_NXPanels = NXPanels(jss);
                surf.m_NYPanels = NYPanels(jss);

                //AVL coding + invert XFLR5::SINE and -sine for left wing
                surf.m_XDistType = XPanelDist(jss);
                surf.m_YDistType = YPanelDist(jss);

                surf.createXPoints();
                surf.setFlap();
                surf.init();
                surf.m_bIsLeftSurf   = false;
                surf.m_bIsRightSurf  = true;
                surf.m_bIsInSymPlane = false;

                surf.m_innerSection = jss;
                surf.m_outerSection = jss+1;

                iSurf++;
            }
        }
    }

    Vector3d Or(0.0,0.0,0.0);
    if(!m_bIsFin || (m_bIsFin && m_bSymFin))
    {
        NSurfaces*=2;
        for (int jSurf=0; jSurf<NSurfaces; jSurf++)
        {
            m_Surface[jSurf].rotateX(Or, XTilt);
            m_Surface[jSurf].rotateY(Or, YTilt);
            m_Surface[jSurf].translate(Trans);
            if(m_bIsFin && m_bSymFin)
            {
                m_Surface[jSurf].m_bIsInSymPlane = true;
                //                m_Surface[jSurf].m_bIsLeftSurf   = true;
                //                m_Surface[jSurf].m_bIsRightSurf  = false;
            }
        }
        m_Surface[NSurfaces-1].m_bIsTipRight = true;
    }
    else
    {
        if(m_bDoubleFin)
        {
            NSurfaces*=2;
            //rotate surfaces symetrically
            int ns2 = int(NSurfaces/2);
            offset.set(0.0, -T.y, 0.0);
            for(int jSurf=0; jSurf<ns2; jSurf++)
            {
                m_Surface[jSurf].rotateX(Or, +XTilt);
                m_Surface[jSurf].rotateZ(Or, YTilt);
                m_Surface[jSurf].translate(Trans);
                m_Surface[jSurf].translate(offset);
                m_Surface[jSurf].m_bIsInSymPlane = false;
            }
            offset.y = -offset.y;
            for(int jSurf=ns2; jSurf< NSurfaces; jSurf++)
            {
                m_Surface[jSurf].rotateX(Or, -XTilt);
                m_Surface[jSurf].rotateZ(Or, -YTilt);
                m_Surface[jSurf].translate(Trans);
                m_Surface[jSurf].translate(offset);
                m_Surface[jSurf].m_bIsInSymPlane = false;
            }
/*            m_Surface[ns2-1]->m_bIsTipRight = true;
            m_Surface[ns2-1]->m_bIsTipLeft  = true;
            m_Surface[0]->m_bIsTipRight   = true;
            m_Surface[0]->m_bIsTipLeft    = true;

            m_Surface[ns2]->m_bIsTipRight        = true;
            m_Surface[ns2]->m_bIsTipLeft         = true;
            m_Surface[NSurfaces-1]->m_bIsTipRight  = true;
            m_Surface[NSurfaces-1]->m_bIsTipLeft   = true;*/
        }
        else
        {
            //Not a double fin, so just a simple single-sided fin
            for (int jSurf=0; jSurf<NSurfaces; jSurf++)
            {
                m_Surface[jSurf].rotateX(Or, XTilt);
                m_Surface[jSurf].rotateZ(Or, YTilt);
                m_Surface[jSurf].translate(Trans);
                m_Surface[jSurf].m_bIsLeftSurf   = true;
                m_Surface[jSurf].m_bIsRightSurf  = false;
                m_Surface[jSurf].m_bIsInSymPlane = true;
            }
        }
    }

    m_Surface[0].m_bIsTipLeft              = true;
    if(NSurfaces>=1) m_Surface[NSurfaces-1].m_bIsTipRight = true;

    if(NSurfaces>1)
    {
        m_Surface[int(NSurfaces/2)-1].m_bJoinRight   = true;
        //check for a center gap greater than 1/10mm
        int nada = int(NSurfaces/2)-1;
        Q_ASSERT(nada>=0);
        if(YPosition(0)>0.0001)     m_Surface[int(NSurfaces/2)-1].m_bJoinRight   = false;

        if(m_bIsFin && m_bDoubleFin) m_Surface[int(NSurfaces/2)-1].m_bJoinRight   = false;
    }
}


/**
* Calculates the chord lengths at each position of the NStation defined by the LLT or the Panel analysis
*@param NStation the number of stations required by the analysis
*/
void Wing::computeChords(int NStation)
{
    double y(0), yob(0), tau(0);
    double x0(0),y0(0),y1(0),y2(0);
    Vector3d C;

    int NSurfaces = m_Surface.size();

    if(NStation !=0)
    {
        //LLT based
        m_NStation = NStation;

        for (int k=0; k<=NStation; k++)
        {
            yob   = cos(k*PI/NStation);
            y = qAbs(yob * m_PlanformSpan/2);
            for (int is=0; is<NWingSection(); is++)
            {
                if(YPosition(is) < y && y <=YPosition(is+1))
                {
                    tau = (y-YPosition(is))/(YPosition(is+1)-YPosition(is));
                    m_Chord[k]  = Chord(is)+(Chord(is+1)-Chord(is)) * tau;
                    m_Offset[k] = Offset(is)+(Offset(is+1)-Offset(is)) * tau;
                    break;
                }
            }
        }
    }
    else
    {
        //VLM Mesh based
        QVector<double> SpanPosition;
        m_NStation = 0;
        int m = 0;

        x0 = m_Surface[NSurfaces/2].m_LA.x;
        y0 = m_Surface[NSurfaces/2].m_LA.y;

        for (int j=NSurfaces/2; j<NSurfaces; j++)
        {
            Surface const &surf = m_Surface.at(j);
            for (int k=0; k<surf.m_NYPanels; k++)
            {
                //calculate span positions at each station
                surf.getYDist(k, y1, y2);
                SpanPosition.append(y0 + (y1+y2)/2.0*surf.m_Length);
                m++;
            }
            y0+=surf.m_Length;
        }

        m_NStation = 2*m;
        for (m=0; m<m_NStation/2; m++)
        {
            m_SpanPos[m] = -SpanPosition[m_NStation/2-m-1];
            m_SpanPos[m+m_NStation/2] = SpanPosition[m];
        }

        m=0;
        for (int j=0; j<NSurfaces; j++)
        {
            Surface const &surf = m_Surface.at(j);
            for (int k=0; k<surf.m_NYPanels; k++)
            {
                //calculate chords and offsets at each station
                m_Chord[m]     = surf.chord(k);
                //                m_StripArea[m] = m_Chord[m]* surf.Getdl(k);

                surf.getLeadingPt(k, C);
                m_Offset[m] = C.x-x0;

                m_Twist[m]  = surf.twist(k);
                Q_ASSERT(!std::isnan(m_Twist[m]));
                m++;
            }
        }
        m_NStation = m;
    }
}


/**
* Calculates the chord lengths at each position of the NStation defined by the LLT or the Panel analysis
* Overloaded function
*@param NStation the number of stations required by the analysis
*@param *chord pointer to the array of chords lengths at the span stations
*@param *offset pointer to the array of offset positions at the span stations
*@param *twist pointer to the array of twist values at the span stations
*/
void Wing::computeChords(int NStation, double *chord, double *offset, double *twist)
{
    if(NStation !=0)
    {
        //LLT based
        m_NStation = NStation;

        for (int k=0; k<=NStation; k++)
        {
            double yob = cos(k*PI/NStation);
            double y = qAbs(yob * m_PlanformSpan/2);
            for (int is=0; is<NWingSection(); is++)
            {
                if(YPosition(is)<y && y<=YPosition(is+1))
                {
                    double tau = (y-YPosition(is))/(YPosition(is+1)-YPosition(is));
                    chord[k]  = Chord(is)  + (Chord(is+1) -Chord(is))  * tau;
                    offset[k] = Offset(is) + (Offset(is+1)-Offset(is)) * tau;
                    twist[k]  = Twist(is)  + (Twist(is+1) -Twist(is))  * tau;
                    break;
                }
            }
        }
    }
}


/**
* Copies the gemetrical data from an existing Wing
*@param pWing a pointer to the instance of the source Wing object
*/
void Wing::duplicate(Wing const*pWing)
{
    m_NStation      = pWing->m_NStation;
    m_PlanformSpan  = pWing->m_PlanformSpan;
    m_ProjectedSpan = pWing->m_ProjectedSpan;
    m_PlanformArea  = pWing->m_PlanformArea;
    m_ProjectedArea = pWing->m_ProjectedArea;
    m_AR            = pWing->m_AR;
    m_TR            = pWing->m_TR;
    m_GChord        = pWing->m_GChord;
    m_MAChord       = pWing->m_MAChord;
    m_Name      = pWing->m_Name;
    m_bSymetric     = pWing->m_bSymetric;
    m_bIsFin        = pWing->m_bIsFin;
    m_bSymFin       = pWing->m_bSymFin;
    m_bDoubleFin    = pWing->m_bDoubleFin;

    clearWingSections();

    m_Section = pWing->m_Section; // watch out for Qt shallow copies

    computeGeometry();

    m_nFlaps = pWing->m_nFlaps;

    m_VolumeMass = pWing->m_VolumeMass;

    m_CoG = pWing->m_CoG;
    m_CoGIxx = pWing->m_CoGIxx;
    m_CoGIyy = pWing->m_CoGIyy;
    m_CoGIzz = pWing->m_CoGIzz;
    m_CoGIxz = pWing->m_CoGIxz;

    clearPointMasses();

    for(int im=0; im<pWing->m_PointMass.size();im++)
    {
        PointMass const &pm = pWing->m_PointMass.at(im);
        m_PointMass.append(pm);
    }

    m_Description = pWing->m_Description;
    m_Color = pWing->m_Color;
}


/**
* Returns the wing's average sweep from root to tip measured at the quarter chord
* The sweep is calulated as the arctangent of the root and tip quarter-chord points
*@return the value of the average sweep, in degrees
*/
double Wing::averageSweep() const
{
    double xroot = rootOffset() + Chord(0)/4.0;
    double xtip  = tipOffset()  + tipChord()/4.0;
    //    double sweep = (atan2(xtip-xroot, m_PlanformSpan/2.0)) * 180.0/PI;
    return (atan2(xtip-xroot, m_PlanformSpan/2.0)) * 180.0/PI;
}


/**
 * Returns the x-position of the quarter-chord point at a given span position, in geometric axes
 *@param yob the span position where the quarter-chord point will be calculated
 *@param xRef the reference position
 *@return the quarter-chord position
 */
double Wing::C4(double yob) const
{
    double y = qAbs(yob*m_PlanformSpan/2.0);
    for(int is=0; is<NWingSection()-1; is++)
    {
        if(YPosition(is)<= y && y <=YPosition(is+1))
        {
            double tau = (y - YPosition(is))/(YPosition(is+1)-YPosition(is));
            double chord  = Chord(is)  + tau * (Chord(is+1) - Chord(is));
            double offset = Offset(is) + tau * (Offset(is+1) - Offset(is));
            double C4 = offset + chord/4.0;
            return C4;
        }
    }
    // should never reach this point
    Q_ASSERT(false);
    return 0.0;
}

/**
 * Calculates and returns the chord length at a given relative span position
 * @param yob the relative span position in %, where the chord length will be calculated
 * @return the chord length
 */
double Wing::getChord(double yob) const
{
    double chord(0), tau(0), y(0);

    y= qAbs(yob*m_PlanformSpan/2.0);//geometry is symetric
    for(int is=0; is<NWingSection()-1; is++)
    {
        if(YPosition(is)<=y && y <=YPosition(is+1))
        {
            tau = (y - YPosition(is))/(YPosition(is+1)-YPosition(is));
            chord = Chord(is) + tau * (Chord(is+1) - Chord(is));
            return chord;
        }
    }
    return -1.0;
}


/**
 * Calculates and returns the offste value at a given relative span position
 * @param yob the relative span position in %, where the offset will be calculated
 * @return the offset value
 */
double Wing::getOffset(double yob) const
{
    double tau(0), y(0);
    double offset(0);

    y= qAbs(yob*m_PlanformSpan/2.0);
    for(int is=0; is<NWingSection()-1; is++)
    {
        if(YPosition(is)<= y && y <=YPosition(is+1))
        {
            tau = (y - YPosition(is))/(YPosition(is+1)-YPosition(is));
            offset = Offset(is) + tau * (Offset(is+1) - Offset(is));
            return offset;
        }
    }

    return -1.0;
}



/**
 * Calculates and returns the twist angle at a given relative span position
 * @param yob the relative position where the twist angle will be calculated
 * @return the twist angle in degrees
 */
double Wing::getTwist(double yob) const
{
    double tau(0);

    double y= qAbs(yob*m_PlanformSpan/2.0);//geometry is symetric

    // calculate twist at each station
    if (y>=0.0)
    {
        //right wing
        for (int is=0; is<NWingSection()-1; is++)
        {
            if(YPosition(is) <= y && y <=YPosition(is+1))
            {
                tau = (y-YPosition(is))/(YPosition(is+1)-YPosition(is));
                return Twist(is)+(Twist(is+1)-Twist(is)) * tau;
            }
        }
    }
    return 0.0;
}



/**
 * Calculates and returns the dihedral angle at a given relative span position
 * @param yob the relative position where the dihedral angle will be calculated
 * @return the dihedral angle in degrees
 */
double Wing::getDihedral(double yob) const
{
    double y= qAbs(yob*m_PlanformSpan/2.0);//geometry is symetric
    for(int is=0; is<NWingSection()-1; is++)
    {
        if(YPosition(is)<= y && y <=YPosition(is+1))
        {
            if(yob>=0) return Dihedral(is);
            else  return -Dihedral(is);
        }
    }
    return 0.0;
}

/**
 * Returns pointers to the left and right foils of a given span position, and the relative position of the span position between these two foils
 * @param pFoil0 the pointer to the pointer of the left foil
 * @param pFoil1 the pointer to the pointer of the right foil
 * @param y the reference span position
 * @param t the ratio between the position of the two foils
 */
void Wing::getFoils(Foil **pFoil0, Foil **pFoil1, double y, double &t)
{
    if  (y>0.0)
    {
        //search Right wing
        for (int is=0; is<NWingSection()-1; is++)
        {
            if (YPosition(is)<=y && y<=YPosition(is+1))
            {
                *pFoil0 = foil(rightFoilName(is));
                *pFoil1 = foil(rightFoilName(is+1));
                t = (y-YPosition(is))/(YPosition(is+1) - YPosition(is));
                return;
            }
        }
    }
    else
    {
        //search left wing
        y = -y;
        for (int is=0; is<NWingSection()-1; is++)
        {
            if (YPosition(is)<=y && y<YPosition(is+1))
            {
                *pFoil0 = foil(leftFoilName(is));
                *pFoil1 = foil(leftFoilName(is+1));
                t = (y-YPosition(is))/(YPosition(is+1) - YPosition(is));
                return;
            }
        }
    }
    t = 0;
    pFoil0 = nullptr;// use linear
    pFoil1 = nullptr;// use linear
}


/**
 * Returns the total mass of the wing, as the sum of volume and point masses
 * @return the total mass
 */
double Wing::totalMass() const
{
    double TotalMass = m_VolumeMass;
    for(int im=0; im<m_PointMass.size(); im++)
        TotalMass += m_PointMass.at(im).mass();
    return TotalMass;
}


void Wing::surfacePoint(double xRel, double ypos, xfl::enumSurfacePosition pos, Vector3d &Point, Vector3d &PtNormal) const
{
    double fy = qAbs(ypos);

    int iSurf = m_Surface.size()/2;

    double yl = 0.0;

    for (int is=0; is<NWingSection()-1; is++)
    {
        if(qAbs(YPosition(is+1)-YPosition(is))>s_MinPanelSize)
        {
            if(YPosition(is)< fy && fy<=YPosition(is+1))
            {
                break;
            }
            yl += m_Surface.at(iSurf).spanLength();
            iSurf++;
        }
    }


    double yRel = (fabs(ypos)-yl)/m_Surface.at(iSurf).spanLength();
    m_Surface.at(iSurf).getSurfacePoint(xRel, xRel, yRel, pos, Point, PtNormal);

    if(ypos<0) Point.y = -Point.y;
}


/**
 * Returns the relative position in % of a given absolute span position
 * @param SpanPos the absolute span position
 * @return the relative position, in %
 */
double Wing::yrel(double SpanPos) const
{
    double y = qAbs(SpanPos);
    for(int is=0; is<NWingSection()-1; is++)
    {
        if(YPosition(is)<=y && y <YPosition(is+1))
        {
            if(SpanPos>0) return  (y-YPosition(is))/(YPosition(is+1)-YPosition(is));
            else          return  (y-YPosition(is+1))/(YPosition(is)-YPosition(is+1));
        }
    }
    return 1.0;
}

/**
 * The z-position of a specified absolute span position.
 * Used for moment evaluations in LLT, where the wing is defined as a 2D planform
 * @param y the abolute span position
 * @return the absolute z-position
 */
double Wing::zPos(double y) const
{
    double tau(0);
    double ZPos(0);

    y= qAbs(y);
    if(y<=0.0) return 0.0;
    for (int is=0; is<NWingSection()-1; is++)
    {
        if(YPosition(is)< y && y<=YPosition(is+1))
        {
            for (int iss=0; iss<is; iss++)
            {
                ZPos+=Length(iss+1) * sin(Dihedral(iss)*PI/180.0);
            }
            tau = (y - YPosition(is))/(YPosition(is+1)-YPosition(is));
            ZPos += tau * Length(is+1) * sin(Dihedral(is)*PI/180.0);
            return ZPos;
        }
    }
    return 0.0;
}


/**
 * Computes the bending moment at each span position based on the results of the panel analysis
 * Assumes the array of force vectors has been calculated previously
 * @param bThinSurface true if the calculation has been performed on thin VLM surfaces, false in the case of a 3D-panelanalysis
 */
void Wing::panelComputeBending(Panel const*pPanel, bool bThinSurface)
{
    QVector<double> ypos, zpos;
    int coef(0),p(0);
    double bm(0);
    Vector3d Dist(0.0,0.0,0.0);
    Vector3d Moment;


    if(bThinSurface)
    {
        coef = 1;
        p    = 0;
    }
    else
    {
        coef = 2;
        p = m_Surface[0].m_NXPanels;
    }

    int NSurfaces = m_Surface.size();

    for (int j=0; j<NSurfaces; j++)
    {
        Surface const &surf = m_Surface.at(j);

        for (int k=0; k<surf.m_NYPanels; k++)
        {
            if(!bThinSurface)
            {
                ypos.append(pPanel[m_FirstPanelIndex+p].CollPt.y);
                zpos.append(pPanel[m_FirstPanelIndex+p].CollPt.z);
            }
            else
            {
                ypos.append(pPanel[m_FirstPanelIndex+p].VortexPos.y);
                zpos.append(pPanel[m_FirstPanelIndex+p].Vortex.z);
            }

            p += coef*surf.m_NXPanels;
        }
    }

    for (int j=0; j<m_NStation; j++)
    {
        bm = 0.0;
        if (ypos[j]<=0)
        {
            for (int jj=0; jj<j; jj++)
            {
                Dist.y =  -ypos[jj]+ypos[j];
                Dist.z =  -zpos[jj]+zpos[j];
                Moment = Dist * m_F[jj];
                bm += Moment.x ;
            }
        }
        else
        {
            for (int jj=j+1; jj<m_NStation; jj++)
            {
                Dist.y =  ypos[jj]-ypos[j];
                Dist.z =  zpos[jj]-zpos[j];
                Moment = Dist * m_F[jj];
                bm += Moment.x ;
            }
        }
        m_BendingMoment[j] = bm;
    }
}

/**
* Scales the wing chord-wise so that the root chord is set to the NewChord value
*@param NewChord the new value of the root chord
*/
void Wing::scaleChord(double NewChord)
{
    double ratio = NewChord/Chord(0);
    for (int is=0; is<m_Section.size(); is++)
    {
        m_Section[is].m_Chord  *= ratio;
        m_Section[is].m_Offset *= ratio;
    }
    computeGeometry();
}


/**
* Scales the wing span-wise so that the span is set to the NewSpan value
*@param NewSpan the new value of the span
*/
void Wing::scaleSpan(double NewSpan)
{
    for (int is=0; is<m_Section.size(); is++)
    {
        double ypos = YPosition(is) * NewSpan/m_PlanformSpan;
        setYPosition(is, ypos);
        m_Section[is].m_Length *= NewSpan/m_PlanformSpan;
    }
    computeGeometry();
}


/**
* Scales the wing's sweep so that the sweep is set to the NewSweep value
* @param newSweep the new value of the average quarter-chord sweep, in degrees
*/
void Wing::scaleSweep(double newSweep)
{
    double rootOffset = m_Section.first().m_Offset;
    double rootchord4 = rootOffset + Chord(0)/4.0;

    //scale each panel's offset
    for(int is=1; is<NWingSection(); is++)
    {
        double chord4Offset = rootchord4 + tan(newSweep*PI/180.0) * m_Section.at(is).m_YPosition;
        m_Section[is].m_Offset = chord4Offset - Chord(is)/4.0;
    }
    computeGeometry();
}


/**
* Scales the wing's twist angles so that the tip twist is set to the NewTwist value.
*@param NewTwist the new value of the average quarter-chord twist, in degrees
*/
void Wing::scaleTwist(double NewTwist)
{
    if(fabs(tipTwist())>0.0001)
    {
        //scale each panel's twist
        double ratio = NewTwist/tipTwist();

        for(int is=1; is<NWingSection(); is++)
        {
            m_Section[is].m_Twist *= ratio;
        }
    }
    else
    {
        //Set each panel's twist in the ratio of the span position
        for(int is=1; is<NWingSection(); is++)
        {
            m_Section[is].m_Twist = NewTwist*YPosition(is)/(m_PlanformSpan/2.0);
        }
    }
    computeGeometry();
}


/**
* Scales the wing's area.
* All dimensions scaled proportionally sqrt(2).
* Useful e.g. when the weight is changed or when playing with Cl.
* @param newArea the new value of the wing's area.
*/
void Wing::scaleArea(double newArea)
{
    if(fabs(m_PlanformArea)<PRECISION) return;
    if(newArea<PRECISION) return;

    double ratio = sqrt(newArea/m_PlanformArea);

    for (int is=0; is<m_Section.size(); is++)
    {
        double ypos = YPosition(is)*ratio;
        setYPosition(is, ypos);
        m_Section[is].m_Chord     *= ratio;
    }
    computeGeometry();
}


/**
 * Returns the number of mesh panels defined on this Wing's surfaces; the number is given for a double-side mesh of the wing
 * @return the total number of panels
 */
int Wing::VLMPanelTotal(bool bThinSurface) const
{
    double MinPanelSize(0);

    if(s_MinPanelSize>0.0) MinPanelSize = s_MinPanelSize;
    else                   MinPanelSize = m_PlanformSpan/1000.0;
    int total = 0;
    for (int is=0; is<NWingSection()-1; is++)
    {
        //do not create a surface if its length is less than the critical size
        if (qAbs(YPosition(is)-YPosition(is+1)) > MinPanelSize)    total += NXPanels(is)*NYPanels(is);
    }
    if(!m_bIsFin) total *=2;

    if(!bThinSurface)
    {
        total *= 2;
        total += 2*NXPanels(0); // assuming a XFLR5::UNIFORM number of chordwise panels over the wing
    }

    return total;
}


/**
*  Calculates the wing aero coefficients
*  Uses Cp distribution in input for thick surfaces
*  Uses Gamma distribution in input for VLM method
*
*  Input data:
*    Freestream speed Qinf
*    Angle of attack Alpha
*    Cp dstribution for thick wings
*    Mu or Gamma distribution, depending on the analysis type
*    Type of surface :
*        - Thin Surface, i.e. VLM type surfaces, with vortex distribution
*        - Thick Surfaces;, i.e. 3D Panels with source+doublet distribution on panels
*    Type of analysis : viscous or inviscid
*
*  Output
*    centre of pressure position (XCP, YCP)
*    moment coefficients GCm, VCm, ICm, GRm, GYm, VYm, IYm
*/
void Wing::panelComputeOnBody(double QInf, double Alpha, double *Cp, double const*Gamma,
                              double &XCP, double &YCP, double &ZCP,
                              double &GCm, double &VCm, double &ICm, double &GRm, double &GYm, double &VYm,double &IYm,
                              WPolar const*pWPolar, Vector3d const &CoG, Panel const *pPanel)

{
    double CPStrip(0), tau(0), NForce(0), cosa(0), sina(0);
    Vector3d HingeLeverArm,  PtC4Strip, PtLEStrip, ForcePt, SurfaceNormal, LeverArmC4CoG, LeverArmPanelC4, LeverArmPanelCoG;
    Vector3d Force, panelforce, StripForce, viscousDragVector, panelmoment, HingeMoment, viscousDragMoment, GeomMoment;
    Vector3d WindNormal, WindDirection;
    Vector3d Origin(0.0,0.0,0.0);

    //initialize
    m_GRm =0.0;
    m_GCm = m_VCm = m_ICm = 0.0;
    m_GYm = m_VYm = m_IYm = 0.0;

    // Define the number of panels to consider on each strip
    int coef=1;
    if(pWPolar->bThinSurfaces()) coef = 1;    // only mid-surface
    else                         coef = 2;    // top and bottom surfaces

    // Define the wind axes
    cosa = cos(Alpha*PI/180.0);
    sina = sin(Alpha*PI/180.0);
    WindDirection.set( cosa, 0.0, sina);
    WindNormal.set(   -sina, 0.0, cosa);


    // Calculate the Reynolds number on each strip
    for (int m=0; m<m_NStation; m++) m_Re[m] = m_Chord[m] * QInf /pWPolar->m_Viscosity;

    int m=0, p=0, nFlap=0;
    m_FlapMoment.clear();

    // For each of the wing's surfaces, calculate the coefficients on each strip
    // and sum them up to get the wing's overall coefficients

    int NSurfaces = m_Surface.size();

    for (int j=0; j<NSurfaces; j++)
    {
        Surface const &surf = m_Surface.at(j);
        //do not consider left tip patch, if any
        if(!pWPolar->bThinSurfaces() && surf.m_bIsTipLeft) p += surf.m_NXPanels;

        if(surf.m_bTEFlap) m_FlapMoment.append(0.0);

        SurfaceNormal = surf.m_Normal;

        // consider each strip in turn
        for (int k=0; k<surf.m_NYPanels; k++)
        {
            //initialize
            viscousDragVector.set(0.0,0.0,0.0);
            StripForce.set(0.0,0.0,0.0);
            GeomMoment.set(0.0,0.0,0.0);

            m_CmPressure[m]    = 0.0;
            CPStrip        = 0.0;

            surf.getLeadingPt(k, PtLEStrip);
            surf.getC4(k, PtC4Strip, tau);
            if(fabs(pWPolar->m_BetaSpec)>0.0)
            {
                PtC4Strip.rotateZ(Origin, pWPolar->m_BetaSpec);
                PtLEStrip.rotateZ(Origin, pWPolar->m_BetaSpec);
            }

            LeverArmC4CoG = PtC4Strip - CoG;

            for (int l=0; l<coef*surf.m_NXPanels; l++)
            {
                Panel const&panel = pPanel[m_FirstPanelIndex+p];
                // Get the force acting on the panel
                if(pPanel[m_FirstPanelIndex+p].m_Pos!=xfl::MIDSURFACE)
                {
                    ForcePt = panel.CollPt;
                    panelforce = panel.Normal * (-Cp[p]) * panel.Area;      // Newtons/q
                }
                else
                {
                    // for each panel along the chord, add the lift coef
                    ForcePt = panel.VortexPos;
                    panelforce  = WindDirection * panel.Vortex;
                    panelforce *= 2.0 * Gamma[p] /QInf;                                 //Newtons/q

                    if(!pWPolar->bVLM1() && !panel.m_bIsLeading)
                    {
                        Force       = WindDirection * panel.Vortex;
                        Force      *= 2.0 * Gamma[p+1] /QInf;                          //Newtons/q
                        panelforce -= Force;
                    }
                    Cp[p] = panelforce.dot(panel.Normal)/panel.Area;    //
                }
                StripForce += panelforce;                                           // Newtons/q
                NForce = panelforce.dot(SurfaceNormal);                             // Newtons/q

                LeverArmPanelC4    = ForcePt - PtC4Strip;                           // m
                LeverArmPanelCoG   = ForcePt - CoG;                                 // m


                panelmoment = LeverArmPanelC4 * panelforce;                             // N.m/q
                m_CmPressure[m]  += panelmoment.y;                                      // N.m/q, w.r.t. quarter chord point

                GeomMoment += LeverArmPanelCoG * panelforce;                        // N.m/q

                XCP       += ForcePt.x * panelforce.dot(WindNormal); //global center of pressure
                YCP       += ForcePt.y * panelforce.dot(WindNormal);
                ZCP       += ForcePt.z * panelforce.dot(WindNormal);
                CPStrip   += ForcePt.x * NForce;

                if(surf.m_bTEFlap)
                {
                    if(surf.isFlapPanel(panel.m_iElement))
                    {
                        //then p is on the flap, so add its contribution
                        HingeLeverArm = ForcePt - surf.m_HingePoint;
                        HingeMoment = HingeLeverArm * panelforce;                   //N.m/q
                        m_FlapMoment[nFlap] += HingeMoment.dot(surf.m_HingeVector)* pWPolar->density() * QInf * QInf/2.0;  //N.m
                    }
                }
                p++;
            }

            // calculate center of pressure position
            NForce = StripForce.dot(SurfaceNormal);
            m_XCPSpanRel[m]    = (CPStrip/NForce - PtLEStrip.x)/m_Chord[m];
            m_XCPSpanAbs[m]    =  CPStrip/NForce ;

            // add viscous properties, if required
            if(pWPolar->bViscous()) viscousDragVector = WindDirection * (m_PCd[m]) * m_StripArea[m];   // N/q
            else                    viscousDragVector.set(0.0,0.0,0.0);

            // global moments
            viscousDragMoment =  LeverArmC4CoG * viscousDragVector;         // N.m/q, w.r.t. CoG

            m_GRm += GeomMoment.dot(WindDirection);

            m_VYm += viscousDragMoment.dot(WindNormal);                     // N.m/q

            //            m_IYm += -m_ICd[m] * m_StripArea[m] * PtC4Strip.y ;
            m_IYm += GeomMoment.dot(WindNormal);                            // N.m/q

            m_VCm += viscousDragMoment.y;                                   // N.m/q
            m_ICm += GeomMoment.y;                                          // N.m/q

            m_CmPressure[m] *= 1.0 /m_Chord[m]/m_StripArea[m];              // N.m/q, w.r.t. quarter chord point
            m_Cm[m] = (GeomMoment.y + viscousDragMoment.y)/m_Chord[m]/m_StripArea[m];   // N.m/q, w.r.t. CoG
            m++;
        }
        //do not consider right tip patch
        if(!pWPolar->bThinSurfaces() && surf.m_bIsTipRight) p += surf.m_NXPanels;
        if(surf.m_bTEFlap) nFlap++;
    }


    //global plane dimensionless coefficients
    GCm += m_VCm + m_ICm; // Pitching moment, sum of Viscous and Induced parts
    VCm += m_VCm;

    ICm += m_ICm;

    //sign convention for rolling and yawing is opposite to algebric results
    GRm -= m_GRm; // Rolling moment, no induced contribution
    GYm -= (m_VYm + m_IYm); // Yawing moment, sum of Viscous and Induced parts
    VYm -= m_VYm;
    IYm -= m_IYm;
}


/**
* In input, takes the speed QInf and the distribution of lift coefficients m_Cl[] along the span
* In output, returns for each span station
*     - The Reynolds number m_Re[]
*    - The viscous drag coefficient m_PCd[]
*      - The top and bottom transition points m_XTrtop[] and m_XTrBot[]
*/
void Wing::panelComputeViscous(double QInf, const WPolar *pWPolar, double &WingVDrag, bool bViscous, QString &OutString)
{
    QString string, strong, strLength;

    bool bPointOutRe, bPointOutCl, bOutRe, bError;
    double tau = 0.0;
    Vector3d PtC4;

    OutString.clear();

    WingVDrag = 0.0;

    bOutRe = bError = bPointOutRe = bPointOutCl = false;

    strLength = "m";

    // Calculate the Reynolds number on each strip
    for (int m=0; m<m_NStation; m++)  m_Re[m] = m_Chord[m] * QInf /pWPolar->m_Viscosity;

    if(!bViscous)
    {
        for(int m=0; m <m_NStation; m++)
        {
            m_PCd[m] = m_XTrTop[m] = m_XTrBot[m] = 0.0;
        }
        return;
    }

    //Interpolate the viscous properties from the foil's type 1 polar mesh
    int m=0;
    for (int j=0; j<m_Surface.size(); j++)
    {
        Surface const &surf = m_Surface.at(j);
        for(int k=0; k<surf.m_NYPanels; k++)
        {
            bOutRe = bPointOutRe = false;
            bPointOutCl = false;
            surf.getC4(k, PtC4, tau);

            m_PCd[m]    = getInterpolatedVariable(2, surf.m_pFoilA, surf.m_pFoilB, m_Re[m], m_Cl[m], tau, bOutRe, bError);
            bPointOutRe = bOutRe || bPointOutRe;
            if(bError) bPointOutCl = true;

            m_XTrTop[m] = getInterpolatedVariable(5, surf.m_pFoilA, surf.m_pFoilB, m_Re[m], m_Cl[m], tau, bOutRe, bError);
            bPointOutRe = bOutRe || bPointOutRe;
            if(bError) bPointOutCl = true;

            m_XTrBot[m] = getInterpolatedVariable(6, surf.m_pFoilA, surf.m_pFoilB, m_Re[m], m_Cl[m], tau, bOutRe, bError);
            bPointOutRe = bOutRe || bPointOutRe;
            if(bError) bPointOutCl = true;

            if(bPointOutCl)
            {
                strong = QString(QObject::tr("           Span pos = %1 ")).arg(m_SpanPos[m], 9,'f',2);
                strong += strLength;
                strong += ",  Re = ";
                string = QString::asprintf("%.0f", m_Re[m]);
                strong += string;

                strong+= QString(QObject::tr(",  Cl = %1 could not be interpolated")+"\n").arg(m_Cl[m],6,'f',2);
                OutString += strong;

                m_bWingOut = true;
            }
            else if(bPointOutRe)
            {
                strong = QString(QObject::tr("           Span pos = %1 ")).arg(m_SpanPos[m],9,'f',2);
                strong += strLength;
                strong += ",  Re = ";
                string = QString::asprintf("%.0f", m_Re[m]);
                strong += string;
                strong += QString(QObject::tr(",  Cl = %1 is outside the flight envelope")+"\n").arg(m_Cl[m],6,'f',2);
                OutString += strong;

                m_bWingOut = true;
            }

            // Sum the total viscous drag of this wing
            WingVDrag  += m_PCd[m] * m_StripArea[m];
            m++;
        }
    }
}


/**
 * Identifies if a given index of a panel belongs to this wing or not
 * @param nPanel the index of the panel
 * @return true if the panel belongs to the wing, false otherwise
 */
bool Wing::isWingPanel(int nPanel, Panel const *pPanel)
{
    for(int p=0; p<m_nPanels; p++)
    {
        if(nPanel==pPanel[m_FirstPanelIndex+p].m_iElement) return true;
    }
    return false;
}


/**
 * Identifies if a given index of a node belongs to this wing or not
 * @param nNode the index of a node
 * @return true if the node belongs to the wing, false otherwise
 */
bool Wing::isWingNode(int nNode, Panel const *pPanel)
{
    for(int p=0; p<m_nPanels; p++)
    {
        if(nNode==pPanel[m_FirstPanelIndex+p].m_iLA) return true;
        if(nNode==pPanel[m_FirstPanelIndex+p].m_iLB) return true;
        if(nNode==pPanel[m_FirstPanelIndex+p].m_iTA) return true;
        if(nNode==pPanel[m_FirstPanelIndex+p].m_iTB) return true;
    }
    return false;
}


/**
 * Removes the section in the geometry of the wing identified by its index
 * @param iSection the index of the section
 */
void Wing::removeWingSection(int const iSection)
{
    if(iSection<0 || iSection>=m_Section.size()) return;
    m_Section.removeAt(iSection);
}


/**
 * Inserts a section in the geometry of the wing at a postion identified by its index
 * @param iSection the index of the section
 */
void Wing::insertSection(int iSection)
{
    if(iSection==0)                      m_Section.prepend(WingSection()); //redundant
    else if(iSection>=m_Section.size())  m_Section.append(WingSection()); //redundant
    else                                 m_Section.insert(iSection, WingSection());
}


/**
 * Appends a new section at the tip of the wing, with values specified as input parameters
 */
void Wing::appendWingSection(double Chord, double Twist, double Pos, double Dihedral, double Offset,
                             int NXPanels, int NYPanels, xfl::enumPanelDistribution XPanelDist, xfl::enumPanelDistribution YPanelDist,
                             const QString &RightFoilName, const QString &LeftFoilName)
{
    m_Section.append(WingSection());
    WingSection &WS = m_Section.last();

    WS.m_Chord      = Chord;
    WS.m_Twist      = Twist;
    WS.m_YPosition  = Pos ;
    WS.m_Dihedral   = Dihedral;
    WS.m_Offset     = Offset ;

    WS.m_NXPanels   = NXPanels ;
    WS.m_NYPanels   = NYPanels;
    WS.m_XPanelDist = XPanelDist;
    WS.m_YPanelDist = YPanelDist;

    WS.m_RightFoilName  = RightFoilName;
    WS.m_LeftFoilName   = LeftFoilName;
}


bool Wing::isWingFoil(const Foil *pFoil) const
{
    if(!pFoil) return false;

    for (int iws=0; iws<NWingSection(); iws++)
    {
        if(pFoil->name() == m_Section.at(iws).m_RightFoilName)
        {
            return true;
        }
    }

    if(!m_bSymetric)
    {
        for (int iws=0; iws<NWingSection(); iws++)
        {
            if(pFoil->name() == m_Section.at(iws).m_LeftFoilName)
            {
                return true;
            }
        }
    }
    return false;
}


/** Finds the intersection point of a line originating at point O and with unit vector U
 * @param O the origin of the line
 * @param U the unit vector on the lie
 * @param I the intersection point, if any, otherwise returns an unchanged value
 * @return true if an intersection point was found, false otherwise
 */
bool Wing::intersectWing(Vector3d O,  Vector3d U, Vector3d &I) const
{
    double dist(0);

    for(int j=0; j<m_Surface.count(); j++)
    {
        Surface const &surf = m_Surface.at(j);
        if(xfl::intersect(surf.m_LA, surf.m_LB,
                          surf.m_TA, surf.m_TB,
                          surf.m_Normal,
                          O, U, I, dist)) return true;
    }
    return false;
}



void Wing::getTextureUV(int iSurf, double *leftV, double *rightV, double &leftU, double &rightU, int nPoints) const
{
    double xRelA(0), xRelB(0), xA(0), xB(0), yA(0), yB(0);
    double xMin=100000, xMax=-100000, yMin=0, yMax=0;
    int iSectionA=0, iSectionB=1;

    Surface const &surf = m_Surface.at(iSurf);

    if(surf.isLeftSurf())
    {
        iSectionB = surf.innerSection();
        iSectionA = surf.outerSection();
    }
    else
    {
        iSectionA = surf.innerSection();
        iSectionB = surf.outerSection();
    }

    if(iSectionA<0 || iSectionB<0 || iSectionA>=m_Section.size() ||iSectionB>=m_Section.size()) return;

    for(int is=0; is<m_Section.count(); is++)
    {
        WingSection const &WS = m_Section.at(is);
        xMin = std::min(xMin, WS.m_Offset);
        xMax = std::max(xMax, WS.m_Offset + WS.m_Chord);
    }

    for(int i=0; i<nPoints; i++)
    {
        if(m_Surface[iSurf].m_NXFlap>0 && m_Surface[iSurf].m_pFoilA && m_Surface[iSurf].m_pFoilB)
        {
            int nPtsTr = nPoints/3;
            int nPtsLe = nPoints-nPtsTr;

            if(i<nPtsTr)
            {
                xRelA = 1.0/2.0*(1.0-cos(PI * double(i)/double(nPtsTr-1)))* (surf.m_pFoilA->m_TEXHinge/100.);
                xRelB = 1.0/2.0*(1.0-cos(PI * double(i)/double(nPtsTr-1)))* (surf.m_pFoilB->m_TEXHinge/100.);
            }
            else
            {
                int j = i-nPtsTr;
                xRelA = surf.m_pFoilA->m_TEXHinge/100. + 1.0/2.0*(1.0-cos(PI* double(j)/double(nPtsLe-1))) * (1.-surf.m_pFoilA->m_TEXHinge/100.);
                xRelB = surf.m_pFoilB->m_TEXHinge/100. + 1.0/2.0*(1.0-cos(PI* double(j)/double(nPtsLe-1))) * (1.-surf.m_pFoilB->m_TEXHinge/100.);
            }
        }
        else
        {
            xRelA  = 1.0/2.0*(1.0-cos(PI * double(i)/double(nPoints-1)));
            xRelB  = xRelA;
        }


        //        xRel  = 1.0/2.0*(1.0-cos( double(i)*PI   /(double)(nPoints-1)));
        xA = m_Section.at(iSectionA).m_Offset + m_Section.at(iSectionA).m_Chord*xRelA;
        xB = m_Section.at(iSectionB).m_Offset + m_Section.at(iSectionB).m_Chord*xRelB;

        leftV[i]  = (xA-xMin)/(xMax-xMin);
        rightV[i] = (xB-xMin)/(xMax-xMin);
    }


    yMin = m_Section.first().m_YPosition;
    yMax = m_Section.last().m_YPosition;

    yA = m_Section.at(iSectionA).m_YPosition;
    yB = m_Section.at(iSectionB).m_YPosition;
    if(surf.isLeftSurf())
    {
        leftU = 1.0-(yA-yMin)/(yMax-yMin);
        rightU  = 1.0-(yB-yMin)/(yMax-yMin);
    }
    else
    {
        leftU  = (yA-yMin)/(yMax-yMin);
        rightU = (yB-yMin)/(yMax-yMin);
    }
}


/**
 * Loads or Saves the data of this Wing to a binary file.
 * @param ar the QDataStream object from/to which the data should be serialized
 * @param bIsStoring true if saving the data, false if loading
 * @return true if the operation was successful, false otherwise
 */
bool Wing::serializeWingWPA(QDataStream &ar, bool bIsStoring)
{
    int ArchiveFormat(0);// identifies the format of the file

    if(bIsStoring)
    {
        //not storing to .wpa format anymore
        return true;
    }
    else
    {
        // loading code
        float f(0),g(0),h(0);
        int k(0);
        //1010 : added storage of alpha channel + added a provision for ints and floats
        //1009 : QFLR5 v0.03 : added mass properties for inertia calculations
        //1008 : QFLR5 v0.02 : Added wing description field
        //1007 : Changed length units to m
        //1006 : Added Wing Color v2.99-15
        //1005 : Added Chordwise spacing (v2.99-00)
        //1004 : corrected NXPanels, NYPanels, YPanelDist to int (v1.99-33)
        //1003 : AVL Format (v1.99-18)
        //1002 : save VLM Mesh (v1.99-12)
        //1001 : initial format

        ar >> ArchiveFormat;

        if (ArchiveFormat <1001 || ArchiveFormat>1100) {
            m_Name = "";
            return false;
        }

        xfl::readCString(ar,m_Name);
        if (m_Name.length() ==0) return false;

        if (ArchiveFormat >=1008)
        {
            xfl::readCString(ar, m_Description);
        }

        ar >> k;
        if(k!=0){
            m_Name = "";
            return false;
        }

        ar >> k;
        if (k==1) m_bSymetric = true;
        else if (k==0) m_bSymetric = false;
        else{
            m_Name = "";
            return false;
        }
        //        m_bVLMSymetric = m_bSymetric;

        int NPanel;
        ar >> NPanel;
        if(NPanel <0 || NPanel>1000) return false;

        clearWingSections();
        m_Section.resize(NPanel+1);

        QString strFoil;

        for (int is=0; is<=NPanel; is++)
        {
            xfl::readCString(ar, strFoil);
            m_Section[is].m_RightFoilName = strFoil;
        }
        for (int is=0; is<=NPanel; is++)
        {
            xfl::readCString(ar, strFoil);
            m_Section[is].m_LeftFoilName = strFoil;
        }

        for (int is=0; is<=NPanel; is++)
        {
            ar >> f; m_Section[is].m_Chord=double(f);
            if (qAbs(Chord(is)) <0.0)
            {
                m_Name = "";
                return false;
            }
        }

        for (int is=0; is<=NPanel; is++)
        {
            ar >> f;   setYPosition(is, double(f));
            if (qAbs(YPosition(is)) <0.0)
            {
                m_Name = "";
                return false;
            }
        }
        for (int is=0; is<=NPanel; is++)
        {
            ar >> f; m_Section[is].m_Offset=double(f);
        }

        if(ArchiveFormat<1007)
        {
            //convert mm to m
            for (int is=0; is<=NPanel; is++)
            {
                double ypos = YPosition(is)/1000.0;
                setYPosition(is, ypos);
                m_Section[is].m_Chord  /= 1000.0;
                m_Section[is].m_Offset /= 1000.0;
            }

        }
        for (int is=0; is<=NPanel; is++)
        {
            ar >> f; m_Section[is].m_Dihedral=double(f);
        }
        for (int is=0; is<=NPanel; is++)
        {
            ar >> f; m_Section[is].m_Twist=double(f);
        }

        ar >> f; //m_XCmRef=f;

        ar >> k;

        for (int is=0; is<=NPanel; is++)
        {
            if(ArchiveFormat<=1003)
            {
                ar >> f;
                m_Section[is].m_NXPanels = int(f);
            }
            else
                ar >> m_Section[is].m_NXPanels;
        }

        for (int is=0; is<=NPanel; is++)
        {
            if(ArchiveFormat<=1003)
            {
                ar >> f;
                m_Section[is].m_NYPanels = int(f);
            }
            else     ar >> m_Section[is].m_NYPanels;
        }
        int total = 0;
        for (int is=0; is<NPanel; is++)
        {
            total += NYPanels(is);
        }


        int spanpos = 0;
        int vlmpanels = 0;
        for (int is=0; is<NPanel; is++)
        {
            spanpos  += NYPanels(is);
            vlmpanels +=NXPanels(is)*NYPanels(is);
        }


        if (ArchiveFormat >=1005)
        {
            for(int is=0; is<=NPanel; is++)
            {
                ar >> k;
                if     (k==1)  setXPanelDist(is, xfl::COSINE);
                else if(k==2)  setXPanelDist(is, xfl::SINE);
                else if(k==-2) setXPanelDist(is, xfl::INVERSESINE);
                else           setXPanelDist(is, xfl::UNIFORM);  //case 0
            }
        }

        for (int is=0; is<=NPanel; is++)
        {
            ar >> k;
            if     (k==1)  setYPanelDist(is, xfl::COSINE);
            else if(k==2)  setYPanelDist(is, xfl::SINE);
            else if(k==-2) setYPanelDist(is, xfl::INVERSESINE);
            else           setYPanelDist(is, xfl::UNIFORM);  //case 0
        }

        if(ArchiveFormat>=1006)
        {
            int r,g,b;
            xfl::readCOLORREF(ar, r,g,b);
        }

        if(ArchiveFormat>=1009)
        {
            ar >> f;  m_VolumeMass = double(f);
            int nMass;

            ar >> nMass;
            QVarLengthArray<double> mass;
            QVarLengthArray<Vector3d> position;
            QVarLengthArray<QString> tag;

            for(int im=0; im<nMass; im++)
            {
                ar >> f;
                mass.append(double(f));
            }
            for(int im=0; im<nMass; im++)
            {
                ar >> f >> g >> h;
                position.append(Vector3d(double(f),double(g),double(h)));
            }
            for(int im=0; im<nMass; im++)
            {
                tag.append("");
                xfl::readCString(ar, tag[im]);
            }

            clearPointMasses();
            for(int im=0; im<nMass; im++)
            {
                m_PointMass.append({mass.at(im), position.at(im), tag.at(im)});
            }
        }

        if(ArchiveFormat>=1010)
        {
            ar >> k; m_Color.setAlpha(k);
            for(int i=0; i<20; i++) ar>>f;
            for(int i=0; i<20; i++) ar>>k;
        }

        computeGeometry();
        return true;
    }
}


/**
 * Loads or Saves the data of this Wing to a binary file.
 * @param ar the QDataStream object from/to which the data should be serialized
 * @param bIsStoring true if saving the data, false if loading
 * @return true if the operation was successful, false otherwise
 */
bool Wing::serializeWingXFL(QDataStream &ar, bool bIsStoring)
{
    QString tag;
    QString rightfoil, leftfoil;
    int nx(0), ny(0);
    int k(0), n(0), is(0);
    int ArchiveFormat(0);// identifies the format of the file
    double dble(0), dm(0), px(0), py(0), pz(0);
    double chord(0), twist(0), pos(0), dihedral(0), offset(0);
    xfl::enumPanelDistribution xDist(xfl::UNIFORM), yDist(xfl::UNIFORM);

    if(bIsStoring)
    {
        ar << 100001;
        ar << m_Name;
        ar << m_Description;

        xfl::writeQColor(ar, m_Color.red(), m_Color.green(), m_Color.blue(), m_Color.alpha());

        ar << m_bSymetric;

        ar << NWingSection();

        for (is=0; is<NWingSection(); is++)
        {
            ar << rightFoilName(is);
            ar << leftFoilName(is);
            ar << Chord(is);
            ar << YPosition(is);
            ar << Offset(is);
            ar << Dihedral(is);
            ar << Twist(is);
            ar << NXPanels(is);
            ar << NYPanels(is);

            switch(XPanelDist(is))
            {
                case xfl::COSINE:
                    ar <<  1;
                    break;
                case xfl::SINE:
                    ar <<  2;
                    break;
                case xfl::INVERSESINE:
                    ar << -2;
                    break;
                default:
                    ar <<  0; //XFLR5::UNIFORM
                    break;
            }

            switch(YPanelDist(is))
            {
                case xfl::COSINE:
                    ar <<  1;
                    break;
                case xfl::SINE:
                    ar <<  2;
                    break;
                case xfl::INVERSESINE:
                    ar << -2;
                    break;
                default:
                    ar <<  0; //XFLR5::UNIFORM
                    break;
            }
        }

        ar << m_VolumeMass;
        ar << m_PointMass.size();
        for(int im=0; im<m_PointMass.size(); im++)
        {
            PointMass const &pm = m_PointMass.at(im);
            ar << pm.mass();
            ar << pm.position().x << pm.position().y << pm.position().z;
            ar << pm.tag();
        }

        ar<<1 ; // formerly bTextures

        // space allocation for the future storage of more data, without need to change the format
        for (int i=1; i<19; i++) ar << 0;
        switch (wingType()) {
            case xfl::MAINWING:
                ar<<0;
                break;
            case xfl::SECONDWING:
                ar<<1;
                break;
            case xfl::ELEVATOR:
                ar<<2;
                break;
            case xfl::FIN:
                ar<<3;
                break;
            case xfl::OTHERWING:
                ar<<4;
                break;
        }

        for (int i=0; i<50; i++) ar << 0.0;

        return true;
    }
    else
    {
        ar >> ArchiveFormat;
        if(ArchiveFormat<100000 || ArchiveFormat>100001) return false;

        ar >> m_Name;
        ar >> m_Description;

/*        int a,r,g,b;
        xfl::readQColor(ar, r, g, b, a);
        m_Color = {r,g,b,a};
*/
        ar >> m_Color;

        ar >> m_bSymetric;

        clearWingSections();
        ar >> n;
        for (int i=0; i<n; i++)
        {
            ar >> rightfoil;
            ar >> leftfoil;
            ar >> chord;
            ar >> pos;
            ar >> offset;
            ar >> dihedral;
            ar >> twist;
            ar >> nx;
            ar >> ny;

            ar >> k;
            if(k==1)       xDist = xfl::COSINE;
            else if(k== 2) xDist = xfl::SINE;
            else if(k==-2) xDist = xfl::INVERSESINE;
            else           xDist = xfl::UNIFORM;

            ar >> k;
            if     (k== 1) yDist = xfl::COSINE;
            else if(k== 2) yDist = xfl::SINE;
            else if(k==-2) yDist = xfl::INVERSESINE;
            else           yDist = xfl::UNIFORM;

            appendWingSection(chord, twist, pos, dihedral, offset, nx, ny, xDist, yDist, rightfoil, leftfoil);
        }

        ar >> m_VolumeMass;
        clearPointMasses();
        ar >> n;
        for(int i=0; i<n; i++)
        {
            ar >> dm >> px >> py >> pz;
            ar >> tag;
            m_PointMass.append({dm, Vector3d(px, py, pz), tag});
        }

        ar>>k; //formerly bTextures

        // space allocation
        for (int i=1; i<19; i++) ar >> k;
        ar >>k;
        switch (k) {
            case 0:
                m_WingType=xfl::MAINWING;
                break;
            case 1:
                m_WingType=xfl::SECONDWING;
                break;
            case 2:
                m_WingType=xfl::ELEVATOR;
                break;
            case 3:
                m_WingType=xfl::FIN;
                break;
            case 4:
                m_WingType=xfl::OTHERWING;
                break;
            default:
                break;
        }

        for (int i=0; i<50; i++) ar >> dble;

        computeGeometry();
        return true;
    }
}


/**
* Scales the wing's Aspect Ratio.
* Chords and span are scaled accordingly but the wing area remains unchanged.
* Good for general optimisation.
* @param newAR the new value of the aspect ratio.
*/
void Wing::scaleAR(double newAR)
{
    if(m_AR<PRECISION)  return;
    if(newAR<PRECISION) return;

    double ratio = sqrt(newAR/m_AR);

    for (int is=0; is<m_Section.size(); is++)
    {
        double ypos = YPosition(is)*ratio;
        setYPosition(is, ypos);
        m_Section[is].m_Chord     /= ratio;
    }
    computeGeometry();
}


/**
* Scales the wing's Taper Ratio.
* Root chord is unchanged, all other chords are scale proportionnally to their span position.
* @param newTR the new value of the taper ratio.
*/
void Wing::scaleTR(double newTR)
{
    if(m_TR<PRECISION)  return;
    if(newTR<PRECISION) return;

    double Ratio = m_TR/newTR;
    for (int is=0; is<m_Section.size(); is++)
    {
        double yRel = YPosition(is)/m_PlanformSpan *2.0;
        double cRatio = 1.0 +  yRel * (Ratio-1.0);
        m_Section[is].m_Chord     *= cRatio;
    }
    computeGeometry();
}


/**
 * Generate the facet data for export to a text file in STL Format.
 * @param out the instance of the QTextStream to which the output will be directed
 */
void Wing::exportSTLBinaryTriangle(QDataStream &outStreamData, Vector3d N, Vector3d Pt0, Vector3d Pt1, Vector3d Pt2, float unit)
{
    static short zero = 0;
    static char buffer[12];

    xfl::writeFloat(outStreamData, N.xf());
    xfl::writeFloat(outStreamData, N.yf());
    xfl::writeFloat(outStreamData, N.zf());
    xfl::writeFloat(outStreamData, Pt0.xf()*unit);
    xfl::writeFloat(outStreamData, Pt0.yf()*unit);
    xfl::writeFloat(outStreamData, Pt0.zf()*unit);
    xfl::writeFloat(outStreamData, Pt1.xf()*unit);
    xfl::writeFloat(outStreamData, Pt1.yf()*unit);
    xfl::writeFloat(outStreamData, Pt1.zf()*unit);
    xfl::writeFloat(outStreamData, Pt2.xf()*unit);
    xfl::writeFloat(outStreamData, Pt2.yf()*unit);
    xfl::writeFloat(outStreamData, Pt2.zf()*unit);

    memcpy(buffer, &zero, sizeof(short));
    outStreamData.writeRawData(buffer, 2);

}

/**
 * Export the wing geometry to a binary file in STL Format.
 * @param out the instance of the QTextStream to which the output will be directed
 */
void Wing::exportSTLBinary(QDataStream &outStream, int CHORDPANELS, int SPANPANELS, float unit)
{
    /***
     *  UINT8[80] – Header
     *     UINT32 – Number of triangles
     *
     *     foreach triangle
     *     REAL32[3] – Normal vector
     *     REAL32[3] – Vertex 1
     *     REAL32[3] – Vertex 2
     *     REAL32[3] – Vertex 3
     *     UINT16 – Attribute byte count
     *     end
    */

    Vector3d N, Pt;
    QVector<Vector3d> NormalA(CHORDPANELS+1);
    QVector<Vector3d> NormalB(CHORDPANELS+1);
    QVector<Vector3d> PtLeft(CHORDPANELS+1);
    QVector<Vector3d> PtRight(CHORDPANELS+1);
    QVector<Vector3d> PtBotLeft(CHORDPANELS+1);
    QVector<Vector3d> PtBotRight(CHORDPANELS+1);

    NormalA.fill(Vector3d());
    NormalB.fill(Vector3d());


    //    80 character header, avoid word "solid"
    //                       0123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789
    QString strong =     "binary STL file                                                                ";
    xfl::writeCString(outStream, strong);

    //Number of triangles
    // nSurfaces
    //   *CHORDPANELS*SPANPANELS   quads
    //   *2                        2 triangles/quad
    //   *2                        top and bottom surfaces
    // 2 Tip patches
    //   1 LE triangle
    //   1 TE triangle
    //   CHORDPANELS-1  quads
    //   *2 triangles/quad

    int nTriangles = m_Surface.count() * CHORDPANELS * SPANPANELS * 2 *2
                     + 2* ((CHORDPANELS-2)*2 + 2);
    outStream << nTriangles;

    short zero = 0;

    N.set(0.0, 0.0, 0.0);
    int iTriangles = 0;

    char buffer[12];

    for (int j=0; j<m_Surface.size(); j++)
    {
        Surface const &surf = m_Surface.at(j);
        //top surface
        for(int is=0; is<SPANPANELS; is++)
        {
            surf.getSidePoints(xfl::TOPSURFACE, nullptr, PtLeft, PtRight,
                                           NormalA, NormalB, CHORDPANELS+1);

            double tauA = double(is)   /double(SPANPANELS);
            double tauB = double(is+1) /double(SPANPANELS);
            double tau = (tauA+tauB)/2.0;
            for(int ic=0; ic<CHORDPANELS; ic++)
            {
                //left side vertices
                N = NormalA[ic] * (1.0-tau) + NormalB[ic] * tau;
                //1st triangle
                xfl::writeFloat(outStream, N.xf());
                xfl::writeFloat(outStream, N.yf());
                xfl::writeFloat(outStream, N.zf());
                Pt = PtLeft[ic]   * (1.0-tauA) + PtRight[ic]   * tauA;
                xfl::writeFloat(outStream, Pt.xf()*unit);
                xfl::writeFloat(outStream, Pt.yf()*unit);
                xfl::writeFloat(outStream, Pt.zf()*unit);
                Pt = PtLeft[ic+1] * (1.0-tauA) + PtRight[ic+1] * tauA;
                xfl::writeFloat(outStream, Pt.xf()*unit);
                xfl::writeFloat(outStream, Pt.yf()*unit);
                xfl::writeFloat(outStream, Pt.zf()*unit);
                Pt = PtLeft[ic]   * (1.0-tauB) + PtRight[ic]   * tauB;
                xfl::writeFloat(outStream, Pt.xf()*unit);
                xfl::writeFloat(outStream, Pt.yf()*unit);
                xfl::writeFloat(outStream, Pt.zf()*unit);

                memcpy(buffer, &zero, sizeof(short));
                outStream.writeRawData(buffer, 2);


                //2nd triangle
                xfl::writeFloat(outStream, N.xf());
                xfl::writeFloat(outStream, N.yf());
                xfl::writeFloat(outStream, N.zf());
                Pt = PtLeft[ic+1] * (1.0-tauA) + PtRight[ic+1] * tauA;
                xfl::writeFloat(outStream, Pt.xf()*unit);
                xfl::writeFloat(outStream, Pt.yf()*unit);
                xfl::writeFloat(outStream, Pt.zf()*unit);
                Pt = PtLeft[ic+1] * (1.0-tauB) + PtRight[ic+1] * tauB;
                xfl::writeFloat(outStream, Pt.xf()*unit);
                xfl::writeFloat(outStream, Pt.yf()*unit);
                xfl::writeFloat(outStream, Pt.zf()*unit);
                Pt = PtLeft[ic]   * (1.0-tauB) + PtRight[ic]   * tauB;
                xfl::writeFloat(outStream, Pt.xf()*unit);
                xfl::writeFloat(outStream, Pt.yf()*unit);
                xfl::writeFloat(outStream, Pt.zf()*unit);

                memcpy(buffer, &zero, sizeof(short));
                outStream.writeRawData(buffer, 2);

                iTriangles +=2;
            }
        }


        //bottom surface
        for(int is=0; is<SPANPANELS; is++)
        {
            surf.getSidePoints(xfl::BOTSURFACE, nullptr, PtLeft, PtRight,
                                           NormalA, NormalB, CHORDPANELS+1);

            double tauA = double(is)   / double(SPANPANELS);
            double tauB = double(is+1) / double(SPANPANELS);
            double tau = (tauA+tauB)/2.0;
            for(int ic=0; ic<CHORDPANELS; ic++)
            {
                N = NormalA[ic] * (1.0-tau) + NormalB[ic] * tau;

                //1st triangle
                xfl::writeFloat(outStream, N.xf()*unit);
                xfl::writeFloat(outStream, N.yf()*unit);
                xfl::writeFloat(outStream, N.zf()*unit);
                Pt = PtLeft[ic]   * (1.0-tauA) + PtRight[ic]   * tauA;
                xfl::writeFloat(outStream, Pt.xf()*unit);
                xfl::writeFloat(outStream, Pt.yf()*unit);
                xfl::writeFloat(outStream, Pt.zf()*unit);
                Pt = PtLeft[ic+1] * (1.0-tauA) + PtRight[ic+1] * tauA;
                xfl::writeFloat(outStream, Pt.xf()*unit);
                xfl::writeFloat(outStream, Pt.yf()*unit);
                xfl::writeFloat(outStream, Pt.zf()*unit);
                Pt = PtLeft[ic]   * (1.0-tauB) + PtRight[ic]   * tauB;
                xfl::writeFloat(outStream, Pt.xf()*unit);
                xfl::writeFloat(outStream, Pt.yf()*unit);
                xfl::writeFloat(outStream, Pt.zf()*unit);

                memcpy(buffer, &zero, sizeof(short));
                outStream.writeRawData(buffer, 2);

                //2nd triangle
                xfl::writeFloat(outStream, N.xf()*unit);
                xfl::writeFloat(outStream, N.yf()*unit);
                xfl::writeFloat(outStream, N.zf()*unit);
                Pt = PtLeft[ic+1] * (1.0-tauA) + PtRight[ic+1] * tauA;
                xfl::writeFloat(outStream, Pt.xf()*unit);
                xfl::writeFloat(outStream, Pt.yf()*unit);
                xfl::writeFloat(outStream, Pt.zf()*unit);
                Pt = PtLeft[ic+1] * (1.0-tauB) + PtRight[ic+1] * tauB;
                xfl::writeFloat(outStream, Pt.xf()*unit);
                xfl::writeFloat(outStream, Pt.yf()*unit);
                xfl::writeFloat(outStream, Pt.zf()*unit);
                Pt = PtLeft[ic]   * (1.0-tauB) + PtRight[ic]   * tauB;
                xfl::writeFloat(outStream, Pt.xf()*unit);
                xfl::writeFloat(outStream, Pt.yf()*unit);
                xfl::writeFloat(outStream, Pt.zf()*unit);

                memcpy(buffer, &zero, sizeof(short));
                outStream.writeRawData(buffer, 2);

                iTriangles +=2;
            }
        }
    }

    Q_ASSERT(iTriangles==m_Surface.count() * CHORDPANELS * SPANPANELS * 2 *2);

    //TIP PATCHES

    for (int j=0; j<m_Surface.size(); j++)
    {
        Surface const &surf = m_Surface.at(j);
        if(surf.isTipLeft())
        {
            surf.getSidePoints(xfl::TOPSURFACE, nullptr, PtLeft, PtRight,
                                           NormalA, NormalB, CHORDPANELS+1);
            surf.getSidePoints(xfl::BOTSURFACE, nullptr, PtBotLeft, PtBotRight,
                                           NormalA, NormalB, CHORDPANELS+1);

            N = surf.m_Normal;
            N.rotateX(90.0);

            //L.E. triangle
            xfl::writeFloat(outStream, N.xf());
            xfl::writeFloat(outStream, N.yf());
            xfl::writeFloat(outStream, N.zf());
            xfl::writeFloat(outStream, PtBotLeft[0].xf()*unit);
            xfl::writeFloat(outStream, PtBotLeft[0].yf()*unit);
            xfl::writeFloat(outStream, PtBotLeft[0].zf()*unit);
            xfl::writeFloat(outStream, PtLeft[1].xf()*unit);
            xfl::writeFloat(outStream, PtLeft[1].yf()*unit);
            xfl::writeFloat(outStream, PtLeft[1].zf()*unit);
            xfl::writeFloat(outStream, PtBotLeft[1].xf()*unit);
            xfl::writeFloat(outStream, PtBotLeft[1].yf()*unit);
            xfl::writeFloat(outStream, PtBotLeft[1].zf()*unit);
            memcpy(buffer, &zero, sizeof(short));
            outStream.writeRawData(buffer, 2);

            iTriangles +=1;

            for(int ic=1; ic<CHORDPANELS-1; ic++)
            {
                //1st triangle
                exportSTLBinaryTriangle(outStream, N, PtBotLeft[ic], PtLeft[ic], PtLeft[ic+1], unit);

                //2nd triangle
                exportSTLBinaryTriangle(outStream, N, PtBotLeft[ic], PtLeft[ic+1], PtBotLeft[ic+1], unit);

                iTriangles +=2;
            }

            //T.E. triangle
            int ic = CHORDPANELS-1;
            exportSTLBinaryTriangle(outStream, N, PtBotLeft[ic], PtLeft[ic+1], PtBotLeft[ic+1], unit);

            iTriangles +=1;
        }

        if(surf.isTipRight())
        {
            surf.getSidePoints(xfl::TOPSURFACE, nullptr, PtLeft, PtRight, NormalA, NormalB, CHORDPANELS+1);
            surf.getSidePoints(xfl::BOTSURFACE, nullptr, PtBotLeft, PtBotRight, NormalA, NormalB, CHORDPANELS+1);

            N = surf.m_Normal;
            N.rotateX(-90.0);

            //L.E. triangle
            exportSTLBinaryTriangle(outStream, N, PtBotRight[0], PtRight[1], PtBotRight[1], unit);
            iTriangles +=1;

            for(int ic=1; ic<CHORDPANELS-1; ic++)
            {
                //1st triangle
                exportSTLBinaryTriangle(outStream, N, PtBotRight[ic], PtRight[ic], PtRight[ic+1], unit);

                //2nd triangle
                exportSTLBinaryTriangle(outStream, N, PtBotRight[ic], PtRight[ic+1], PtBotRight[ic+1], unit);
                iTriangles +=2;
            }

            //T.E. triangle
            int ic = CHORDPANELS-1;
            exportSTLBinaryTriangle(outStream, N, PtBotRight[ic], PtRight[ic], PtBotRight[ic+1], unit);

            iTriangles +=1;
        }
    }

    qDebug() << "Written " << nTriangles << " faces";
    Q_ASSERT(iTriangles==nTriangles);

}

/**
 * Write the facet data to a file in STL Format.
 * bool binaryOut - whether the function stores binary or text data?
 * &outStreamData if binaryOut then required for output (else is ignored)
 * &outStreamText if not binaryOut then required for output (else is ignored)
 * reverse - flips the order of the 2nd and 3rd points - since points are clockwise
 *     looking at the outer face this flips the face over
 */
void Wing::exportSTLTriangle3dPrintable(QDataStream &outStreamData, QTextStream &outStreamText, bool binaryOut,
                                        Vector3d Pt0, Vector3d Pt1, Vector3d Pt2, Vector3d N, Vector3d offset, float unit, bool reverse)
{
    N.normalize();
    if (reverse)
        N.reverse();

    if (binaryOut) {
        short zero = 0;
        char buffer[12];

        xfl::writeFloat(outStreamData, N.xf());
        xfl::writeFloat(outStreamData, N.yf());
        xfl::writeFloat(outStreamData, N.zf());
        xfl::writeFloat(outStreamData, Pt0.xf()*unit);
        xfl::writeFloat(outStreamData, Pt0.yf()*unit);
        xfl::writeFloat(outStreamData, Pt0.zf()*unit);
        if (! reverse) {
            xfl::writeFloat(outStreamData, Pt1.xf()*unit);
            xfl::writeFloat(outStreamData, Pt1.yf()*unit);
            xfl::writeFloat(outStreamData, Pt1.zf()*unit);
            xfl::writeFloat(outStreamData, Pt2.xf()*unit);
            xfl::writeFloat(outStreamData, Pt2.yf()*unit);
            xfl::writeFloat(outStreamData, Pt2.zf()*unit);
        } else {
            xfl::writeFloat(outStreamData, Pt2.xf()*unit);
            xfl::writeFloat(outStreamData, Pt2.yf()*unit);
            xfl::writeFloat(outStreamData, Pt2.zf()*unit);
            xfl::writeFloat(outStreamData, Pt1.xf()*unit);
            xfl::writeFloat(outStreamData, Pt1.yf()*unit);
            xfl::writeFloat(outStreamData, Pt1.zf()*unit);
        }
        memcpy(buffer, &zero, sizeof(short));
        outStreamData.writeRawData(buffer, 2);

    } else {
        //stream the triangle
        outStreamText << QString::asprintf("  facet normal %13.7f  %13.7f  %13.7f\n",  N.x, N.y, N.z);
        outStreamText << "    outer loop\n";
        outStreamText << QString::asprintf("      vertex %13.7f  %13.7f  %13.7f\n",  Pt0.x+offset.x, Pt0.y+offset.y, Pt0.z+offset.z);
        if (! reverse) {
            outStreamText << QString::asprintf("      vertex %13.7f  %13.7f  %13.7f\n",  Pt1.x+offset.x, Pt1.y+offset.y, Pt1.z+offset.z);
            outStreamText << QString::asprintf("      vertex %13.7f  %13.7f  %13.7f\n",  Pt2.x+offset.x, Pt2.y+offset.y, Pt2.z+offset.z);
        } else {
            outStreamText << QString::asprintf("      vertex %13.7f  %13.7f  %13.7f\n",  Pt2.x+offset.x, Pt2.y+offset.y, Pt2.z+offset.z);
            outStreamText << QString::asprintf("      vertex %13.7f  %13.7f  %13.7f\n",  Pt1.x+offset.x, Pt1.y+offset.y, Pt1.z+offset.z);
        }
        outStreamText << "    endloop\n  endfacet\n";
    }
}

/**
 * Generate the facet data for export to a text file in STL Format.
 * @param out the instance of the QTextStream to which the output will be directed
 */
void Wing::exportSTLTextTriangle(QTextStream &outStream, Vector3d N, Vector3d Pt0, Vector3d Pt1, Vector3d Pt2)
{
    Vector3d Pt;
    //stream the triangle
    outStream << QString::asprintf("  facet normal %13.7f  %13.7f  %13.7f\n",  N.x, N.y, N.z);
    outStream << "    outer loop\n";
    outStream << QString::asprintf("      vertex %13.7f  %13.7f  %13.7f\n",  Pt0.x, Pt0.y, Pt0.z);
    outStream << QString::asprintf("      vertex %13.7f  %13.7f  %13.7f\n",  Pt1.x, Pt1.y, Pt1.z);
    outStream << QString::asprintf("      vertex %13.7f  %13.7f  %13.7f\n",  Pt2.x, Pt2.y, Pt2.z);
    outStream << "    endloop\n  endfacet\n";

}

/**
 * Export the wing geometry to a text file in STL Format.
 * @param out the instance of the QTextStream to which the output will be directed
 */
void Wing::exportSTLText(QTextStream &outStream, int CHORDPANELS, int SPANPANELS)
{
    /***
     * solid name
     *
     *       facet normal ni nj nk
     *         outer loop
     *           vertex v1x v1y v1z
     *           vertex v2x v2y v2z
     *           vertex v3x v3y v3z
     *      endloop
     *   endfacet
     *
     * endsolid name
    */
    QString name = m_Name;
    name.replace(" ","");
    QString strong = "solid " + name + "\n";
    outStream << strong;

    Vector3d N, Pt, Pt0, Pt1, Pt2;
    QVector<Vector3d> NormalA(CHORDPANELS+1);
    QVector<Vector3d> NormalB(CHORDPANELS+1);
    QVector<Vector3d> PtLeft(CHORDPANELS+1);
    QVector<Vector3d> PtRight(CHORDPANELS+1);
    QVector<Vector3d> PtBotLeft(CHORDPANELS+1);
    QVector<Vector3d> PtBotRight(CHORDPANELS+1);

    NormalA.fill(Vector3d());
    NormalB.fill(Vector3d());

    //Number of triangles
    // nSurfaces
    //   *CHORDPANELS*SPANPANELS   quads
    //   *2                        2 triangles/quad
    //   *2                        top and bottom surfaces
    // 2 Tip patches
    //   1 LE triangle
    //   1 TE triangle
    //   CHORDPANELS-1  quads
    //   *2 triangles/quad

    int nTriangles = m_Surface.count() * CHORDPANELS * SPANPANELS * 2 *2
                      + 2* ((CHORDPANELS-2)*2 + 2);
    N.set(0.0, 0.0, 0.0);
    int iTriangles = 0;

    for (int j=0; j<m_Surface.size(); j++)
    {
        Surface const &surf = m_Surface.at(j);
        for(int is=0; is<SPANPANELS; is++)
        {
            //top surface
            surf.getSidePoints(xfl::TOPSURFACE, nullptr, PtLeft, PtRight, NormalA, NormalB, CHORDPANELS+1);

            double tauA = double(is)   / double(SPANPANELS);
            double tauB = double(is+1) / double(SPANPANELS);
            double tau = (tauA+tauB)/2.0;
            for(int ic=0; ic<CHORDPANELS; ic++)
            {
                N = (NormalA[ic]+NormalA[ic+1]) * (1.0-tau) + (NormalB[ic]+NormalB[ic+1]) * tau;
                N.normalize();

                //1st triangle
                Pt0 = PtLeft[ic]   * (1.0-tauA) + PtRight[ic]   * tauA;
                Pt1 = PtLeft[ic+1] * (1.0-tauA) + PtRight[ic+1] * tauA;
                Pt2 = PtLeft[ic]   * (1.0-tauB) + PtRight[ic]   * tauB;
                exportSTLTextTriangle(outStream, N, Pt0, Pt1, Pt2);


                //2nd triangle
                Pt0 = PtLeft[ic]   * (1.0-tauB) + PtRight[ic]   * tauB;
                Pt1 = PtLeft[ic+1] * (1.0-tauA) + PtRight[ic+1] * tauA;
                Pt2 = PtLeft[ic+1] * (1.0-tauB) + PtRight[ic+1] * tauB;
                exportSTLTextTriangle(outStream, N, Pt0, Pt1, Pt2);
                iTriangles +=2;
            }

            //bottom surface
            surf.getSidePoints(xfl::BOTSURFACE, nullptr, PtLeft, PtRight,
                                           NormalA, NormalB, CHORDPANELS+1);

            for(int ic=0; ic<CHORDPANELS; ic++)
            {
                //left side vertices
                N = (NormalA[ic]+NormalA[ic+1]) * (1.0-tau) + (NormalB[ic]+NormalB[ic+1]) * tau;
                N.normalize();

                //1st triangle
                Pt0 = PtLeft[ic]   * (1.0-tauA) + PtRight[ic]   * tauA;
                Pt1 = PtLeft[ic+1] * (1.0-tauA) + PtRight[ic+1] * tauA;
                Pt2 = PtLeft[ic]   * (1.0-tauB) + PtRight[ic]   * tauB;
                exportSTLTextTriangle(outStream, N, Pt0, Pt1, Pt2);

                //2nd triangle
                Pt0 = PtLeft[ic+1] * (1.0-tauA) + PtRight[ic+1] * tauA;
                Pt1 = PtLeft[ic+1] * (1.0-tauB) + PtRight[ic+1] * tauB;
                Pt2 = PtLeft[ic]   * (1.0-tauB) + PtRight[ic]   * tauB;
                exportSTLTextTriangle(outStream, N, Pt0, Pt1, Pt2);
                iTriangles +=2;
            }
        }
    }

    Q_ASSERT(iTriangles==m_Surface.count() * CHORDPANELS * SPANPANELS * 2 *2);

    //TIP PATCHES

    for (int j=0; j<m_Surface.size(); j++)
    {
        Surface const &surf = m_Surface.at(j);
        if(surf.isTipLeft())
        {
            surf.getSidePoints(xfl::TOPSURFACE, nullptr, PtLeft,    PtRight,    NormalA, NormalB, CHORDPANELS+1);
            surf.getSidePoints(xfl::BOTSURFACE, nullptr, PtBotLeft, PtBotRight, NormalA, NormalB, CHORDPANELS+1);

            N = surf.m_Normal;
            N.rotateX(90.0);

            //L.E. triangle
            exportSTLTextTriangle(outStream, N, PtBotLeft[0], PtLeft[1], PtBotLeft[1]);
            iTriangles +=1;

            for(int ic=1; ic<CHORDPANELS-1; ic++)
            {
                //1st triangle
                exportSTLTextTriangle(outStream, N, PtBotLeft[ic], PtLeft[ic], PtBotLeft[ic+1]);
                //2nd triangle
                exportSTLTextTriangle(outStream, N, PtBotLeft[ic+1], PtLeft[ic], PtLeft[ic+1]);
                iTriangles +=2;
            }
            //T.E. triangle
            int ic = CHORDPANELS-1;
            exportSTLTextTriangle(outStream, N, PtBotLeft[ic], PtLeft[ic], PtBotLeft[ic+1]);

            iTriangles +=1;
        }

        if(surf.isTipRight())
        {
            surf.getSidePoints(xfl::TOPSURFACE, nullptr, PtLeft,    PtRight,    NormalA, NormalB, CHORDPANELS+1);
            surf.getSidePoints(xfl::BOTSURFACE, nullptr, PtBotLeft, PtBotRight, NormalA, NormalB, CHORDPANELS+1);

            N = surf.m_Normal;
            N.rotateX(-90.0);

            //L.E. triangle
            exportSTLTextTriangle(outStream, N, PtBotRight[0], PtLeft[1], PtBotRight[1]);

            iTriangles +=1;

            for(int ic=1; ic<CHORDPANELS-1; ic++)
            {
                //1st triangle
                exportSTLTextTriangle(outStream, N, PtBotRight[ic], PtRight[ic], PtBotRight[ic+1]);
                //2nd triangle
                exportSTLTextTriangle(outStream, N, PtBotRight[ic+1], PtRight[ic], PtRight[ic+1]);
                iTriangles +=2;
            }
            //T.E. triangle
            int ic = CHORDPANELS-1;
            exportSTLTextTriangle(outStream, N, PtBotRight[ic], PtRight[ic], PtBotRight[ic+1]);
            iTriangles +=1;
        }
    }

    Q_ASSERT(iTriangles==nTriangles);

    strong = "endsolid " + name + "\n";
    outStream << strong;
}



Vector3d Wing::foilXZIntersection(Vector3d A, Vector3d B, Vector3d C, Vector3d D)
{
    // Line AB represented as a1x + b1z = c1
    double a1 = B.z - A.z;
    double b1 = A.x - B.x;
    double c1 = a1*(A.x) + b1*(A.z);

    // Line CD represented as a2x + b2z = c2
    double a2 = D.z - C.z;
    double b2 = C.x - D.x;
    double c2 = a2*(C.x)+ b2*(C.z);

    double determinant = a1*b2 - a2*b1;

    if (determinant == 0)
    {
        // The lines are parallel. This is simplified
        // by returning a pair of FLT_MAX
        return {FLT_MAX, FLT_MAX, FLT_MAX};
    }
    else
    {
        double x = (b2*c1 - b1*c2)/determinant;
        double z = (a1*c2 - a2*c1)/determinant;
        return {x, 0, z};
    }
}

// foils are composed of vertices (in xflr5 generally called points) with a normal vector
// xflr5 widely uses a vector3d class, and uses two of these vectors for the originating vertex location
//   and it's associated normal.
// these seem to be named left/right for the points  and A/B for the respective normals and are generally separated into top/bottom
// these are then stored in vectors (the c array version) running from trailing edge to leading edge
// TODO needs checking whether top is reversed (as surface.h implies)
void Wing::generateSecondSkinFoilPoints(QVector<Vector3d> &PtPrimaryTop, QVector<Vector3d> &NormalPrimaryTop,
                                        QVector<Vector3d> &PtPrimaryBot, QVector<Vector3d> &NormalPrimaryBot,
                                        QVector<Vector3d> &PtSecondTop, QVector<Vector3d> &NormalSecondTop,
                                        QVector<Vector3d> &PtSecondBot, QVector<Vector3d> &NormalSecondBot,
                                        QVector<double> &skinThicknessTop, QVector<double>  &skinThicknessBot,
                                        int outputStyle)
{

    qDebug() << "generateSecondSkinFoilPoints\n";

    qDebug() << NormalPrimaryTop.size() << "\n";
    for(int ic=0; ic<NormalPrimaryTop.size(); ic++)
    {
        // Get the initial point locations for the secondary surface
        // correct the normals for both skins
        //qDebug() << ic;
        //top
        // The normal for the secondary point will point in the opposite direction to it's primary counterpart
        NormalSecondTop[ic] = NormalPrimaryTop[ic];

        if (outputStyle == PRINTABLE)
            NormalSecondTop[ic].reverse();    // if we're printing a wing then this is the inner skin, so the normal should point inwards

        // The location of the secondary point will be skinThickness * secondary normal from the primary point
        PtSecondTop[ic] = PtPrimaryTop[ic] + NormalSecondTop[ic] * skinThicknessTop[ic];

        // bot
        // The normal for the secondary point will point in the opposite direction to it's primary counterpart
        NormalSecondBot[ic] = NormalPrimaryBot[ic];

        if (outputStyle == PRINTABLE)
            NormalSecondBot[ic].reverse();    // if we're printing a wing then this is the inner skin, so the normal should point inwards

        // The location of the secondary point will be skinThickness * secondary normal from the primary point
        PtSecondBot[ic] = PtPrimaryBot[ic] + NormalSecondBot[ic] * skinThicknessBot[ic];

//        qDebug() << "Top1->2 {" << PtPrimaryTop[ic].x << "," << PtPrimaryTop[ic].y << "," << PtPrimaryTop[ic].z
//               << "}\n        {" << PtSecondTop[ic].x << "," << PtSecondTop[ic].y << "," << PtSecondTop[ic].z << "}";
//        qDebug() << "Bot1->2 {" << PtPrimaryBot[ic].x << "," << PtPrimaryBot[ic].y << "," << PtPrimaryBot[ic].z
//               << "}\n        {" << PtSecondBot[ic].x << "," << PtSecondBot[ic].y << "," << PtSecondBot[ic].z << "}";

    }

    // Reorder the points where needed to ensure that any points which had switched places in the X axis (ie chordwise) are back
    // in order, so when they are stitched to the primary points the edges won't cross / faces get reversed
    std::sort(PtSecondTop.begin(), PtSecondTop.end(), sortSecondSkinFoilPointsTop);
    std::sort(PtSecondBot.begin(), PtSecondBot.end(), sortSecondSkinFoilPointsBot);

    if (outputStyle == PRINTABLE) {
        // For the secondary surface if the z value for the bottom face is above that of the top face then despite the angulation of the faces
        // the primary points are closer than the skin thickness. These points should be removed to avoid slicing errors due to the wing being non manifold


        // move the LE points of the inner skin so they are at least skinThickness away from the outer skin
        for (int i = 0; i < PtPrimaryTop.size(); i++) {
            if (PtSecondTop[i].x < PtSecondTop[i].x - skinThicknessTop[0]) {
                PtSecondTop[i].x = PtSecondTop[i].x + skinThicknessTop[0];
                PtSecondBot[i].x = PtSecondBot[i].x - skinThicknessTop[0];
            } else {
                break;
            }
        }


        // move the TE points of the inner skin so they are at least skinThickness away from the outer skin
        // TODO: This needs to be fixed to adjust the x location, since currently it will result in an overly thick skin at the trailing edge
        int skinEnded = 0;
        for (int i = PtPrimaryTop.size() - 1; i > 0; i--) {
            //find the first location where the inner surface top point is above the bottom point (ie in correct orientation)
            if (PtSecondTop[i].z > PtSecondBot[i].z) {
                skinEnded = i;
                break;
            }
        }
        if ((skinEnded > 0) & (skinEnded < PtPrimaryTop.size()-1)) {

            //Vector3d PtTEInnerXZ = foilXZIntersection(PtSecondTop[skinEnded], PtSecondBot[skinEnded+1], PtSecondBot[skinEnded], PtSecondTop[skinEnded+1]);
            // move all points within the skin to the first point beyond the skin
            for (int i = skinEnded+1; i < PtPrimaryTop.size(); i++) {
                PtSecondTop[i] = PtSecondTop[skinEnded];
                PtSecondTop[i].z = (PtSecondTop[skinEnded].z + PtSecondBot[skinEnded].z)/2.0;
                PtSecondBot[i] = PtSecondBot[skinEnded];
                PtSecondBot[i].z = (PtSecondTop[skinEnded].z + PtSecondBot[skinEnded].z)/2.0;
            }
        }

        // this will average any remaining points if the wing is particularly warped
        for (int i = skinEnded; i < PtPrimaryTop.size(); i++) {
            if (PtSecondTop[i].z < PtSecondBot[i].z) {
                PtSecondTop[i].z = (PtSecondTop[i].z + PtSecondBot[i].z ) / 2.0;
                PtSecondBot[i].z = PtSecondTop[i].z;
            }
        }

    } else {
        // intersection check isn't required if a mold is being created as the top and bottom skins can't intersect.
        // but we need to extend the trailing and leading edges out to provide room for aligning/connecting them (eg with bolts)
        // and ensure the top and bottom have flat faces to establish a firm contact


    }


    // TODO: Recalculate normals from the points either side of them
    //NormalSecondTop =
    //NormalSecondBot =

}



uint32_t Wing::stitchWingSurface(QDataStream &outStreamData, QTextStream &outStreamText, bool &binaryOut,
                         QVector<Vector3d> &PtLeft, QVector<Vector3d> &NormalA,
                         QVector<Vector3d> &PtRight, QVector<Vector3d> &NormalB,
                         double tau, double tauA, double tauB, Vector3d &offset, float& unit, bool reverse)
{
    qDebug() << "stitchWingSurface - start";
    uint32_t iTriangles = 0;
    Vector3d Pt0, Pt1, Pt2;

    //Work back chordwise (from LE to TE) and split each rectangular panel into two triangles
    for(int ic=0; ic<PtLeft.size()-2; ic++)
    {
        Vector3d N = (NormalA[ic]+NormalA[ic+1]) * (1.0-tau) + (NormalB[ic]+NormalB[ic+1]) * tau;
        N.normalize();

        //1st triangle
        Pt0 = PtLeft[ic]   * (1.0-tauA) + PtRight[ic]   * tauA;
        Pt1 = PtLeft[ic+1] * (1.0-tauA) + PtRight[ic+1] * tauA;
        Pt2 = PtLeft[ic]   * (1.0-tauB) + PtRight[ic]   * tauB;
        exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, Pt0, Pt1, Pt2, N, offset, unit, reverse);


        //2nd triangle
        Pt0 = PtLeft[ic]   * (1.0-tauB) + PtRight[ic]   * tauB;
        Pt1 = PtLeft[ic+1] * (1.0-tauA) + PtRight[ic+1] * tauA;
        Pt2 = PtLeft[ic+1] * (1.0-tauB) + PtRight[ic+1] * tauB;
        exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, Pt0, Pt1, Pt2, N, offset, unit, reverse);

        iTriangles +=2;
    }

    qDebug() << "stitchWingSurface - end";
    return iTriangles;
}


/**
 * Cap the end of a surface "tube" at an arbitrary distance between two end foils
 *
 * Write the facet data to a file in STL Format.
 * bool binaryOut - whether the function stores binary or text data?
 * &outStreamData if binaryOut then required for output (else is ignored)
 * &outStreamText if not binaryOut then required for output (else is ignored)
 *
 * Because stl files expect the vertices to be ordered clockwise when facing from the direction of the normal
 *    there is a Left and a Right version of this function.
 *
 *  Returns: the number of triangles written from within this process
 */
uint32_t Wing::stitchFoilFace(QDataStream &outStreamData, QTextStream &outStreamText, bool &binaryOut, bool bRightCap,
                         QVector<Vector3d> &PtTopLeft, QVector<Vector3d> &PtBotLeft,
                         QVector<Vector3d> &PtTopRight, QVector<Vector3d> &PtBotRight,
                         double tau, Vector3d &offset, float& unit)
{


    Vector3d N, Pt0, Pt1, Pt2;

    qDebug() << "stitchFoilFace\n";
    outStreamText << "stitchFoilFace\n";
    //for RIBS and TIP PATCHES

    int iTriangles = 0;

    // set the appropriate normal
    if (bRightCap)
        N = { 0, 1, 0};
    else
        N = { 0, -1, 0};


    //L.E. triangle
    Pt0 = PtBotLeft[0]   * (1.0-tau) + PtBotRight[0]   * tau;
    Pt1 = PtTopLeft[1] * (1.0-tau) + PtTopRight[1] * tau;
    Pt2 = PtBotLeft[1] * (1.0-tau) + PtBotRight[1] * tau;
    exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, Pt0, Pt1, Pt2, N, offset, unit, bRightCap);
    iTriangles +=1;

    for(int ic=1; ic<PtTopLeft.size()-2; ic++)
    {
        //1st triangle
        Pt0 = PtBotLeft[ic]   * (1.0-tau) + PtBotRight[ic]   * tau;
        Pt1 = PtTopLeft[ic] * (1.0-tau) + PtTopRight[ic] * tau;
        Pt2 = PtTopLeft[ic+1] * (1.0-tau) + PtTopRight[ic+1] * tau;
        exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, Pt0, Pt1, Pt2, N, offset, unit, bRightCap);


        //2nd triangle
        Pt0 = PtBotLeft[ic]   * (1.0-tau) + PtBotRight[ic]   * tau;
        Pt1 = PtTopLeft[ic+1] * (1.0-tau) + PtTopRight[ic+1] * tau;
        Pt2 = PtBotLeft[ic+1] * (1.0-tau) + PtBotRight[ic+1] * tau;
        exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, Pt0, Pt1, Pt2, N, offset, unit, bRightCap);

        iTriangles +=2;
    }

    //T.E. triangle
    int ic = PtTopLeft.size()-2;
    Pt0 = PtBotLeft[ic]   * (1.0-tau) + PtBotRight[ic]   * tau;
    Pt1 = PtTopLeft[ic] * (1.0-tau) + PtTopRight[ic] * tau;
    Pt2 = PtBotLeft[ic+1] * (1.0-tau) + PtBotRight[ic+1] * tau;
    exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, Pt0, Pt1, Pt2, N, offset, unit, bRightCap);

    iTriangles +=1;

    qDebug() << "iTriangles: " << iTriangles << "\n";
    outStreamText << "end stitchTopToBottomRight\n";
    return iTriangles;

}

/**
 * Cap the end of a surface "tube" at an arbitrary distance between two end foils
 *
 * Write the facet data to a file in STL Format.
 * bool binaryOut - whether the function stores binary or text data?
 * &outStreamData if binaryOut then required for output (else is ignored)
 * &outStreamText if not binaryOut then required for output (else is ignored)
 *
 * Because stl files expect the vertices to be ordered clockwise when facing from the direction of the normal
 *    there is a Left and a Right version of this function.
 *
 *  Returns: the number of triangles written from within this process
 */
uint32_t Wing::stitchSkinEdge(QDataStream &outStreamData, QTextStream &outStreamText, bool &binaryOut, bool bRightCap, int outputStyle,
                         QVector<Vector3d> &PtPrimaryTopLeft, QVector<Vector3d> &PtPrimaryBotLeft,
                         QVector<Vector3d> &PtPrimaryTopRight, QVector<Vector3d> &PtPrimaryBotRight,
                         QVector<Vector3d> &PtSecondTopLeft, QVector<Vector3d> &PtSecondBotLeft,
                         QVector<Vector3d> &PtSecondTopRight, QVector<Vector3d> &PtSecondBotRight,
                         double tau, Vector3d &offset, float& unit)
{


    Vector3d N, Pt0, Pt1, Pt2;

    qDebug() << "stitchFoilFace\n";
    outStreamText << "stitchFoilFace\n";
    //for RIBS and TIP PATCHES

    int iTriangles = 0;

    // set the appropriate normal
    if (bRightCap)
        N = { 0, 1, 0};
    else
        N = { 0, -1, 0};


    for(int ic=0; ic<PtPrimaryTopLeft.size()-1; ic++)
    {
        //1st triangle (joining top surfaces)
        Pt0 = PtSecondTopLeft[ic]   * (1.0-tau) + PtSecondTopRight[ic]   * tau;
        Pt1 = PtPrimaryTopLeft[ic] * (1.0-tau) + PtPrimaryTopRight[ic] * tau;
        Pt2 = PtPrimaryTopLeft[ic+1] * (1.0-tau) + PtPrimaryTopRight[ic+1] * tau;
        exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, Pt0, Pt1, Pt2, N, offset, unit, bRightCap);

        //2nd triangle (joining top surfaces)
        Pt0 = PtSecondTopLeft[ic]   * (1.0-tau) + PtSecondTopRight[ic]   * tau;
        Pt1 = PtPrimaryTopLeft[ic+1] * (1.0-tau) + PtPrimaryTopRight[ic+1] * tau;
        Pt2 = PtSecondTopLeft[ic+1] * (1.0-tau) + PtSecondTopRight[ic+1] * tau;
        exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, Pt0, Pt1, Pt2, N, offset, unit, bRightCap);

        //1st triangle (joining bottom surfaces)
        Pt0 = PtPrimaryBotLeft[ic]   * (1.0-tau) + PtPrimaryBotRight[ic]   * tau;
        Pt1 = PtSecondBotLeft[ic] * (1.0-tau) + PtSecondBotRight[ic] * tau;
        Pt2 = PtSecondBotLeft[ic+1] * (1.0-tau) + PtSecondBotRight[ic+1] * tau;
        exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, Pt0, Pt1, Pt2, N, offset, unit, bRightCap);


        //2nd triangle (joining bottom surfaces)
        Pt0 = PtPrimaryBotLeft[ic]   * (1.0-tau) + PtPrimaryBotRight[ic]   * tau;
        Pt1 = PtSecondBotLeft[ic+1] * (1.0-tau) + PtSecondBotRight[ic+1] * tau;
        Pt2 = PtPrimaryBotLeft[ic+1] * (1.0-tau) + PtPrimaryBotRight[ic+1] * tau;
        exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, Pt0, Pt1, Pt2, N, offset, unit, bRightCap);

        iTriangles +=2;
    }

    qDebug() << "iTriangles: " << iTriangles << "\n";
    outStreamText << "end stitchTopToBottomRight\n";
    return iTriangles;

}


// generate a bracing cylinder through the rib (ie generally oriented along the line of the wing - in the y direction)
// if this is located with p0.y and p1.y located to run internally so the ends are directed "outward" it will create a cutout
// otherwise it will form a solid spar
// alignToY will cause the endface to be locked to the y plane (to ensure it is flush with rib faces)
uint32_t Wing::stitchSpar(QDataStream &outStreamData, QTextStream &outStreamText, bool &binaryOut,
          int pointsAroundRim, double radius, Vector3d pLeft, Vector3d pRight,
          int type, Vector3d &offset, float& unit)
{
    //TODO: Theoretically this should create an ovoid when the spar doesn't go directly parallel to the y-axis
    // but it's not that sophisticated yet

    qDebug() << "stitchSpar";
    qDebug() << "pLeft { " << pLeft.x << "," << pLeft.y << ","  << pLeft.z << " }" ;
    qDebug() << "pRight { " << pRight.x << "," << pRight.y << ","  << pRight.z << " }" ;


    uint32_t iTriangles = 0;

    constexpr double M_PI = 3.14159265358979323846;
    constexpr double M_2PI = M_PI * 2.0;
    constexpr double M_PI14 = M_PI * 0.25;
    constexpr double M_PI34 = M_PI * 0.75;
    constexpr double M_PI54 = M_PI * 1.25;
    constexpr double M_PI74 = M_PI * 1.75;


    //If its a cutout then all the points on the circle will be bound to a flange
    // which provides 4 standard points at each end for other mesh parts to join to
    vector<Vector3d> flangeL = {pLeft, pLeft, pLeft, pLeft};
    flangeL[0].x += radius * 1.5;
    flangeL[1].x -= radius * 1.5;
    flangeL[2].z += radius * 1.5;
    flangeL[3].z -= radius * 1.5;
    vector<Vector3d> flangeR = {pRight, pRight, pRight, pRight};
    flangeR[0].x += radius * 1.5;
    flangeR[1].x -= radius * 1.5;
    flangeR[2].z += radius * 1.5;
    flangeR[3].z -= radius * 1.5;


    // set the appropriate normal
    Vector3d NA = { 0, 1, 0};
    Vector3d NB = { 0, -1, 0};


    // step around the rim generating 2 left and 2 right points and writing the appropriate faces for them.
    for (int i = 0; i < pointsAroundRim; i++)
    {
        // generate vertices for four points on the cylinder (
        double rl0 = (double)i / (double)pointsAroundRim * M_2PI;  // current number of radians around the circle
        Vector3d l0 = {std::cos(rl0) * radius + pLeft.x, pLeft.y, std::sin(rl0) * radius + pLeft.z};

        double rl1 = ((double)i + 1.0f) / (double)pointsAroundRim * M_2PI;  // current number of radians around the circle
        Vector3d l1 = {std::cos(rl1) * radius + pLeft.x, pLeft.y, std::sin(rl1) * radius + pLeft.z};

        double rr0 = ((double)i + 1.0f) / (double)pointsAroundRim * M_2PI;  // current number of radians around the circle
        Vector3d r0 = {std::cos(rr0) * radius + pRight.x, pRight.y, std::sin(rr0) * radius + pRight.z};

        double rr1 = ((double)i + 1.0f) / (double)pointsAroundRim * M_2PI;  // current number of radians around the circle
        Vector3d r1 = {std::cos(rr1) * radius + pRight.x, pRight.y, std::sin(rr1) * radius + pRight.z};


        if (type == SPARCUTOUT) {
            // join the cylinder rim points to the appropriate flange anchor point
            if ((rl0 <= M_PI14) | (rl0 > M_PI74)) {
                exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, flangeL[0], l1, l0, NA, offset, unit);
                exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, flangeR[0], r0, r1, NB, offset, unit);
            } else if ((rl0 > M_PI14) | (rl0 <= M_PI34)) {
                exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, flangeL[3], l1, l0, NA, offset, unit);
                exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, flangeR[3], r0, r1, NB, offset, unit);
            } else if ((rl0 > M_PI34) | (rl0 <= M_PI54)) {
                exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, flangeL[1], l1, l0, NA, offset, unit);
                exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, flangeR[1], r0, r1, NB, offset, unit);
            } else if ((rl0 > M_PI54) | (rl0 <= M_PI74)) {
                exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, flangeL[2], l1, l0, NA, offset, unit);
                exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, flangeR[2], r0, r1, NB, offset, unit);
            }

            // create the faces along the length of the cylinder
            exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, l0, l1, r0, NA, offset, unit, true);
            exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, l1, r1, r0, NB, offset, unit, true);

        } else {
            // join the cylinder rim points to the appropriate centre point
            exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, pLeft, l1, l0, NA, offset, unit);
            exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, pRight, r0, r1, NB, offset, unit);

            // create the faces along the length of the cylinder
            exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, l0, l1, r0, NA, offset, unit);
            exportSTLTriangle3dPrintable(outStreamData, outStreamText, binaryOut, l1, r1, r0, NB, offset, unit);

        }


        iTriangles += 4;
    }

    return iTriangles;
}


/**
 * Export the wing geometry to a file in STL Format (either binary or text).
 * This is a 3d printable wing - ie with a skin on the inside
 * The version can also generate a mold - ie with the skin on the outside and no internal features
 * @param out the instance of the QTextStream to which the output will be directed
 * outputStyle:
 *   0 - standard 3d viewable mesh (not coded as already exists in xflr5)
 *   1 - generate a 3d printable wing with ribs and a skin
 *   2 - generate just the ribs
 *   3 - generate a mold (ie with the skin on the outside and no internal features) for casting
 *
 *   returns then number of faces saved to the stl
 */
uint32_t Wing::exportSTL3dPrintable(QDataStream &outStreamData, QTextStream &outStreamText, bool binaryOut,
                                int CHORDPANELS, int SPANPANELS,
                                int outputStyle, float unit)
{


    if (binaryOut) {
        //Use STL Binary Format Output
        /***
         *  UINT8[80] – Header
         *  UINT32 – Number of triangles
         *
         *  foreach triangle
         *     REAL32[3] – Normal vector
         *     REAL32[3] – Vertex 1
         *     REAL32[3] – Vertex 2
         *     REAL32[3] – Vertex 3
         *     UINT16 – Attribute byte count
         *  end
        */

        //    80 character header, avoid word "solid" (NB appears to be one character short as requires space for the trailing \0)
        //                0123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789
        QString strong = "binary STL file                                                                ";
        xfl::writeCString(outStreamData, strong);

        // TODO: Number of triangles is unknown at this point so
        // we will need to seek back to this location and overwrite it after all the triangles have been counted
        // this is done in miarex.cpp after the function returns
        char buffer[4] = {'\0','\0','\0','\0'};
        outStreamData.writeRawData(buffer, sizeof(uint32_t));


    } else {

        //Use STL Text Format Output
        /***
         * solid name
         *
         *       facet normal ni nj nk
         *         outer loop
         *           vertex v1x v1y v1z
         *           vertex v2x v2y v2z
         *           vertex v3x v3y v3z
         *      endloop
         *   endfacet
         *
         * endsolid name
        */
        QString name = m_Name;
        name.replace(" ","");
        outStreamText << "solid " + name + "\n";
    }




    Vector3d N, Pt, Pt0, Pt1, Pt2, offset;

    QVector<Vector3d> NormalPrimaryTopA(CHORDPANELS+1);       // Holds the panel normals for the current chord
    QVector<Vector3d> NormalPrimaryTopB(CHORDPANELS+1);       // Weirdly the points seem to be called Left and Right
    QVector<Vector3d> NormalPrimaryBotA(CHORDPANELS+1);       // But the respective normals seem to be called A and B if xflt
    QVector<Vector3d> NormalPrimaryBotB(CHORDPANELS+1);       // for consistency this convention has been retained in this function
    QVector<Vector3d> PtPrimaryTopLeft(CHORDPANELS+1);        // Holds the corners of points on the L side of the upper wing panels current chord
    QVector<Vector3d> PtPrimaryTopRight(CHORDPANELS+1);       // Holds the corners of points on the R side of the upper wing panels in the current chord
    QVector<Vector3d> PtPrimaryBotLeft(CHORDPANELS+1);     // Ditto - but for the wing underside
    QVector<Vector3d> PtPrimaryBotRight(CHORDPANELS+1);    // ...

    QVector<Vector3d> NormalSecondTopA(CHORDPANELS+1);       // Holds the panel normals for the current chord
    QVector<Vector3d> NormalSecondTopB(CHORDPANELS+1);
    QVector<Vector3d> NormalSecondBotA(CHORDPANELS+1);
    QVector<Vector3d> NormalSecondBotB(CHORDPANELS+1);
    QVector<Vector3d> PtSecondTopLeft(CHORDPANELS+1);        // Holds the corners of points on the L side of the upper wing panels current chord
    QVector<Vector3d> PtSecondTopRight(CHORDPANELS+1);       // Holds the corners of points on the R side of the upper wing panels in the current chord
    QVector<Vector3d> PtSecondBotLeft(CHORDPANELS+1);     // Ditto - but for the wing underside
    QVector<Vector3d> PtSecondBotRight(CHORDPANELS+1);    // ...

    NormalPrimaryTopA.fill(Vector3d());
    NormalPrimaryTopB.fill(Vector3d());
    NormalPrimaryBotA.fill(Vector3d());
    NormalPrimaryBotB.fill(Vector3d());

    NormalSecondTopA.fill(Vector3d());
    NormalSecondTopB.fill(Vector3d());
    NormalSecondBotA.fill(Vector3d());
    NormalSecondBotB.fill(Vector3d());
    N.set(0.0, 0.0, 0.0);       // tmp used for storing the face normal
    offset.set(0.0, 0.0, 0.0);  // used to relocate the various sections of the print down to the baseplate
    int iTriangles = 0;

    // autoset the ribs locations if not already done
    if (ribLocations.size() == 0)
        for (int i = 0; i < 100; i++)
            ribLocations.push_back(i * ribSpacing);

    QVector<double> skinThicknessTop(CHORDPANELS+1);
    QVector<double> skinThicknessBot(CHORDPANELS+1);
    skinThicknessTop.fill(skinThickness);   // since we don't know how to set the skin thickness just make it constant
    skinThicknessBot.fill(skinThickness);   // since we don't know how to set the skin thickness just make it constant

/*
    // create a test spar. NB Spars will not be correctly modeled if they penetrate the wing surface
    spars.push_back({});

    spars[0].pL = PtPrimaryTopLeft[0];        //   {0.0, 0.0, 0.0};
    spars[0].pL.x - 5*mm;
    spars[0].pR = PtPrimaryTopRight[0];         //{1.0, 0.0, 0.0};
    spars[0].pR.x - 5*mm;

    spars[0].radius = 2.5*mm;
    spars[0].vertexCount = 25;
    spars[0].type = SPARCUTOUT;
*/



//    double chordMin = m_Surface.at();

    /*
    From wing.h:
    The panels are numbered from left tip to right tip in the span wise direction. Then in the chordwise direction:
           from T.E. to L.E in the case of VLM  (=Vortex Lattice Method)
           from lower surface TE, to leading edge, and to upper surface TE

    The data is stored in International Standard Units, i.e. meters, seconds, kg, and Newtons.
    Angular data is stored in degrees.
    */

    qDebug() << "m_Surface.size (=number of wing sides): " << m_Surface.size() << "\n";
    // m_Surface[0] = the left wing
    // m_Surface[1] = the right wing

    // because the panels are stored left to right (rather than root to tip):
    // Left wing: is=0 = tip, is=SPANPANELS = root
    // Right wing: is=0 = root, is=SPANPANELS = tip

    // we're going to start by just doing one section of the left wing (generally it can just be mirrored in the printing software anyway)
    // If the wing has multiple sections this will not work brilliantly - it will split each section and put a rib at the base of each
    // what would be better is to measure the size of each section then add the required proportion of each new section on until it was done
    // this should be possible but use of offset and tracking of the distance from the root






    for (int j=1; j<m_Surface.size(); j++)  //
    {
        //outStreamText << "Starting Surface " << j << "\n";



        Surface const &surf = m_Surface.at(j);

        bool isRHS = surf.isRightSurf();


        //Get the chordwise points from the foils at the ends of both top and bottom of the current section
        surf.getSidePoints(xfl::TOPSURFACE, nullptr, PtPrimaryTopLeft, PtPrimaryTopRight, NormalPrimaryTopA, NormalPrimaryTopB, CHORDPANELS+1);
        surf.getSidePoints(xfl::BOTSURFACE, nullptr, PtPrimaryBotLeft, PtPrimaryBotRight, NormalPrimaryBotA, NormalPrimaryBotB, CHORDPANELS+1);


        //it's critical the ribs are flat on the printbed or they won't stick
        //so set all of the x-values for the left and right foils to the average value
        //this will introduce some distortion, but if it matters could be compensated for by
        //removing any dihedral angle before running the stl export
        double avY = 0.0;
        for(int ic=0; ic<=CHORDPANELS; ic++)
        {
            avY += PtPrimaryTopLeft[ic].y;
            avY += PtPrimaryBotLeft[ic].y;
        }
        avY /= CHORDPANELS+1;
        for(int ic=0; ic<=CHORDPANELS; ic++) {
            PtPrimaryTopLeft[ic].y = avY;
            PtPrimaryBotLeft[ic].y = avY;
        }

        avY = 0.0;
        for(int ic=0; ic<=CHORDPANELS; ic++)
        {
            avY += PtPrimaryTopRight[ic].y;
            avY += PtPrimaryBotRight[ic].y;
        }
        avY /= CHORDPANELS+1;
        for(int ic=0; ic<=CHORDPANELS; ic++) {
            PtPrimaryTopRight[ic].y = avY;
            PtPrimaryBotRight[ic].y = avY;
        }


        // Generate LHS foil
        generateSecondSkinFoilPoints(PtPrimaryTopLeft, NormalPrimaryTopA, PtPrimaryBotLeft, NormalPrimaryBotA,
                                 PtSecondTopLeft, NormalSecondTopA, PtSecondBotLeft, NormalSecondBotA,
                                 skinThicknessTop, skinThicknessBot, outputStyle);
        // Generate RHS foil
        generateSecondSkinFoilPoints(PtPrimaryTopRight, NormalPrimaryTopB, PtPrimaryBotRight, NormalPrimaryBotB,
                                 PtSecondTopRight, NormalSecondTopB, PtSecondBotRight, NormalSecondBotB,
                                 skinThicknessTop, skinThicknessBot, outputStyle);

        if (outputStyle == MOLD) {
            // if we're printing a mold then the primary surface is the inner skin, so the normals should point inwards
            for(int ic=0; ic<=CHORDPANELS; ic++)
            {
                NormalPrimaryTopA[ic].reverse();
                NormalPrimaryTopB[ic].reverse();
                NormalPrimaryBotA[ic].reverse();
                NormalPrimaryBotB[ic].reverse();
            }
        }



        double wingSpan = m_PlanformSpan;
        double wingSectionLength = m_Section[j].m_Length;
        double surfaceLength = surf.m_Length;

        int spanDivisions = ((double)surf.m_Length/(double)ribSpacing + 1);
        int panelsInSpan = ((double)surf.m_Length / (double) m_PlanformSpan * (double)SPANPANELS + 1);
        int panelsPerDivision = ((double)panelsInSpan / (double)spanDivisions + 1);
        double sizePerPanel = (ribSpacing - ribThickness) / (double)panelsPerDivision;
        //is=0; is<SPANPANELS

        double sectionRootDistanceFromWingRoot = 0;
        double distanceFromSectionLeft = 0;

        double zDivisionOffset = 0;

        if (isRHS)
            sectionRootDistanceFromWingRoot=0.0f;              // TODO: this isn't appropriate for multi-surface wings, but will do as for initial testing
        else
            sectionRootDistanceFromWingRoot=wingSpan/2.0f;     // TODO: this isn't appropriate for multi-surface wings, but will do as for initial testing


        qDebug() << "Wing Dimensions: " << wingSpan << "," << wingSectionLength << "," << surfaceLength;
        qDebug() << "Divisions: " << SPANPANELS << "," << spanDivisions << "," << panelsPerDivision << "," << sizePerPanel << "," << distanceFromSectionLeft << "\n";

        // TODO: this will need to be modified to allow transition across multiple sections
        for(int sd=0; sd<spanDivisions; sd++)
        {

            double dfslAtRibRoot = distanceFromSectionLeft;
            double tauRR = dfslAtRibRoot / surf.m_Length;
            double minX = PtPrimaryTopLeft[0].x * (1.0-tauRR) + PtPrimaryTopRight[0].x * tauRR;

            // each division should be placed flat on the printer bed
            // TODO: CTO why does y require x2 multiplier - is there a bug somewhere else resulting in this fudge requirement?
            offset = { -minX, -sectionRootDistanceFromWingRoot-distanceFromSectionLeft*2.0f, zDivisionOffset };

            // calculate the z offset for the next printed part (since it keeps related calculations together rather than doing it later)
            double maxZSpan = 0;
            for(int ic=0; ic<CHORDPANELS; ic++) {
                double zTop = PtPrimaryTopLeft[ic].z * (1.0-tauRR) + PtPrimaryTopRight[ic].z * tauRR;
                double zBot = PtPrimaryBotLeft[ic].z * (1.0-tauRR) + PtPrimaryBotRight[ic].z * tauRR;
                double zSpan = zTop-zBot;
                if (maxZSpan < zSpan)
                    maxZSpan = zSpan;
            }
            zDivisionOffset += maxZSpan * partZSpacing;




            if (ribThickness > 0.0) {
                // Generate the rib

                // tau is the proportional distance along the current span
                // uses the same A/B logic as Normals (ie A is the left face, B is the right face
                double tauA = distanceFromSectionLeft / surf.m_Length;
                if (tauA > 1.0)
                    break;
                double tauC = double(distanceFromSectionLeft+ribThickness) / double(surf.m_Length);
                if (tauC > 1.0)
                    tauC = 1.0f;     // for the last division it can't be further than the full length of the span
                double tau = (tauA+tauC)/2.0;

                //rib top surface
                iTriangles += stitchWingSurface(outStreamData, outStreamText, binaryOut,
                         PtPrimaryTopLeft, NormalPrimaryTopA, PtPrimaryTopRight, NormalPrimaryTopB,
                        tau, tauA, tauC, offset, unit, false);
                //rib bottom surface
                iTriangles += stitchWingSurface(outStreamData, outStreamText, binaryOut,
                         PtPrimaryBotLeft, NormalPrimaryBotA, PtPrimaryBotRight, NormalPrimaryBotB,
                         tau, tauA, tauC, offset, unit, true);


                // now fill in the faces
                if (isRHS) {

                    // tau is the proportional distance along the current span
                    // uses the same A/B logic as Normals (ie A is the left face, B is the right face
                    double tauRibRoot = dfslAtRibRoot / surf.m_Length;
                    double tauRibInner = (dfslAtRibRoot + ribThickness) / surf.m_Length;

                    if (outputStyle == PRINTABLE)
                    {


                        // Generate spar cutouts if they penetrate this rib?
                        int sparPenetrations = 0;
                        for (auto& spar : spars) {
                            // if the spar contacts the rib it will be treated as full penetration
                            if ((spar.pL.y <= dfslAtRibRoot + ribThickness) & (spar.pR.y >= dfslAtRibRoot)) {
                                sparPenetrations++;

                                iTriangles += stitchSpar(outStreamData, outStreamText, binaryOut,
                                          spar.vertexCount, spar.radius + sparCutoutClearance, spar.pL, spar.pR,
                                          spar.type, offset, unit);

                                /*
                                double sparLengthY = spar.p0.y - spar.p1.y;
                                double tauSparA = (sectionRootDistanceFromWingRoot + dfslAtRibRoot - spar.p0.y) / sparLengthY;
                                double tauSparB = (sectionRootDistanceFromWingRoot + dfslAtRibRoot + ribThickness - spar.p0.y) / sparLengthY;

                                Vector3d p0 = {1,1,1};
                                Vector3d p1 = {1,1,1};

                                iTriangles += stitchSpar(outStreamData, outStreamText, binaryOut,
                                          sparPointsAroundRim, spar.radius + sparCutoutClearance, p0, p1,
                                          spar.cutout, offset, unit);
                                */
                            }
                        }

                        if (sparPenetrations == 0) {
                            // generate the rib face in contact with the printer buildplate
                            iTriangles += stitchFoilFace(outStreamData, outStreamText, binaryOut, isRHS,
                                           PtPrimaryTopLeft, PtPrimaryBotLeft, PtPrimaryTopRight, PtPrimaryBotRight,
                                           tauRibRoot, offset, unit);


                            // generate the rib face linking the top and bottom surfaces of the inner skin
                            if (skinThickness == 0) {
                                // If there is no skin the ribs should use the primary foil to ensure the edges go to the outside

                                iTriangles += stitchFoilFace(outStreamData, outStreamText, binaryOut, isRHS,
                                               PtPrimaryTopLeft, PtPrimaryBotLeft, PtPrimaryTopRight, PtPrimaryBotRight,
                                               tauRibInner, offset, unit);
                            } else {
                                // otherwise there should be a gap between outer and inner so the inner skin is a continuous piece
                                iTriangles += stitchFoilFace(outStreamData, outStreamText, binaryOut, isRHS,
                                               PtSecondTopLeft, PtSecondBotLeft, PtSecondTopRight, PtSecondBotRight,
                                               tauRibInner, offset, unit);
                            }
                        }
                        else
                        {

                            for (auto& spar : spars) {


                            }



                            // We'll need to stitch to the surface of the spar
                            float sparVertexAv = 0.0;
                            sparVertexAv = (2*CHORDPANELS+2) / sparPenetrations;

                            // TODO: for the moment we're just going to stitch top and bottom surfaces directly - will sort out the spar penetration later
                            iTriangles += stitchFoilFace(outStreamData, outStreamText, binaryOut, isRHS,
                                           PtPrimaryTopLeft, PtPrimaryBotLeft, PtPrimaryTopRight, PtPrimaryBotRight,
                                           tauRibRoot, offset, unit);

                            // generate the rib face linking the top and bottom surfaces of the inner skin
                            iTriangles += stitchFoilFace(outStreamData, outStreamText, binaryOut, isRHS,
                                           PtSecondTopLeft, PtSecondBotLeft, PtSecondTopRight, PtSecondBotRight,
                                           tauRibInner, offset, unit);


                        }



                    }
                    else //if (outputStyle != PRINTABLE)
                    {

                        // at each end there should be caps, then at each rib the rib is outside and is squared off so the mold sits flat on a surface
                        // there should also be a wall which goes up the outer edge - in case it needs filling with something cheap to support the mold
                        // generate a skirt outside the wing

                    }

                    distanceFromSectionLeft+=ribThickness; // (panelsPerDivision*sizePerPanel);

                }
                else    // if (!isRHS)
                {
                    //stitchTopToBottomLeft(outStreamData, outStreamText, binaryOut, PtPrimaryTopLeft, PtPrimaryBotLeft, N, offset, unit);

                    distanceFromSectionLeft-=ribThickness;

                }
            }



            if (skinThickness > 0.0) {

                // generate the skin panels (ie both top and bottom surface panels for internal and external faces)
                for(int ppd=0; ppd<panelsPerDivision; ppd++)
                {

                    // tau is the proportional distance along the current span
                    // uses the same A/B logic as Normals (ie A is the left face, B is the right face
                    double tauA = distanceFromSectionLeft / surf.m_Length;
                    if (tauA > 1.0)
                        break;
                    double tauB = double(distanceFromSectionLeft+sizePerPanel) / double(surf.m_Length);
                    if (tauB > 1.0)
                        tauB = 1.0f;     // for the last division it can't be further than the full length of the span

                    double tau = (tauA+tauB)/2.0;

                    //primary top surface
                    iTriangles += stitchWingSurface(outStreamData, outStreamText, binaryOut,
                             PtPrimaryTopLeft, NormalPrimaryTopA, PtPrimaryTopRight, NormalPrimaryTopB,
                            tau, tauA, tauB, offset, unit, false);
                    //primary bottom surface
                    iTriangles += stitchWingSurface(outStreamData, outStreamText, binaryOut,
                             PtPrimaryBotLeft, NormalPrimaryBotA, PtPrimaryBotRight, NormalPrimaryBotB,
                             tau, tauA, tauB, offset, unit, true);

                    //TODO: add strain relief at the edge attaching to the skin

                    //secondary top surface
                    iTriangles += stitchWingSurface(outStreamData, outStreamText, binaryOut,
                             PtSecondTopLeft, NormalSecondTopA, PtSecondTopRight, NormalSecondTopB,
                             tau, tauA, tauB, offset, unit, true);
                    //secondary bottom surface
                    iTriangles += stitchWingSurface(outStreamData, outStreamText, binaryOut,
                             PtSecondBotLeft, NormalSecondBotA, PtSecondBotRight, NormalSecondBotB,
                             tau, tauA, tauB, offset, unit, true);

                    distanceFromSectionLeft+=sizePerPanel;
                }

                // cap the end of the skin with faces linking the outer and inner edges of the skin
                double tauSkinCap = distanceFromSectionLeft / surf.m_Length;
                if (tauSkinCap > 1.0)
                    tauSkinCap = 1.0;
                iTriangles += stitchSkinEdge(outStreamData, outStreamText, binaryOut, isRHS, outputStyle,
                               PtPrimaryTopLeft, PtPrimaryBotLeft, PtPrimaryTopRight, PtPrimaryBotRight,
                               PtSecondTopLeft, PtSecondBotLeft, PtSecondTopRight, PtSecondBotRight,
                               tauSkinCap, offset, unit);

            }
        }
    }



    // BRACING STRUTS - A cylindrical support
    // STRUT CUTOUTS - A cylindrical hole - eg to add carbon fibre tubing for support


    // TIPCAP
    // This is half an icosagon with a radius equal to half the distance between top and bottom surfaces


    if (binaryOut) {
        // stl binary format just ends
    } else {
        QString name = m_Name;
        name.replace(" ","");
        outStreamText <<  "endsolid " + name + "\n";
    }

    qDebug() << "Written " << iTriangles << " faces";

    return iTriangles;
}



/**
 * Returns a pointer to the foil with the corresponding name or NULL if not found.
 * @param strFoilName the name of the Foil to search for in the array
 * @return a pointer to the foil with the corresponding name or NULL if not found.
 */
Foil* Wing::foil(QString const &strFoilName)
{
    if(!strFoilName.length()) return nullptr;

    for (int i=0; i<s_poaFoil->size(); i++)
    {
        Foil*pFoil = s_poaFoil->at(i);
        if (pFoil->name() == strFoilName)
        {
            return pFoil;
        }
    }

    return nullptr;
}


/**
*Interpolates a variable on the polar mesh, based on the geometrical position of a point between two sections on a wing.
*@param m_poaPolar the pointer to the array of polars.
*@param nVar the index of the variable to interpolate.
*@param pFoil0 the pointer to the left foil of the wing's section.
*@param pFoil1 the pointer to the left foil of the wing's section.
*@param Re the Reynolds number at the point's position.
*@param Cl the lift coefficient at the point's position, used as the input parameter.
*@param Tau the relative position of the point between the two foils.
*@param bOutRe true if Cl is outside the min or max Cl of the polar mesh.
*@param bError if Re is outside the min or max Reynolds number of the polar mesh.
*@return the interpolated value.
*/
double Wing::getInterpolatedVariable(int nVar, Foil *pFoil0, Foil *pFoil1, double Re, double Cl, double Tau, bool &bOutRe, bool &bError)
{
    bool IsOutRe = false;
    bool IsError  = false;
    bOutRe = false;
    bError = false;
    double Var0, Var1;
    if(!pFoil0)
    {
        Cl = 0.0;
        Var0 = 0.0;
    }
    else Var0 = getPlrPointFromCl(pFoil0, Re, Cl,nVar, IsOutRe, IsError);
    if(IsOutRe) bOutRe = true;
    if(IsError) bError = true;


    if(!pFoil1)
    {
        Cl = 0.0;
        Var1 = 0.0;
    }
    else Var1 = getPlrPointFromCl(pFoil1, Re, Cl,nVar, IsOutRe, IsError);
    if(IsOutRe) bOutRe = true;
    if(IsError) bError = true;

    if (Tau<0.0) Tau = 0.0;
    if (Tau>1.0) Tau = 1.0;

    return ((1-Tau) * Var0 + Tau * Var1);
}




/**
* Returns the value of an aero coefficient, interpolated on a polar mesh, and based on the value of the Reynolds Number and of the lift coefficient.
* Proceeds by identifiying the two polars surronding Re, then interpolating both with the value of Alpha,
* last by interpolating the requested variable between the values measured on the two polars.
*@param m_poaPolar the pointer to the array of polars.
*@param pFoil the pointer to the foil
*@param Re the Reynolds number .
*@param Cl the lift coefficient, used as the input parameter for interpolation.
*@param PlrVar the index of the variable to interpolate.
*@param bOutRe true if Cl is outside the min or max Cl of the polar mesh.
*@param bError if Re is outside the min or max Reynolds number of the polar mesh.
*@return the interpolated value.
*/
double Wing::getPlrPointFromCl(Foil *pFoil, double Re, double Cl, int PlrVar, bool &bOutRe, bool &bError)
{
    /*    Var
    0 =    m_Alpha;
    1 = m_Cl;
    2 = m_Cd;
    3 = m_Cdp;
    4 = m_Cm;
    5, 6 = m_XTr1, m_XTr2;
    7, 8 = m_HMom, m_Cpmn;
    9,10 = m_ClCd, m_Cl32Cd;
*/
    double Clmin=0, Clmax=0;
    Polar *pPolar;
    double Var1=0, Var2=0, u=0, dist=0;
    Var1 = Var2 = u = dist = 0.0;
    int pt=0;
    int n=0;

    bOutRe = false;
    bError = false;

    if(!pFoil)
    {
        bOutRe = true;
        bError = true;
        return 0.000;
    }

    n=0;
    // Are there any Type 1 polars available for this foil ?
    for (int i = 0; i<s_poaPolar->size(); i++)
    {
        pPolar = s_poaPolar->at(i);
        if((pPolar->polarType()== xfl::FIXEDSPEEDPOLAR) && (pPolar->foilName() == pFoil->name()))
        {
            n++;
            if(n>=2) break;
        }
    }

    //more than one polar - interpolate between  - tough job

    //First Find the two polars with Reynolds number surrounding wanted Re
    Polar * pPolar1 = nullptr;
    Polar * pPolar2 = nullptr;
    int nPolars = s_poaPolar->size();
    //Type 1 Polars are sorted by crescending Re Number

    //if Re is less than that of the first polar, use this one
    for (int i=0; i<nPolars; i++)
    {
        pPolar = s_poaPolar->at(i);
        if((pPolar->polarType()==xfl::FIXEDSPEEDPOLAR) && (pPolar->foilName()==pFoil->name()) && pPolar->m_Cl.size()>0)
        {
            // we have found the first type 1 polar for this foil
            if (Re < pPolar->Reynolds())
            {
                bOutRe = true;
                //interpolate Cl on this polar
                QVector<double> const &pX = pPolar->getPlrVariable(PlrVar);
                int size = pPolar->m_Cl.size();
                if(Cl < pPolar->m_Cl[0])
                {
                    return pX.front();
                }
                if(Cl > pPolar->m_Cl[size-1])
                {
                    return pX.back();
                }
                for (i=0; i<size-1; i++)
                {
                    if(pPolar->m_Cl[i] <= Cl && Cl < pPolar->m_Cl[i+1])
                    {
                        //interpolate
                        if(pPolar->m_Cl[i+1]-pPolar->m_Cl[i] < 0.00001)//do not divide by zero
                            return pX.at(i);
                        else {
                            u = (Cl - pPolar->m_Cl[i])  /(pPolar->m_Cl[i+1]-pPolar->m_Cl[i]);
                            return pX.at(i) + u * (pX.at(i+1)-pX.at(i));
                        }
                    }
                }
                break;
            }
            break;
        }
    }

    // if not Find the two polars
    for (int i=0; i< nPolars; i++)
    {
        pPolar = s_poaPolar->at(i);
        if((pPolar->polarType()== xfl::FIXEDSPEEDPOLAR) && (pPolar->foilName() == pFoil->name())  && pPolar->m_Cl.size()>0)
        {
            // we have found the first type 1 polar for this foil
            pPolar->getClLimits(Clmin, Clmax);
            if (pPolar->Reynolds() <= Re)
            {
                if(Clmin <= Cl && Cl <= Clmax)
                {
                    pPolar1 = pPolar;
                }
            }
            else
            {
                if(Clmin <= Cl && Cl <= Clmax)
                {
                    pPolar2 = pPolar;
                    break;
                }
            }
        }
    }

    if (!pPolar2)
    {
        //then Re is greater than that of any polar
        // so use last polar and interpolate Cls on this polar
        bOutRe = true;
        if(!pPolar1)
        {
            bOutRe = true;
            bError = true;
            return 0.000;
        }
        int size = pPolar1->m_Cl.size();
        if(!size)
        {
            bOutRe = true;
            bError = true;
            return 0.000;
        }

        QVector<double> const &pX = pPolar1->getPlrVariable(PlrVar);
        if(Cl < pPolar1->m_Cl[0])       return pX.front();
        if(Cl > pPolar1->m_Cl[size-1]) return pX.back();
        for (int i=0; i<size-1; i++)
        {
            if(pPolar1->m_Cl[i] <= Cl && Cl < pPolar1->m_Cl[i+1])
            {
                //interpolate
                if(pPolar1->m_Cl[i+1]-pPolar1->m_Cl[i] < 0.00001)
                {//do not divide by zero
                    return pX.at(i);
                }
                else
                {
                    u = (Cl - pPolar1->m_Cl[i])
                            /(pPolar1->m_Cl[i+1]-pPolar1->m_Cl[i]);
                    return pX.at(i) + u * (pX.at(i+1)-pX.at(i));
                }
            }
        }
        //Out in Re, out in Cl...
        return pX.back();
    }
    else
    {
        // Re is between that of polars 1 and 2
        // so interpolate Cls for each
        if(!pPolar1)
        {
            bOutRe = true;
            bError = true;
            return 0.000;
        }
        int size = pPolar1->m_Cl.size();
        if(!size)
        {
            bOutRe = true;
            bError = true;
            return 0.000;
        }

        QVector<double> const &pX = pPolar1->getPlrVariable(PlrVar);
        pPolar1->getClLimits(Clmin, Clmax);
        if(Cl < Clmin)
        {
            Var1 = pX.front();
            bOutRe = true;
        }
        else if(Cl > Clmax)
        {
            Var1 = pX.back();
            bOutRe = true;
        }
        else
        {
            //first Find the point closest to Cl=0
            pt = 0;
            dist = fabs(pPolar1->m_Cl[0]);
            for (int i=1; i<size;i++)
            {
                if (fabs(pPolar1->m_Cl[i])< dist)
                {
                    dist = fabs(pPolar1->m_Cl[i]);
                    pt = i;
                }
            }
            if(Cl<pPolar1->m_Cl[pt])
            {
                for (int i=pt; i>0; i--)
                {
                    if(Cl<= pPolar1->m_Cl[i] && Cl > pPolar1->m_Cl[i-1])
                    {
                        //interpolate
                        if(fabs(pPolar1->m_Cl[i]-pPolar1->m_Cl[i-1]) < 0.00001)
                        {
                            //do not divide by zero
                            Var1 = pX.at(i);
                            break;
                        }
                        else
                        {
                            u = (Cl - pPolar1->m_Cl[i-1])
                                    /(pPolar1->m_Cl[i]-pPolar1->m_Cl[i-1]);
                            Var1 = pX.at(i-1) + u * (pX.at(i)-pX.at(i-1));
                            break;
                        }
                    }
                }
            }
            else
            {
                for (int i=pt; i<size-1; i++)
                {
                    if(pPolar1->m_Cl[i] <=Cl && Cl < pPolar1->m_Cl[i+1])
                    {
                        //interpolate
                        if(fabs(pPolar1->m_Cl[i+1]-pPolar1->m_Cl[i]) < 0.00001){//do not divide by zero
                            Var1 = pX.at(i);
                            break;
                        }
                        else
                        {
                            u = (Cl - pPolar1->m_Cl[i])
                                    /(pPolar1->m_Cl[i+1]-pPolar1->m_Cl[i]);
                            Var1 = pX.at(i) + u * (pX.at(i+1)-pX.at(i));
                            break;
                        }
                    }
                }
            }
        }
        size = pPolar2->m_Cl.size();
        if(!size)
        {
            bOutRe = true;
            bError = true;
            return 0.000;
        }

        QVector<double> const &pX2 = pPolar2->getPlrVariable(PlrVar);
        pPolar2->getClLimits(Clmin, Clmax);

        if(Cl < Clmin)
        {
            Var2 = pX2.front();
            bOutRe = true;
        }
        else if(Cl > Clmax)
        {
            Var2 = pX2.back();
            bOutRe = true;
        }
        else
        {
            //first Find the point closest to Cl=0
            pt = 0;
            dist = qAbs(pPolar2->m_Cl[0]);
            for (int i=1; i<size;i++)
            {
                if (fabs(pPolar2->m_Cl[i])< dist)
                {
                    dist =fabs(pPolar2->m_Cl[i]);
                    pt = i;
                }
            }
            if(Cl<pPolar2->m_Cl[pt])
            {
                for (int i=pt; i>0; i--)
                {
                    if(Cl<= pPolar2->m_Cl[i] && Cl > pPolar2->m_Cl[i-1])
                    {
                        //interpolate
                        if(fabs(pPolar2->m_Cl[i]-pPolar2->m_Cl[i-1]) < 0.00001)
                        {//do not divide by zero
                            Var2 = pX2.at(i);
                            break;
                        }
                        else
                        {
                            u = (Cl - pPolar2->m_Cl[i-1])
                                    /(pPolar2->m_Cl[i]-pPolar2->m_Cl[i-1]);
                            Var2 = pX2.at(i-1) + u * (pX2.at(i)-pX2.at(i-1));
                            break;
                        }
                    }
                }
            }
            else
            {
                for (int i=pt; i<size-1; i++)
                {
                    if(pPolar2->m_Cl[i] <=Cl && Cl < pPolar2->m_Cl[i+1])
                    {
                        //interpolate
                        if(fabs(pPolar2->m_Cl[i+1]-pPolar2->m_Cl[i]) < 0.00001)
                        {
                            //do not divide by zero
                            Var2 = pX2.at(i);
                            break;
                        }
                        else
                        {
                            u = (Cl - pPolar2->m_Cl[i])
                                    /(pPolar2->m_Cl[i+1]-pPolar2->m_Cl[i]);
                            Var2 = pX2.at(i) + u * (pX2.at(i+1)-pX2.at(i));
                            break;
                        }
                    }
                }
            }
        }

        // then interpolate Variable

        double v =   (Re - pPolar1->Reynolds()) / (pPolar2->Reynolds() - pPolar1->Reynolds());
        double Var = Var1 + v * (Var2-Var1);
        return Var;
    }
}




/**
 Auxiliary integral used in LLT calculations
*/
double Wing::IntegralC2(double y1, double y2, double c1, double c2) const
{
    double res = 0.0;

    if (qAbs(y2-y1)<1.e-5) return 0.0;
    double g = (c2-c1)/(y2-y1);

    res = (c1-g*y1)*(c1-g*y1)*(y2-y1) +
            g * (c1-g*y1)      *(y2*y2-y1*y1)+
            g*g/3.0            *(y2*y2*y2-y1*y1*y1);

    return res;
}


/**
 Auxiliary integral used in LLT calculations
*/
double Wing::IntegralCy(double y1, double y2, double c1, double c2) const
{
    double res = 0.0;
    if (qAbs(y2-y1)<1.e-5) return (y1+y2)/2.0 * (c1+c2)/2.0;

    double g = (c2-c1)/(y2-y1);
    res = (c1-g*y1)/2.0 *(y2*y2 - y1*y1) + g/3.0 * (y2*y2*y2-y1*y1*y1);
    return res;
}

