//----------------------------------*-C++, Fortran-*----------------------------------// /*!
/* \file   ~/DAGMC/FluDAG/src/cpp/fluka_funcs.cpp
 * \author Julie Zachman 
 * \date   Mon Mar 22 2013 
 * \brief  Functions called by fluka
 * \note   After mcnp_funcs
 */
//---------------------------------------------------------------------------//
// $Id: 
//---------------------------------------------------------------------------//

#include "fluka_funcs.h"

#include "MBInterface.hpp"
#include "MBCartVect.hpp"

#include "DagMC.hpp"
#include "moab/Types.hpp"

using moab::DagMC;

#include <iomanip>
#include <fstream>     // ofstream
#include <sstream>
#include <cstring>
#include <list>        
#include <algorithm>   // sort
#include <utility>     // makepair
#include <stdlib.h>    // atoi

#ifdef CUBIT_LIBS_PRESENT
#include <fenv.h>
#endif

// globals

#define DAG DagMC::instance()

#define DGFM_SEQ   0
#define DGFM_READ  1
#define DGFM_BCAST 2


#ifdef ENABLE_RAYSTAT_DUMPS


#include <fstream>
#include <numeric>

static std::ostream* raystat_dump = NULL;

#endif 

#define ID_START 26

bool debug = false; 

std::set<int> make_exception_set()
{
    std::set<int> nuc_exceptions;
    
    // Preserve FLUKA Entropy: Ref Fluka Manual pp. 318-323
    // Question about 
    // Xenon (many named isotopes, useful only for detectors?)
    // Question: I think we should also put the stable form on the no-collapse list,
    // e.g. H, He, Li, B
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("H-1")));   // HYDROG-1
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("H-2")));   // DEUTERIU
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("H-3")));   // TRITIUM
    
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("He-4")));  // HELIUM-4
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("Li-6")));  // LITHIU-6
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("Li-7")));  // LITHIU-7
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("B-10")));  // BORON-10
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("B-11")));  // BORON-11
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("Sr-90"))); // 90-SR
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("I-129"))); // 129-I
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("Cs-135")));// 135-CS
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("Cs-137")));// 137-CS
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("Th-230")));// 230-TH
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("Th-232")));// 232-TH
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("U-233"))); // 233-U
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("U-234"))); // 234-U
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("U-235"))); // 235-U
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("U-238"))); // 238-U
    // All isotopes should be on the exception list, including the base isotope
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("H")));
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("He")));
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("Li")));
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("B")));
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("Sr")));
    nuc_exceptions.insert(pyne::nucname::id(const_cast<char *>("I")));

    // Print out results
    if (debug)
    {
       std::cout << "Nucids of FLUKA exceptions" << std::endl;
       int i=1;
       for (std::set<int>::iterator ptr = nuc_exceptions.begin(); 
            ptr != nuc_exceptions.end(); ++ptr)
       {
            std::cout << std::setw(10) << std::right << *ptr;
	    if (i%5 == 0)
	    {
	       std::cout << std::endl;
	    }
	    else
	    {
	       std::cout << ", ";
	    }
	    i++;
       }
       std::cout << std::endl;
    }

    return nuc_exceptions;
}

const char *delimiters = ":/";

// an empty synonym map to provide as a default argument to parse_properties()
static const std::map<std::string,std::string> no_synonyms;

/* Maximum character-length of a cubit-named material property */
int MAX_MATERIAL_NAME_SIZE = 32;


/* Static values used by dagmctrack_ */

static DagMC::RayHistory history;

bool on_boundary;
double old_direction[3];
MBEntityHandle next_surf; // the next suface the ray will hit
MBEntityHandle prev_surf; // the last value of next surface
MBEntityHandle PrevRegion; // the integer region that the particle was in previously



/**************************************************************************************************/
/******                                FLUKA stubs                                         ********/
/**************************************************************************************************/
//---------------------------------------------------------------------------//

