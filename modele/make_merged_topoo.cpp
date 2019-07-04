#include <algorithm>
#include <string>
#include <iostream>

#include <tclap/CmdLine.h>
#include <boost/filesystem.hpp>

#include <everytrace.h>

#include <ibmisc/memory.hpp>
#include <ibmisc/blitz.hpp>
#include <ibmisc/filesystem.hpp>
#include <ibmisc/linear/compressed.hpp>
#include <spsparse/eigen.hpp>

#include <icebin/GCMRegridder.hpp>
#include <icebin/modele/grids.hpp>
#include <icebin/modele/hntr.hpp>
#include <icebin/modele/global_ec.hpp>
#include <icebin/modele/merge_topo.hpp>

using namespace netCDF;
using namespace ibmisc;
using namespace spsparse;
using namespace icebin;
using namespace icebin::modele;

static double const NaN = std::numeric_limits<double>::quiet_NaN();

/** Command-line program reads:

    a) TOPOO (TOPO on oncean grid) and EOpvAOp matrix generated for
       global ice, but with ice sheet ("local ice") removed.

    b) GCMRegridder data structure, on the Ocean grid, capable of
       providing the missing ice sheet, directly from a hi-res form.
       For example, obtained from a PISM stat file.

Produces: TOPOO and EOpvAOp in which the local ice has been merged
    into the global ice.  This will later be processed by
    make_topoa.cpp to produce ModelE input files on the Atmosphere
    grid.
*/
struct ParseArgs {
    /** Name of the TOPOO file generated from the base ice.  It should
    be MISSING the ice sheets that will be provided by gcmO_fname.
    NOTE: `_ng` means "no Greenland" i.e. one or more ice sheets has
          been removed. */
    std::string topoo_ng_fname;

    /** Name of file out which the base EvA matrix (for global ice)
    will be loaded.  It should be MISSING the ice sheets that will be
    provided by gcmO_fname.
    NOTE: `_ng` means "no Greenland" i.e. one or more ice sheets has
          been removed. */
    std::string global_ecO_ng_fname;

    /** Name of file out of which the GCMRegirdder (ocean grid) for
    the local ice sheets will be loaded. */
    std::string gcmO_fname;

    /** Name of the files out of which the elevmaskI for each ice
    sheet will be loaded.  They must be in the same order as found in
    gcmO_fname.  Each filename is form of <format>:<fname>, allowing
    this program to know how to load and interpret the information in
    the file.  Currently, the only format is `pism:`; in which
    state files written by PISM are read. */
    std::vector<std::string> elevmask_xfnames;

    /** Should elevation classes between global and local ice be merged?
    This is desired when running without two-way coupling. */
    bool squash_ec;

    /** Output filename; the merged TOPOO and merged EOpvAOp matrices
    are written to this file. */
    std::string topoo_merged_fname;

    /** Radius of the earth to use when needed. */
    double eq_rad;

    ParseArgs(int argc, char **argv);
};

ParseArgs::ParseArgs(int argc, char **argv)
{
    // Wrap everything in a try block.  Do this every time, 
    // because exceptions will be thrown for problems.
    try {  
        TCLAP::CmdLine cmd("Command description message", ' ', "<no-version>");


        // ---------------- Input Filesnames
        TCLAP::ValueArg<std::string> topoo_ng_a("i", "topoo",
            "Knockout (eg Greenland-free) TOPOO file, writen by make_topoo",
            false, "topoo_ng.nc", "knockout topoo file", cmd);

        TCLAP::ValueArg<std::string> global_ecO_ng_a("c", "global_ecO",
            "Knockout (eg Greenland-free) Elevation Class Matrix file (ocean grid)",
            false, "global_ecO_ng.nc", "knockout matrix file", cmd);

        TCLAP::ValueArg<std::string> gcmO_ng_a("g", "gcmO",
            "File containing the GCMRegridder representing all ice sheets to be merged in (ocean grid)",
            false, "gcmO.nc", "GCMRegridder description", cmd);

        TCLAP::ValueArg<bool> squash_ec_a("s", "squash_ec",
            "Merge elevation classes between global and local ice?",
            false, true, "bool", cmd);

        TCLAP::MultiArg<std::string> elevmask_a("e", "elevmask",
            "<Source file for ice sheet elevation and maks>",
            false, "[type]:[fname]", cmd);

        TCLAP::ValueArg<double> eq_rad_a("R", "radius",
            "Radius of the earth",
            false, modele::EQ_RAD, "earth radius", cmd);


        // --------------- Output filenames
        TCLAP::ValueArg<std::string> topoo_merged_a("o", "topoo_merged",
            "Merged TOPOO file to write",
            false, "topoo_merged.nc", "output topoo file", cmd);

        // Parse the argv array.
        cmd.parse( argc, argv );

        // Extract values from TCLAP data structures.
        topoo_ng_fname = topoo_ng_a.getValue();
        global_ecO_ng_fname = global_ecO_ng_a.getValue();
        gcmO_fname = gcmO_ng_a.getValue();
        squash_ec = squash_ec_a.getValue();
        elevmask_xfnames = elevmask_a.getValue();
        topoo_merged_fname = topoo_merged_a.getValue();
    } catch (TCLAP::ArgException &e) { // catch any exceptions
        std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
        exit(1);
    }
}


