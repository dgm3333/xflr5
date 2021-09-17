/****************************************************************************

    xflr5 v6
    Copyright (C) Andre Deperrois 
    GNU General Public License v3

*****************************************************************************/

#pragma once

#include <QColor>

#include <xflcore/constants.h>
#include <xflgeom/geom3d/vector3d.h>

#define NDIM 4  // dimension of system
#define GRAVITY 6.673e-11 // N.m2/kg2
#define AU 1.4959787e11    // Astronomical unit in meters
#define LIGHTSPEED 299.792e6     // m/s

class Planet
{
    public:
        Planet();

        void rk4_step(double dt, int nsteps);

        Vector3d position() const {return Vector3d(m_var[0], m_var[1], 0.0);}
        double velocity() const {return sqrt(m_var[2]*m_var[2]+m_var[3]*m_var[3]);}
        double distance() const {return sqrt(m_var[0]*m_var[0]+m_var[1]*m_var[1]);}
        double totalEnergy() const;
        double refEnergy() const {return m_RefEnergy;}

//        void setPosition(double x, double y) {f_t[0]=x; f_t[1]=y;}
        void setPosition(double x, double y) {m_var[0]=sqrt(x*x+y*y); m_var[1]=0.0;}
        void setCircularVelocity() {setCircularVelocity(s_CentralMass);}
        void setCircularVelocity(double mass)
        {
            m_var[2] = 0.0;
            double om = sqrt(GRAVITY*mass/distance()/distance()/distance());
            double d = distance();
            m_var[3] = om *d;  // m/s
        }

        void setVelocity(double vx, double vy) {m_var[2]=vx; m_var[3]=vy;}
        void setRefEnergy() {m_RefEnergy = totalEnergy();}
        void setOrbit(double a, double e, double i, double O, double o);

        double period() const;// {return 2.0*PI/omega();}// seconds

        double period_circular() const {return 2.0*PI/sqrt(GRAVITY*s_CentralMass/distance()/distance()/distance());}// seconds


        void gravityForce(const double *f, double *f_x, double *f_y);
        void gravity_rhs(double *f, double *rhsf);

        void list(QString &props) const;

        static void setCentralMass(double mass) {s_CentralMass=mass;} // the sun, the black hole

    public:
        double m_mass;    // mass
        double m_Radius; // planet radius used for display - meters

        QColor m_Color;
        QString m_Name;

        // RK4 parameters
        double m_var[NDIM]; // x, y, x', y'

        //standard orbital elements https://en.wikipedia.org/wiki/Orbital_elements
        double m_a;        // semi-major axis
        double m_e;        // excentricity
        double m_i;        // inclination
        double m_Omega;    // longitude of the ascending node
        double m_omega;    // argument of periapsis

        double m_RefEnergy;

        static double s_CentralMass;
};


/** Describes a star or a galaxy */
struct Star
{
    double m_Magnitude=0.0;
    double m_Ra=0.0;//right ascension
    double m_Da=0.0;//declination

    Vector3d m_Position;
    QColor m_Color;
    QString m_Name;
};