//---------------------------------------------------------------------------//
// jomiwr(..)
//---------------------------------------------------------------------------//
/// Initialization routine, was in WrapInit.c
//  For DAGMC only sets the number of volumes in the problem
void jomiwr(int & nge, const int& lin, const int& lou, int& flukaReg)
{

  if(debug)
    {
      std::cout << "================== JOMIWR =================" << std::endl;
    }

  //Original comment:  returns number of volumes
  unsigned int numVol = DAG->num_entities(3);
  flukaReg = numVol;

  if(debug)
    {
      std::cout << "Number of volumes: " << flukaReg << std::endl;
      std::cout << "================== Out of JOMIWR =================" << std::endl;
    }

  return;
}

//---------------------------------------------------------------------------//
// g_step(..)
//---------------------------------------------------------------------------//
//  returns approved step of particle and all variables 
//
void g_step(double& pSx, 
          double& pSy, 
          double& pSz, 
          double* pV,
          int& oldReg,         // pass through
          const int& oldLttc,  // ignore
          double& propStep,    // .
          int& nascFlag,       // .
          double& retStep,     // reset in this method
          int& newReg,         // return from callee
          double& saf,         // safety 
          int& newLttc,        // .
          int& LttcFlag,       // . 
          double* sLt,         // .
          int* jrLt)           // .
{
  double safety; // safety parameter

  if(debug)
    {
      std::cout<<"============= G_STEP	 =============="<<std::endl;    
      std::cout << "Position " << pSx << " " << pSy << " " << pSz << std::endl;
      std::cout << "Direction vector " << pV[0] << " " << pV[1] << " " << pV[2] << std::endl;
      std::cout << "Oldreg = " << oldReg << std::endl;
      std::cout << "PropStep = " << propStep << std::endl;
    }
  
  double point[3] = {pSx,pSy,pSz};
  double dir[3]   = {pV[0],pV[1],pV[2]};  

  if(debug)
    {
      std::cout << "cel = " << oldReg << " pos = " << point[0] << " " << point[1] << " " << point[2];
      std::cout << " dir = " << dir[0] << " " << dir[1] << " " << dir[2] ;
      std::cout << " prop = " << propStep ;
    }
  g_fire(oldReg, point, dir, propStep, retStep, saf, newReg); // fire a ray 
  old_direction[0]=dir[0],old_direction[1]=dir[1],old_direction[2]=dir[2];
  if(debug)
    {
      std::cout << " ret = " << retStep;
      std::cout << " new cel = " << newReg << std::endl;

      std::cout << "saf = " << saf << std::endl;
      std::cout << std::setw(20) << std::scientific;
      std::cout << "newReg = " << newReg << " retStep = " << retStep << std::endl;
    }

  return;
}

