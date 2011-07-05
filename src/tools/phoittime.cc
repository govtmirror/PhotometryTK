// __BEGIN_LICENSE__
// Copyright (C) 2006-2011 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__


// What's this file supposed to do ?
//
// (Pho)tometry (It)eration Exposure (Time) Update
//
// With Reflectance
// .... see docs
//
// With out Relectance
//      T = T + sum((Ik-Tk*A)*A*Sk)/sum((A*Sk)^2)

#include <vw/Image.h>
#include <vw/Plate/PlateFile.h>
#include <photk/RemoteProjectFile.h>
#include <photk/TimeAccumulators.h>
#include <photk/Macros.h>
#include <photk/Common.h>
using namespace vw;
using namespace vw::platefile;
using namespace photk;

#include <boost/foreach.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

using namespace std;

struct Options : photk::BaseOptions {
  // Input
  Url ptk_url;
  bool dry_run;

  // For spawning multiple jobs
  int job_id, num_jobs, level;
};

void update_exposure( Options& opt ) {

  // Open up project file
  RemoteProjectFile remote_ptk( opt.ptk_url );
  ProjectMeta project_info;
  remote_ptk.get_project( project_info );

  // Deciding what cameras
  int minidx, maxidx;
  minidx = float(project_info.num_cameras()*opt.job_id)/float(opt.num_jobs);
  maxidx = float(project_info.num_cameras()*(opt.job_id+1))/float(opt.num_jobs);

  // Load platefile
  boost::shared_ptr<PlateFile> drg_plate, albedo_plate, reflect_plate;
  remote_ptk.get_platefiles(drg_plate,albedo_plate,reflect_plate);
  
  for (int j = minidx; j < maxidx; j++ ) {
    // Pick up current time exposure
    CameraMeta cam_info;
    remote_ptk.get_camera(j, cam_info);

    double prv_exposure_time = cam_info.exposure_t();

    // Deciding working area
    if ( opt.level < 0 )
      opt.level = drg_plate->num_levels() - 1;
    int32 full = 1 << opt.level;
    BBox2i affected_tiles(0,0,full,full);
    std::list<TileHeader> drg_tiles =
      drg_plate->search_by_region(opt.level, affected_tiles,
                                  TransactionRange(j+1,j+1));
    ImageView<PixelGrayA<float32> > drg_temp, albedo_temp;

    if ( project_info.reflectance() == ProjectMeta::NONE ) {
      // Accumulating time exposure
      TimeDeltaNRAccumulator taccum(cam_info.exposure_t());

      // Updating current time exposure
      BOOST_FOREACH( const TileHeader& drg_tile, drg_tiles ) {
        drg_plate->read( drg_temp, drg_tile.col(), drg_tile.row(),
                         opt.level, j+1, true );
        albedo_plate->read( albedo_temp, drg_tile.col(), drg_tile.row(),
                            opt.level, -1, false );
        for_each_pixel(drg_temp, albedo_temp, taccum);
      }
      cam_info.set_exposure_t(cam_info.exposure_t()+taccum.value());
    } else {
      vw_throw( NoImplErr() << "Sorry, reflectance code is incomplete.\n" );
      // Accumulating time exposure
      TimeDeltaAccumulator taccum(cam_info.exposure_t());

      // Updating current time exposure
    }
    if ( !opt.dry_run ) {
      remote_ptk.set_camera(j, cam_info);

      // Increment iterations
      if ( opt.job_id == 0 )
        remote_ptk.set_iteration(project_info.current_iteration()+1);
    }
    vw_out() << "Camera[" << j << "] updated exposure time: "
             << cam_info.exposure_t() << " delta "
             << cam_info.exposure_t() - prv_exposure_time << "\n";

  }
}

void handle_arguments( int argc, char *argv[], Options& opt ) {
  po::options_description general_options("");
  general_options.add_options()
    ("level,l", po::value(&opt.level)->default_value(-1), "Default is to process lowest level.")
    ("dry-run", "Don't write results")
    ("job_id,j", po::value(&opt.job_id)->default_value(0), "")
    ("num_jobs,n", po::value(&opt.num_jobs)->default_value(1), "");
  general_options.add( photk::BaseOptionsDescription(opt) );

  po::options_description positional("");
  positional.add_options()
    ("ptk_url",  po::value(&opt.ptk_url),  "Input PTK Url");

  po::positional_options_description positional_desc;
  positional_desc.add("ptk_url", 1);

  std::ostringstream usage;
  usage << "Usage: " << argv[0] << " <ptk-url>\n";

  po::variables_map vm =
    photk::check_command_line( argc, argv, opt, general_options,
                             positional, positional_desc, usage.str() );

  opt.dry_run = vm.count("dry-run");

  if ( opt.ptk_url == Url() )
    vw_throw( ArgumentErr() << "Missing project file url!\n"
              << usage.str() << general_options );
}

int main( int argc, char *argv[] ) {

  Options opt;
  try {
    handle_arguments( argc, argv, opt );
    update_exposure( opt );
  } PHOTK_STANDARD_CATCHES;

  return 0;
}