/*
 *   Project: OpenCL Framework and kernels for pyFAI.
 *
 *   List of files: ocl_base.cpp
 *                  ocl_base.hpp
 *                  ocl_xrpd1d_fullsplit.cpp
 *                  ocl_xrpd2d_fullsplit.cpp
 *                  ocl_xrpd1d.hpp
 *                  ocl_xrpd2d.hpp
 *                  ocl_tools.h
 *                  ocl_ckerr.h
 *                  ocl_azim_kernel_2.cl
 *                  ocl_azim_kernel2D_2.cl
 *                  ocl_xrpd1d.i
 *                  ocl_xrpd2d.i
 *                  setup_xrpd1d.py
 *                  setup_xrpd2d.py
 *
 *   Copyright (C) 2011-12 European Synchrotron Radiation Facility
 *                           Grenoble, France
 *
 *   Principal authors: D. Karkoulis (karkouli@esrf.fr)
 *   Last revision: 24/06/2011
 *    
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   and the GNU Lesser General Public License  along with this program.
 *   If not, see <http://www.gnu.org/licenses/>.
 */

/* OpenCL library for acceleration of Azimuthal regroupping */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>

#include "ocl_base.hpp"

#define CE CL_CHECK_ERR_PR
#define C  CL_CHECK_PR

#define CEN CL_CHECK_ERR_PRN
#define CN  CL_CHECK_PRN

#define CER CL_CHECK_ERR_PR_RET
#define CR  CL_CHECK_PR_RET

//#define silent
#ifdef _SILENT
  #define fprintf(stream,...)
#endif


typedef unsigned long lui;

/**
 * \brief Overloaded constructor for base class.
 *
 * Complete logging functionality
 *
 */
ocl::ocl(FILE *stream, const char *fname, int safe, int depth, int perf_time, int timestamp, const char *identity):exec_identity(identity)
{

  cLog_init(&hLog,stream,fname,0,static_cast<enum_LOGTYPE>(safe),static_cast<enum_LOGDEPTH>(depth),perf_time,timestamp);
  if(identity)
    cLog_date_text(&hLog,LOGDNONE,"(%s)\n",identity);
  else
    cLog_date(&hLog,LOGDNONE);
  usesStdout = 0;

  if(static_cast<enum_LOGDEPTH>(depth) >= LOGDBASIC) cLog_log_configuration(&hLog);

  ContructorInit();
  setDocstring("OpenCL base functionality for xrpd1d. \nFeel free to play around but you will not be able to perform integrations"\
                "at this level.\nYou may check the OpenCL platforms and devices found in your system at this point. Try print_devices\n"\
                "Try any of the derived classes xrpd1d and xrpd2d for complete functionality. \n","ocl_base.hpp");

}

/**
 * \brief Overloaded constructor for base class.
 *
 * Output is set to file "fname"
 *
 */
ocl::ocl(const char *fname, const char *identity):exec_identity(identity)
{

  cLog_init(&hLog,NULL,fname,0,LOGTFAST,LOGDDEBUG,1,1);
  if(identity)
    cLog_date_text(&hLog,LOGDNONE,"(%s)\n",identity);
  else
    cLog_date(&hLog,LOGDNONE);
  usesStdout = 0;

  cLog_log_configuration(&hLog);

  ContructorInit();
  setDocstring("OpenCL base functionality for xrpd1d. \nFeel free to play around but you will not be able to perform integrations"\
                "at this level.\nYou may check the OpenCL platforms and devices found in your system at this point. Try print_devices\n"\
                "Try any of the derived classes xrpd1d and xrpd2d for complete functionality. \n","ocl_base.hpp");

}

/**
 * \brief Default constructor for base class.
 *
 * Output is set to stdout
 *
 */
ocl::ocl():exec_identity(NULL){

  cLog_init(&hLog,stdout,NULL,0,LOGTFAST,LOGDDEBUG,1,1);
  cLog_date(&hLog,LOGDNONE);
  usesStdout = 1;

  cLog_report_configuration(&hLog);

  ContructorInit();
  setDocstring("OpenCL base functionality for xrpd1d. \nFeel free to play around but you will not be able to perform integrations"\
                "at this level.\nYou may check the OpenCL platforms and devices found in your system at this point. Try print_devices\n"\
                "Try any of the derived classes xrpd1d and xrpd2d for complete functionality. \n","ocl_base.hpp");  

}