//---------------------------------------------------------------------------//
// void g_fire(int& oldRegion, double point[], double dir[], 
//              double &propStep, double& retStep,  int& newRegion)
//---------------------------------------------------------------------------//
// oldRegion - the region of the particle's current coordinates
// point     - the particle's coordinate location vector
// dir       - the direction vector of the particle's current path (ray)
// propStep  - ??
// retStep   - returned as the distance from the particle's current location, along its ray, to the next boundary
// newRegion - gotten from the value returned by DAG->next_vol
// newRegion is gotten from the volue returned by DAG->next_vol
void g_fire(int &oldRegion, double point[], double dir[], double &propStep, 
            double &retStep, double &safety,  int &newRegion)
{

  MBEntityHandle vol = DAG->entity_by_index(3,oldRegion);
  double next_surf_dist;
  MBEntityHandle newvol = 0;

  // direction changed reset history
  if( dir[0] == old_direction[0] && dir[1] == old_direction[1] && dir[2] == old_direction[2] ) 
    {   
    }
  else
    {
      history.reset();
    }
   
  oldRegion = DAG->index_by_handle(vol); // convert oldRegion int into MBHandle to the volume
  if(on_boundary)
    {
      if(boundary_test(vol,point,dir)==0) // if ray not on leaving vol
	{
	  history.reset(); // reset history
	  on_boundary = false; // reset on boundary
	}
    }
  

  MBErrorCode result = DAG->ray_fire(vol, point, dir, next_surf, next_surf_dist,&history); // fire a ray 
  if ( result != MB_SUCCESS )
    {
      std::cout << "DAG ray fire error" << std::endl;
      exit(0);
    }

  if ( next_surf == 0 ) // if next_surface is 0 then we are lost
    {
      std::cout << "!!! Lost Particle !!! " << std::endl;
      std::cout << "in region, " << oldRegion << " aka " << DAG->entity_by_index(3,oldRegion) << std::endl;  
      std::cout.precision(25);
      std::cout << std::scientific ; 
      std::cout << "position of particle " << point[0] << " " << point[1] << " " << point[2] << std::endl;
      std::cout << " traveling in direction " << dir[0] << " " << dir[1] << " " << dir[2] << std::endl;
      std::cout << "!!! Lost Particle !!!" << std::endl;
      newRegion = -3; // return error
      return;
    }

  // set the safety
  
  retStep = next_surf_dist; // the returned step length is the distance to next surf
  if ( propStep >= retStep ) // will cross into next volume next step
    {
      MBErrorCode rval = DAG->next_vol(next_surf,vol,newvol);
      newRegion = DAG->index_by_handle(newvol);
      retStep = retStep; //+1.0e-9 ; // path limited by geometry
      next_surf = next_surf;
      on_boundary=true;
      // history is preserved
    }
  else // step less than the distance to surface
    {
      newRegion = oldRegion; // dont leave the current region
      retStep = propStep; //physics limits step
      next_surf = prev_surf; // still hit the previous surface
      history.reset();
	//_to_last_intersection();
      on_boundary=false;
    }

  PrevRegion = newRegion; // particle will be moving to PrevRegion upon next entry.

  if(debug)
  {
     std::cout << "Region on other side of surface is  = " << newRegion << \
                  ", Distance to next surf is " << retStep << std::endl;
  }

  prev_surf = next_surf; // update the surface

  return;
}
///////			End g_step and g_fire
/////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------//
// normal
//---------------------------------------------------------------------------//
// Local wrapper for fortran-called, f_normal.  This function is supplied for testing
// purposes.  Its signature shows what parameters are being used in our wrapper 
// implementation.  
// Any FluDAG calls to f_normal should use this call instead.
// ASSUMES:  no ray history
// Notes
// - direction is not taken into account 
// - curRegion is not currently used.  
int  normal (double& posx, double& posy, double& posz, double *norml, int& curRegion)
{
   int flagErr; 
   int dummyReg;
   double dummyDirx, dummyDiry, dummyDirz;
   f_normal(posx, posy, posz, dummyDirx, dummyDiry, dummyDirz, norml, curRegion, dummyReg, flagErr);
   return flagErr;
}
//---------------------------------------------------------------------------//
// f_normal(..)
//---------------------------------------------------------------------------//
//  Note:  The normal is calculated at the point on the surface nearest the 
//         given point
// ASSUMES:  Point is on the boundary
// Parameters Set:
//     norml vector
//     flagErr = 0 if ok, !=0 otherwise
// Does NOT set any region, point or direction vector.
// Globals used:
//     next_surf, set by ray_fire 
void f_normal(double& pSx, double& pSy, double& pSz,
            double& pVx, double& pVy, double& pVz,
	    double* norml, const int& oldRegion, 
	    const int& newReg, int& flagErr)
{
  if(debug)
  {
      std::cout << "============ NRMLWR =============" << std::endl;
  }

  MBEntityHandle OldReg = DAG -> entity_by_index(3,oldRegion); // entity handle
  double xyz[3] = {pSx,pSy,pSz}; //position vector
  double uvw[3] = {pVx,pVy,pVz}; //particl directoin
  int result; // particle is entering or leaving

  MBErrorCode ErrorCode = DAG->test_volume_boundary( OldReg, next_surf,xyz,uvw, result, &history);  // see if we are on boundary
  ErrorCode = DAG->get_angle(next_surf,xyz,norml); 
  // result = 1 entering, 0 leaving
  if ( result == 0 ) // vector should point towards OldReg
    {
      norml[0] = norml[0]*-1.0;
      norml[1] = norml[1]*-1.0;
      norml[2] = norml[2]*-1.0;
    }


  if(debug)
  {
      std::cout << "Normal: " << norml[0] << ", " << norml[1] << ", " << norml[2]  << std::endl;
  }
  return;
}
///////			End normal() and f_normal()