int main(int argc, char **argv)
{
    everytrace_init();
    ParseArgs args(argc, argv);

    // ============= Define input/output  variables
    ibmisc::ArrayBundle<double,2> topoo;

    // ------------- Non-rounded versions (Op)
    auto &foceanOp(topoo.add("FOCEANF", {
        "description", "Fractional ocean ocver",
        "units", "1",
        "sources", "GISS 1Qx1",
    }));
    auto &fgiceOp(topoo.add("FGICEF", {
        "description", "Glacial Ice Surface Fraction (Ocean NOT rounded)",
        "units", "0:1",
        "sources", "GISS 1Qx1",
    }));
    auto &zatmoOp(topoo.add("ZATMOF", {
        "description", "Atmospheric Topography",
        "units", "m",
        "sources", "ETOPO2 1Qx1",
    }));

    // ------------ Rounded Versions (Om)
    auto &foceanOm(topoo.add("FOCEAN", {
        "description", "0 or 1, Bering Strait 1 cell wide",
        "units", "1",
        "source", "GISS 1Qx1",
    }));
    auto &flakeOm(topoo.add("FLAKE", {
        "description", "Lake Surface Fraction",
        "units", "0:1",
        "sources", "GISS 1Qx1",
    }));
    auto &fgrndOm(topoo.add("FGRND", {
        "description", "Ground Surface Fraction",
        "units", "0:1",
        "sources", "GISS 1Qx1",
    }));
    auto &fgiceOm(topoo.add("FGICE", {
        "description", "Glacial Ice Surface Fraction",
        "units", "0:1",
        "sources", "GISS 1Qx1",
    }));
    auto &zatmoOm(topoo.add("ZATMO", {
        "description", "Atmospheric Topography",
        "units", "m",
        "sources", "ETOPO2 1Qx1",
    }));
    auto &zlakeOm(topoo.add("ZLAKE", {
        "description", "Lake Surface Topography",
        "units", "m",
        "sources", "ETOPO2 1Qx1",
    }));
    auto &zicetopO(topoo.add("ZICETOP", {
        "description", "Atmospheric Topography (Ice-Covered Regions Only)",
        "units", "m",
        "sources", "ETOPO2 1Qx1",
    }));


    // ================================== Read Input Files

    // Read metadata and global EOpvAOp matrix (from output of global_ec.cpp)
    global_ec::Metadata metaO;
    ibmisc::ZArray<int,double,2> EOpvAOp_ng;
    {NcIO ncio(args.global_ecO_ng_fname, 'r');
    linear::Weighted_Compressed EOpvAOp_s;    // sparse indexing
        metaO.ncio(ncio);
        EOpvAOp_ng.ncio(ncio, "EvA.M");
    }
    HntrSpec &hspecO(metaO.hspecA);
    // HntrSpec hspecA(make_hntrA(hspecO));
    // Indexing &indexingHCO(metaO.indexingHC);
    // Indexing indexingHCA({"A", "HC"}, {0,0}, {hspecA.size(), indexingHCO[1].extent}, {1,0});

    // Read TOPOO input (global ice)
    {NcIO topoo_nc(args.topoo_ng_fname, 'r');

        // Read from topoO file, and allocate resulting arrays.
        topoo.ncio_alloc(topoo_nc, {}, "", "double",
            get_or_add_dims(topoo_nc, {"jm", "im"}, {hspecO.jm, hspecO.im}));
    }

    // Read the GCMRegridder
    GCMRegridder_Standard gcmO;
    {NcIO gcmO_nc(args.gcmO_fname, 'r');
        gcmO.ncio(gcmO_nc, "m");
    }

    // Read per-ice sheet elevmasks (for land+ice and ice only)
    std::vector<blitz::Array<double,1>> emI_lands, emI_ices;
    for (auto const &xfname : args.elevmask_xfnames) {
        // Parse each spec of the form <format>:<fname>
        int colon = xfname.find(':');
        if (colon < 0) (*icebin_error)(-1,
            "elevmask spec '%s' must be in the format of type:fname", xfname.c_str());

        std::string stype(xfname.substr(0, colon));
        std::string spec (xfname.substr(colon+1));

        // Dispatch to the read method, based on format.
        blitz::Array<double,1> emI_land, emI_ice;
        if (stype == "pism") {
            read_elevmask_pism(spec, 0, emI_land, emI_ice);
        } else {
            (*icebin_error)(-1,
                "Unrecognized elevmask spec type %s", stype.c_str());
        }

        // Store results
        emI_lands.push_back(emI_land);
        emI_ices.push_back(emI_ice);
    }

    std::vector<std::string> errors;

    // We need correctA=true here to get FOCEANF, etc.
    merge_topoO(
        foceanOp, fgiceOp, zatmoOp,
        foceanOm, flakeOm, fgrndOm, fgiceOm, zatmoOm, zicetopO, &gcmO,
        RegridParams(false, true, {0.,0.,0.}),  // (scale, correctA, sigma)
        emI_lands, emI_ices, args.eq_rad, errors);

    SparseSetT dimAOp;
    EOpvAOpResult eam(compute_EOpvAOp_merged(
        dimAOp, EOpvAOp_ng,
        RegridParams(false, false, {0.,0.,0.}),  // (scale, correctA, sigma)
        &gcmO, args.eq_rad, emI_ices,
        true, true,    // use_global_ice=t, use_local_ice=t
        metaO.hcdefs, metaO.indexingHC, args.squash_ec, errors));


    // Print sanity check errors to STDERR
    for (std::string const &err : errors) fprintf(stderr, "ERROR: %s\n", err.c_str());

    // ================== Write output
    // Write all inputs to a single output file
    ZArray<int,double,2> EOpvAOp_c({eam.dimEOp.sparse_extent(), dimAOp.sparse_extent()});
    {NcIO ncio(args.topoo_merged_fname, 'w');

        // Write Ocean grid metadata
        metaO.hspecA.ncio(ncio, "hspecA");    // Actually ocean grid


        eam.indexingHC.ncio(ncio, "indexingHC");
        auto xxdims(get_or_add_dims(ncio, {"nhc"}, {eam.hcdefs.size()}));
        ncio_vector(ncio, eam.hcdefs, true, "hcdefs", "double", xxdims);
        ncio_vector(ncio, eam.underice_hc, true, "underice_hc", "short", xxdims);

        // Compress and Write EOpvAOp; our merged EOpvAOp needs to be
        // in the same (compressed) format as the original base
        // EOpvAOp that we read.
        {auto EOpvAOp_a(EOpvAOp_c.accum());
            for (auto ii=begin(*eam.EOpvAOp); ii != end(*eam.EOpvAOp); ++ii) {
                EOpvAOp_a.add({
                    eam.dimEOp.to_sparse(ii->index(0)),
                    dimAOp.to_sparse(ii->index(1))},
                    ii->value());
            }
        }    // Flush compression on ~EOpvAOp_a()

        // We write just the main matrix; but not the other things involved in
        // linear::Weighted_Compressed.
        EOpvAOp_c.ncio(ncio, "EvA.M");

        // Write out all the TOPOO items
        topoo.ncio(ncio, {}, "", "double",
            get_or_add_dims(ncio, {"jm", "im"}, {hspecO.jm, hspecO.im}));

    }

    if (errors.size() > 0) return -1;
    return 0;
}