ocl::~ocl(){
  clean();
  ocl_tools_destroy(oclconfig);
  delete oclconfig;
  delete sgs;
  cLog_fin(&hLog);
  delete[] docstr;

}

/**
 * \brief Common initialisations for all the constructors
 */
void ocl::ContructorInit()
{
  stream=stdout;
  usesStdout=1;
  
  hasActiveContext=0;
  isConfigured  = 0;
  hasQueue      = 0;
  hasBuffers    = 0;
  hasProgram    = 0;
  hasKernels    = 0;
  hasTthLoaded  = 0;
  hasChiLoaded  = 0;
  
  useSolidAngle = 0;
  useMask       = 0;
  useDummyVal   = 0;
  useTthRange   = 0;
  useChiRange   = 0;

  reset_time();
  
  oclconfig = new ocl_config_type;
  ocl_tools_initialise(oclconfig,&hLog);

  sgs = new az_argtype;
  sgs->Nx = 0;
  sgs->Nimage = 0;
  sgs->Nbins = 0;
  sgs->Nbinst = 0;
  sgs->Nbinsc = 0;
  sgs->usefp64 = 0;
  
  docstr = new char[8192];
  
}

void ocl::update_logger(FILE *stream, const char *fname, int safe, int depth, int perf_time, int timestamp)
{
  cLog_fin(&hLog);
  cLog_init(&hLog,stream,fname,0,static_cast<enum_LOGTYPE>(safe),static_cast<enum_LOGDEPTH>(depth),perf_time,timestamp);
}

/**
 *  \brief Prints a list of OpenCL capable devices, their platforms and their ids to stream
 */  
void ocl::show_devices(int ignoreStream){

  //Print all the probed devices
  if(ignoreStream) 
  {
    oclconfig->hLog->status=0;
    ocl_check_platforms(oclconfig->hLog);
    oclconfig->hLog->status=1;
  }
  else ocl_check_platforms(oclconfig->hLog);

return;
}

/**
 * \brief Returns the pair ID (platform.device) of the active device
 */
void ocl::get_contexed_Ids(int &platform, int &device)
{
  platform = oclconfig->platfid;
  device   = oclconfig->devid;
return;  
}

/**
 * \brief Returns the -C++- pair ID (platform.device) of the active device
 */
std::pair<int,int> ocl::get_contexed_Ids()
{
return std::make_pair(oclconfig->platfid, oclconfig->devid);  
}

/**
 * \brief Prints some basic information about the device in use
 *
 * The following platform information is displayed:
 *     Name, Version, Vendor and Extensions
 * Similarily for the device:
 *     Name, Type, Version, Driver version, Extensions and Global memory
 * To datafields are also accessible externally by platform_info and
 * device_info structs (not oclconfig->_info!. oclconfig is protected)
 * 
 * @param ignoreStream Integer flag that tells the function to
 *            ignore any active output stream (stdout or not)
 *            and redirect output to display
 */
void ocl::show_device_details(int ignoreStream){
	
	if(hasActiveContext)
	{
		int dev,plat;
		std::ostringstream heading_stream;
    char *heading;
		char cast_plat,cast_dev;

		get_contexed_Ids(plat,dev);

		cast_plat = '0' + (char)plat;
		cast_dev  = '0' + (char)dev;

		heading_stream << '(' << cast_plat << '.' << cast_dev << ')' << ' ';
    heading = new char [heading_stream.str().length() + 1];
    strcpy(heading,heading_stream.str().c_str());
		//Force cLogger to print to stdout
    if(ignoreStream) hLog.status = 0;

		cLog_extended(&hLog,"%s Platform name: %s\n", heading, oclconfig->platform_info.name);
		cLog_extended(&hLog,"%s Platform version: %s\n", heading, oclconfig->platform_info.version);
		cLog_extended(&hLog,"%s Platform vendor: %s\n", heading, oclconfig->platform_info.vendor);
		cLog_extended(&hLog,"%s Platform extensions: %s\n", heading, oclconfig->platform_info.extensions);

		cLog_extended(&hLog,"\n");

		cLog_extended(&hLog,"%s Device name: %s\n", heading, oclconfig->device_info.name);
		cLog_extended(&hLog,"%s Device type: %s\n", heading, oclconfig->device_info.type);
		cLog_extended(&hLog,"%s Device version: %s\n", heading, oclconfig->device_info.version);
		cLog_extended(&hLog,"%s Device driver version: %s\n", heading, oclconfig->device_info.driver_version);
		cLog_extended(&hLog,"%s Device extensions: %s\n", heading, oclconfig->device_info.extensions);
		cLog_extended(&hLog,"%s Device Max Memory: %f (MB)\n", heading, oclconfig->device_info.global_mem/1024.f/1024.f);

    //Revert cLogger to normal operation
    if(ignoreStream) hLog.status = 1;
    delete [] heading;
	}
return;
}