/////////////////////////////////////////////////////////////////////
//
// check_vol(..)
//
// Returns either true or false, if the point pos belongs to the region oldRegion
//
//
inline bool check_vol( double pos[3], double dir[3], int oldRegion)
{
  int is_inside; // in volume or not
  // convert region id into entityhandle
  MBEntityHandle volume = DAG->entity_by_index(3, oldRegion); // get the volume by index
  MBErrorCode code = DAG->point_in_volume(volume, pos, is_inside,dir);
  if ( code != MB_SUCCESS)
    {
      std::cout << "Failed in DAG call to get point_in_volume" << std::endl;
    }

  if ( is_inside == 1 ) // we are inside the cell tested
    return true;
  else
    return false;
}


/////////////////////////////////////////////////////////////////////
//---------------------------------------------------------------------------//
// look(..)
//---------------------------------------------------------------------------//
// Testable local wrapper for fortran-called, f_look
// This function signature shows what parameters are being used in our wrapper implementation
// oldRegion is looked at if we are no a boundary, but it is not set.
// ASSUMES:  position is not on a boundary
// RETURNS: nextRegion, the region the given point is in 
int look( double& posx, double& posy, double& posz, double* dir, int& oldRegion)
{
   int flagErr;
   int lattice_dummy;  // not used
   int nextRegion;     
   f_look(posx, posy, posz, dir, oldRegion, lattice_dummy, nextRegion, flagErr, lattice_dummy);
   return nextRegion;
}
//---------------------------------------------------------------------------//
// f_look(..)
//---------------------------------------------------------------------------//
// Wrapper for localisation of starting point of particle.
//
// Question:  Should pV, the direction vector, be used?  
//////////////////////////////////////////////////////////////////
// This function answers the question What volume is the point in?  
// oldReg - not looked at UNLESS the volume is on the boundary, then newReg=oldReg
// nextRegion - set to the volume index the point is in.
// ToDo:  Is there an error condition for the flagErr that is guaranteed not to be equal to the next region?
//        Find a way to make use of the error return from point_in_volume
void f_look(double& pSx, double& pSy, double& pSz,
          double* pV, const int& oldReg, const int& oldLttc,
          int& nextRegion, int& flagErr, int& newLttc)
{
  if(debug)
  {
      std::cout << "======= LKWR =======" << std::endl;
      std::cout << "position is " << pSx << " " << pSy << " " << pSz << std::endl; 
  }
  
  history.reset();

  double xyz[] = {pSx, pSy, pSz};       // location of the particle (xyz)
  const double dir[] = {pV[0],pV[1],pV[2]};
  // Initialize to outside boundary.  This value can be 0 or +/-1 for ouside, inside, or on boundary.
  // ToDo:  Should this be initialized at all?  Or should it be initialized to an invalide number?
  int is_inside = 0;                    
  int num_vols = DAG->num_entities(3);  // number of volumes

  for (int i = 1 ; i <= num_vols ; i++) // loop over all volumes
    {
      MBEntityHandle volume = DAG->entity_by_index(3, i); // get the volume by index
      // No ray history 
      MBErrorCode code = DAG->point_in_volume(volume, xyz, is_inside);

      // check for non error
      if(MB_SUCCESS != code) 
	{
	  std::cout << "Error return from point_in_volume!" << std::endl;
	  flagErr = -3;
	  return;
	}

      if ( is_inside == 1 ) // we are inside the cell tested
	{
	  nextRegion = i;
          //BIZARRELY - WHEN WE ARE INSIDE A VOLUME, BOTH, nextRegion has to equal flagErr
	  flagErr = nextRegion;
	  return;	  
	}
      else if ( is_inside == -1 )
	{
	  std::cout << "We cannot be here" << std::endl;
	  exit(0);
	}
    }  // end loop over all volumes

  flagErr = nextRegion; // return nextRegion
  return;
}