/**
 * \brief Makes platform and device info datafields visible to external callers
 *
 * Promote_device_details() is called after each successfull context creation by an
 * init() function. It copies the internal info structures to the public
 * platform_info and device_info.
 * 
 */
void ocl::promote_device_details()
{
	platform_info = oclconfig->platform_info;
	device_info   = oclconfig->device_info;
}
/**
 * \brief Returns a documentation string
 */
void ocl::help(){

  printf("%s\n",docstr);
  return;
}

/**
 * \brief Init a default context.
 *
 * Typically the default is GPU but may vary depending on the prominent libOpenCL.
 * If a context exists, clean() is called to reset and prevent memory leaks before requesting
 * the new context
 *
 * @param useFp64 Optional Boolean parameter to limit search only in FP64 capable devices (default = false).
 *
 */
int ocl::init(const bool useFp64){

  //Pick a device and initiate a context. If a context exists destroy it
  clean();
  if(ocl_init_context(oclconfig,"DEF",(int)useFp64)) return -1;
  else hasActiveContext=1;
  promote_device_details();

return 0;
}

/**
 * \brief Init a context with predefined device type.
 *
 * If a context exists, clean() is called to reset and prevent memory leaks before requesting
 * the new context
 *
 * @param devicetype A string containing the type of the required device. Possible options are
 *                   "GPU","gpu","CPU","cpu","ACC","acc","DEF","def","ALL","all"
 * @param useFp64 Optional Boolean parameter to limit search only in FP64 capable devices (default = false).
 *
 */
int ocl::init(const char *devicetype,const bool useFp64){

  //Pick a device and initiate a context. If a context exists destroy it
  this->clean();
  if(ocl_init_context(oclconfig,devicetype,(int)useFp64)) return -1;
  else hasActiveContext=1;
  promote_device_details();

return 0;
}

/**
 * \brief Init a context with predefined device type and device id.
 *
 * If a context exists, clean() is called to reset and prevent memory leaks before requesting
 * the new context. The platform and device id can be queried by the show_devices() method of
 * the parent class.
 *
 * @param devicetype A string containing the type of the required device. Possible options are
 *                   "GPU","gpu","CPU","cpu","ACC","acc","DEF","def","ALL","all"
 * @param platformid An integer stating the id of the platform (C notation)
 * @param devid An integer stating the id of the device (C notation)
 * @param useFp64 Optional Boolean parameter to limit search only in FP64 capable devices (default = True).
 *
 */
int ocl::init(const char *devicetype,int platformid,int devid,const bool useFp64){

  //Pick a device and initiate a context. If a context exists destroy it
  clean();
  if(ocl_init_context(oclconfig,devicetype,platformid,devid,(int)useFp64)) return -1;
  else hasActiveContext=1;
  promote_device_details();

return 0;
}

/**
 * \brief Free OpenCL related resources allocated by the library.
 *
 * clean() is used to reinitiate the library back in a vanilla state.
 * It may be asked to preserve the context created by init or completely clean up OpenCL.
 * Guard/Status flags that are set will be reset.
 *
 */
int ocl::clean(int preserve_context){

  reset_time();
  if(!preserve_context)
  {
    if(hasActiveContext){
      ocl_destroy_context(oclconfig->oclcontext, &hLog);
      hasActiveContext=0;
      cLog_debug(&hLog,"--released OpenCL context\n");
      return 0;
    }
  }

return -2;
}

/**
 * \brief Forcibly and recklessly release and OpenCL context
 *
 * Calls to kill_context() may result to memory leaks depending on occassion and OpenCL
 * driver. Only available to test such cases and will be removed in the future.
 */
void ocl::kill_context(){
  if(hasActiveContext)
  {
    ocl_destroy_context(this->oclconfig->oclcontext, &hLog);
    hasActiveContext=0;
    cLog_debug(&hLog,"Forced destroy context\n");
  }else cLog_debug(&hLog,"Attempted Forced destroy context ignored\n");
  return;
}

/**
 * \brief Resets profiling counters to initial values
 */
void ocl::reset_time()
{
  execTime_ms   = 0.0f;
  memCpyTime_ms = 0.0f;
  execCount = 0;
}

/**
 * \brief Returns the total kernel execution time in ms
 */
float ocl::get_exec_time()
{
  return execTime_ms;
}

/**
 * \brief Returns the total time spent in memory copies in ms
 */
float ocl::get_memCpy_time()
{
  return memCpyTime_ms;
}

/**
 * \brief Returns the count of integrations performed
 */
unsigned int ocl::get_exec_count()
{
  return execCount;
}

/*
 * Get the status of the intergator as an integer
 * bit 0: has context
 * bit 1: size are set
 * bit 2: is configured (kernel compiled)
 * bit 3: pos0/delta_pos0 arrays are loaded (radial angle)
 * bit 4: pos1/delta_pos1 arrays are loaded (azimuthal angle)
 * bit 5: solid angle correction is set
 * bit 6: mask is set
 * bit 7: use dummy value
 */
int ocl::get_status(){
	int value=0;
	if (hasActiveContext)
		value+=1;
	if  (isConfigured)
		value+=2;
	if (hasProgram)
		value+=4;
	if (hasTthLoaded)
		value+=8;
	if (hasChiLoaded)
		value+=16;
	if (useSolidAngle)
		value+=32;
	if (useMask)
		value+=64;
	if (useDummyVal)
		value+=256;
	return value;


}
/**
 * \brief Progressive OpenCL buffer release
 *
 * Releases OpenCL buffers referenced by the OpenCL toolbox using
 * ocl_relNbuffers_byref
 *
 * @param level How many buffers to "pop" (last to first)
 */
void ocl::clean_clbuffers(int level){
  //Check that level < Nbuffers is done internally
  ocl_relNbuffers_byref(oclconfig,level);
}

/**
 * \brief Progressive OpenCL kernel release
 *
 * Releases OpenCL kernels referenced by the OpenCL toolbox using
 * ocl_relNkernels_byref
 *
 * @param level How many kernels to "pop" (last to first)
 */
void ocl::clean_clkernels(int level){
  ocl_relNkernels_byref(oclconfig,level);
}

/**
 * A default text exists, but the method will try to load its readme file.
 * If the file cannot be found or another problem exists, the default string is returned.
 *
 */
void ocl::setDocstring(const char *default_text, const char *filename)
{
  using namespace std;

  //Create a backup string in case the default string fails to reallocate
  char *bkp = new char[8192];
  strncpy(bkp,default_text,8192);
  if(bkp[8191]!='\0')bkp[8191]='\0';
  strcpy(docstr,bkp);

  ifstream readme;
  std::streamoff len=0;

  readme.open(filename,ios::in | ios::binary);

  //If the file exists:
  if(readme){

    //And can find the end get its size and rewind it.
    readme.seekg(0,ios_base::end);
    len = readme.tellg();
    readme.seekg(0,ios_base::beg);

    //If the file is bigger than the default string array we need to reallocate it.
    //If reallocation failed we point the default string to the backup string to avoid segmentation fault if help() is called
    //A successfull reallocation means we need to delete the backup string
    if(len >= 8192)
    {
      if(docstr) delete[] docstr;
      docstr = NULL;
      docstr = new char[len+1];
      if(!docstr)
      {
        docstr = bkp;
        readme.close();
        return;
      }else delete[] bkp;

    //-1 is the fail code of tellg. In that case docstring has not been reallocated.
    //we just need to delete the backup string and return the default one
    }else if (len == -1)
    {
      readme.close();
      delete[] bkp;
      return;
    }else delete[] bkp;

    //Read from file and check we read ALL the data
    if( readme.read(docstr,len).gcount() != len) cLog_critical(&hLog,"setDocstring read size mismatch!\n");
    docstr[len] = '\0';
    readme.close();
  }
  return;
}