void f_lostlook(double& pSx, double& pSy, double& pSz,
          double* pV, const int& oldReg, const int& oldLttc,
          int& nextRegion, int& flagErr, int& newLttc)
{
    f_look(pSx,pSy,pSz,pV,oldReg,oldLttc,nextRegion,flagErr,newLttc);
    return;
}

/*
 * entering or leaving, if particle on boundary 
 */
int boundary_test(MBEntityHandle vol, double xyz[3], double uvw[3])
{
  int result;
  MBErrorCode ErrorCode = DAG->test_volume_boundary(vol,next_surf,xyz,uvw, result,&history);  // see if we are on boundary
  return result;
}
/*
 *   Particle localisation when magnetic field tracking is on
 */
void lkmgwr(double& pSx, double& pSy, double& pSz,
            double* pV, const int& oldReg, const int& oldLttc,
	    int& flagErr, int& newReg, int& newLttc)
{
  

    const double xyz[] = {pSx, pSy, pSz}; // location of the particle (xyz)
    int is_inside = 0; // logical inside or outside of volume
    int num_vols = DAG->num_entities(3); // number of volumes

    for (int i = 1 ; i <= num_vols ; i++) // loop over all volumes
      {
	MBEntityHandle volume = DAG->entity_by_index(3, i); // get the volume by index
	// No ray history or ray direction.
	MBErrorCode code = DAG->point_in_volume(volume, xyz, is_inside);

	// check for non error
	if(MB_SUCCESS != code) 
	  {
	    std::cout << "Error return from point_in_volume!" << std::endl;
	    flagErr = 1;
	    return;
	  }

	if ( is_inside == 1 ) // we are inside the cell tested
	  {
	    newReg = i;
	    flagErr = i+1;
	    if(debug)
	      {
		std::cout << "point is in region = " << newReg << std::endl;
	      }
	    return;
	  }
      }  // end loop over all volumes

    std::cout << "particle is nowhere!" << std::endl;
    newReg = -100;
    std::cout << "point is not in any volume" << std::endl;
    return;
}

void f_lookdb(double& pSx, double& pSy, double& pSz,
	    double* pV, const int& oldReg, const int& oldLttc,
	    int& newReg, int& flagErr, int& newLttc)
{
  if(debug)
    {
      std::cout<<"============= F_LooKDB =============="<< std::endl;
    }
  //return region number and dummy variables
  newReg=0;   
  newLttc=0;
  flagErr=-1; 

  return;
}


/*
 * f_g1rt
 */
void f_g1rt(void)
{
  if(debug)
    {
      std::cout<<"============ F_G1RT ============="<<std::endl;
    }
    return;
}

// Set DNEAR option if needed
int f_idnr(const int & nreg, const int & mlat) 

{
	

// returns 0 if user doesn't want Fluka to use DNEAR to compute the 
// step (the same effect is obtained with the GLOBAL (WHAT(3)=-1)
// card in fluka input), returns 1 if user wants Fluka always to use DNEAR.

	return 0;
}

///////////////////////////////////////////////////////////////////
// from WrapReg2Name.cc 
//
// Wrapper for getting region name corresponding to given region number
///////////////////////////////////////////////////////////////////
void rg2nwr(const int& mreg, const char* Vname)
{
  std::cout << "============= RG2NWR ==============" << std::endl;    
  std::cout << "mreg=" << mreg << std::endl;
  std::string vvname;
  region2name(mreg, vvname);
  Vname = vvname.c_str();
  std::cout << "reg2nmwr: Vname " << Vname<< std::endl;  
  return;
}

///////////////////////////////////////////////////////////////////
// from WrapReg.hh 
//
// Wrapper for scoring hits: previous step end-point is taken from 
// history (and compared with fluka region index, flukaReg),
// then the wrapper returns all the information regarding the 
// volume tree, i.e. returns indMother[] array with all the 
// mother volumes index and repMother[] array with all the 
// mother volumes repetition number.   
///////////////////////////////////////////////////////////////////
void rgrpwr(const int& flukaReg, const int& ptrLttc, int& g4Reg,
            int* indMother, int* repMother, int& depthFluka)
{
  std::cout << "============= RGRPWR ==============" << std::endl;    
  std::cout << "ptrLttc=" << ptrLttc << std::endl;
  return;
}

///////////////////////////////////////////////////////////////////
// from WrapMag.hh
//
// Wrapper for geometry tracking in magnetic field: returns magnetic 
// field values in a given position.
/////////////////////////////////////////////////////////////////
void fldwr(const double& pX, const double& pY, const double& pZ,
            double& cosBx, double& cosBy, double& cosBz, 
            double& Bmag, int& reg, int& idiscflag)

{
  std::cout<<"================== MAGFLD ================="<<std::endl;
  return;
}

///////////////////////////////////////////////////////////////////
// from WrapFlgfwr.cc
//
// Wrapper for setting of fluka geometry flag
//////////////////////////////////////////////////////////////////
void flgfwr ( int& flkflg )
{
  std::cout << "=======FLGFWR =======" << std::endl;
  return;
}

///////////////////////////////////////////////////////////////////
// from WrapLookFX.hh
//
// Wrapper for localisation of particle to fix particular conditions.
// At the moment is the same as WrapLookZ.hh. 
//////////////////////////////////////////////////////////////////
void lkfxwr(double& pSx, double& pSy, double& pSz,
            double* pV, const int& oldReg, const int& oldLttc,
	    int& newReg, int& flagErr, int& newLttc)
{
  std::cout << "======= LKFXWR =======" << std::endl;

  return;
}

/**************************************************************************************************/
/******                                End of FLUKA stubs                                  ********/
/**************************************************************************************************/

// FluDAG Material Card  Functions
void fludag_write(std::string matfile, std::string lfname)
{
  // Use DAG to read and count the volumes.  
  std::map<int, std::string> map_name;
  if (0 == DAG->num_entities(3) )
  {
     std::cout << "Error: there are no volumes in this geometry!" << std::endl;
     return;
  }

  // get the pyne materials and tallies
  UWUW workflow_data = UWUW(matfile);

  std::list<pyne::Material> pyne_list;
  std::map<std::string, pyne::Material> pyne_map;
  pyne_map = workflow_data.material_library;

  // ASSIGNMA Cards
  std::ostringstream astr;
  fludagwrite_assignma(astr, pyne_map, map_name);

  // MATERIAL Cards
  pyne::NUC_DATA_PATH = workflow_data.full_filepath; // for atomic data

  // write COMPOUND CARDS
  std::ostringstream mstr;
  fludag_all_materials(mstr, pyne_map);

  // Write all the streams to the input file
  std::string header = "*...+....1....+....2....+....3....+....4....+....5....+....6....+....7...";
  std::ofstream lcadfile (lfname.c_str());
  lcadfile << header << std::endl;
  lcadfile << astr.str();
  lcadfile << header << std::endl;
  lcadfile << mstr.str();

  lcadfile << header << std::endl;
  lcadfile << "* UW**2 tallies" << std::endl;
  mstr.str("");
  fludag_all_tallies(mstr,workflow_data.tally_library);
  lcadfile << mstr.str();

  // all done
  lcadfile.close();
}

//---------------------------------------------------------------------------//
// fludagwrite_assignma
//---------------------------------------------------------------------------//
// Put the ASSIGNMAt statements in the output ostringstream
void fludagwrite_assignma(std::ostringstream& ostr, 
                          std::map<std::string, pyne::Material> pyne_map,    
			  std::map<int, std::string> map_name)         
{
  // get the material and density props
  std::map<MBEntityHandle,std::vector<std::string> > material_assignments = get_property_assignments("mat",3,":/");
  std::map<MBEntityHandle,std::vector<std::string> > density_assignments = get_property_assignments("rho",3,":/");

  pyne::Material material;

  std::vector<std::string> material_props,density_props;

  // loop over all volumes
  for (unsigned int vol_i = 1 ; vol_i <= DAG->num_entities(3) ; vol_i++)
  {
    int cellid = DAG->id_by_index( 3, vol_i );
    MBEntityHandle entity = DAG->entity_by_index( 3, vol_i );

    material_props = material_assignments[entity];
    density_props = density_assignments[entity];
    
    if( material_props.size() > 1 ) {
      std::cout << "more than one material for volume with id " << cellid << std::endl;
      std::cout << cellid << " has the following material assignments" << std::endl;
      for ( int j = 0 ; j < material_props.size() ; j++ ) {
	std::cout << material_props[j] << std::endl;
      }
      std::cout << "Please check your material assignments " << cellid << std::endl;
      exit(EXIT_FAILURE);
    }
    if(density_props.size() > 1) {
      std::cout << "More than one density specified for " << cellid <<std::endl;
      std::cout << cellid << " has the following density assignments" << std::endl;
      for ( int j = 0 ; j < density_props.size() ; j++ ) {
	std::cout << density_props[j] << std::endl;
      }
      std::cout << "Please check your density assignments " << cellid << std::endl;
      exit(EXIT_FAILURE);
    }

    std::string grp_name = "";
    if (!density_props[0].empty())
      grp_name = "mat:"+material_props[0]+"/rho:"+density_props[0];
    else
      grp_name = "mat:"+material_props[0];


    std::string fluka_name = "";

    // not graveyard or vacuum or implicit compliment
    if (grp_name.find("Graveyard") == std::string::npos && grp_name.find("Vacuum") == std::string::npos 
	&& !(DAG->is_implicit_complement(entity)) ) 
      {
	material = pyne_map[grp_name];
	fluka_name = material.metadata["fluka_name"].asString();
      }
    // found graveyard
    else if (grp_name.find("Graveyard") != std::string::npos || 
	     grp_name.find("graveyard") != std::string::npos )  
      {
	fluka_name = "BLCKHOLE";
      }
    // vacuum
    else if (grp_name.find("Vacuum") != std::string::npos)  
      {
	 fluka_name = "VACUUM";
      }
    else if (  DAG->is_implicit_complement(entity) )
      {
	 fluka_name = "VACUUM";
      }

    // The fluka name has been found, create the card
    ostr << std::setw(10) << std::left  << "ASSIGNMA ";
    ostr << std::setw(10) << std::right << fluka_name;
    ostr << std::setprecision(0) << std::fixed << std::showpoint 
	 << std::setw(10) << std::right << (float)vol_i << std::endl;

  }   // End loop through vol_i
  std::cout << std::endl;

}  // end fludagwrite_assignma


// Get tally cards for all tallies in the problem
void fludag_all_tallies(std::ostringstream& mstr, std::map<std::string,pyne::Tally> tally_map)
{
  int start_unit = 21; // starting unit number for tallies

  std::map<std::string,pyne::Tally>::iterator it;

  // generate number of tally/particle pairs
  std::list<std::string> tally_parts;
  std::string tally_id;
  for ( it = tally_map.begin() ; it != tally_map.end() ; ++it ) {
    tally_id = (it->second).tally_type+"/"+(it->second).particle_name;
    if( std::count(tally_parts.begin(),tally_parts.end(),tally_id) == 0 )
      {
	tally_parts.insert(tally_parts.end(),tally_id);
      }
  }
  
  // loop over tallies in map
  for ( it = tally_map.begin() ; it != tally_map.end() ; ++it ) {
    pyne::Tally tally = (it->second);
    // pyne tallies are by id, FluDAG is by index, need to convert
    MBEntityHandle vol_eh = DAG->entity_by_id(3,tally.entity_id);
    // volume index
    int vol_idx = DAG->index_by_handle(vol_eh);
    // recast tally to index, use entity_name for setting volume

    MBErrorCode rval = DAG->measure_volume(vol_eh,tally.entity_size);

    std::stringstream ss;
    ss << vol_idx;
    ss << ".";
    tally.entity_name = ss.str();

    std::string tally_id = tally.tally_type+"/"+tally.particle_name;

    std::list<std::string>::iterator iter = std::find (tally_parts.begin(), tally_parts.end(), tally_id);

    int unit_number = std::distance(tally_parts.begin(), iter) + start_unit;

    ss.str(std::string());
    ss << "-";
    ss << unit_number;
    

    mstr << tally.fluka(ss.str()) << std::endl;
  }
  
  return;
}

//---------------------------------------------------------------------------//
// fludag_all_materials
//---------------------------------------------------------------------------//
// Get material cards for all materials in the problem, both elemental and compounds
void fludag_all_materials(std::ostringstream& mstr, std::map<std::string,pyne::Material> pyne_map)
{
  std::set<int> exception_set = make_exception_set();

  std::map<int, std::string> map_nucid_fname;

  pyne::Material unique = pyne::Material();

  // loop over all materials, summing
  std::map<std::string, pyne::Material>::iterator nuc;
  for ( nuc = pyne_map.begin(); nuc != pyne_map.end(); ++nuc)
  {
    unique = unique + (nuc->second);
  }
  // now collapse elements
  unique = unique.collapse_elements(exception_set);

  // number of required material cards due to calls
  int num_mat = unique.comp.size();

  // write out material card for each one
  int i = ID_START;
  pyne::comp_map::iterator element;
  std::string mat_line;
  for ( element = unique.comp.begin() ; element != unique.comp.end() ; ++element)
    {
      int nuc_id = element->first; // get the nuc id
      pyne::comp_map nucvec;
      nucvec[nuc_id] = 100.0; // create temp nucvec
      pyne::Material element_tmp = pyne::Material(nucvec); // create temp material
      mat_line = element_tmp.fluka(i);
      if (mat_line.length() != 0)
      {
         i++;
	 mstr << mat_line;
      }
    }

  // now write out material card & compound card for each compound
  std::string compound_string;
  for ( nuc = pyne_map.begin() ; nuc != pyne_map.end(); ++nuc)
  {
    pyne::Material compound = (nuc->second).collapse_elements(exception_set);
    compound_string = compound.fluka(i);
    if ( compound_string.length() != 0 )
      {
	i++;
        mstr << compound_string;
      }
  }
}

// region2name 
void region2name(int volindex, std::string &vname )  // file with cell/surface cards
{
  std::stringstream ss;
  ss << volindex;
  ss << ".";
  vname = ss.str();
}

// get all property in all volumes
std::map<MBEntityHandle,std::vector<std::string> > get_property_assignments(std::string property, 
									    int dimension, std::string delimiters)
{

  std::map<MBEntityHandle,std::vector<std::string> > prop_map;

  std::vector< std::string > mcnp5_keywords;
  std::map< std::string, std::string > mcnp5_keyword_synonyms;

  // populate keywords
  mcnp5_keywords.push_back( "mat" );
  mcnp5_keywords.push_back( "rho" );
  mcnp5_keywords.push_back( "tally" );

  // get initial sizes
  int num_entities = DAG->num_entities( dimension );

  // parse data from geometry
  MBErrorCode rval = DAG->parse_properties( mcnp5_keywords, mcnp5_keyword_synonyms,delimiters.c_str());

  if (MB_SUCCESS != rval) {
    std::cout << "DAGMC failed to parse metadata properties" <<  std::endl;
    exit(EXIT_FAILURE);
  }


  // loop over all cells
  for( int i = 1; i <= num_entities; ++i ) {
    // get cellid
    MBEntityHandle entity = DAG->entity_by_index( dimension, i );

    std::vector<std::string> properties;
    std::vector<std::string> tmp_properties;


    // get the group contents
    if( DAG->has_prop( entity, property ) )
      {
	rval = DAG->prop_values(entity,property,tmp_properties);
	properties.push_back(tmp_properties[0]);
      }
    else
      properties.push_back("");

    prop_map[entity]=properties;

  }

  return prop_map;
}
