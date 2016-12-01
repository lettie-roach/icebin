#pragma once

#include <cstdlib>

#include <ibmisc/VarTransformer.hpp>
#include <ibmisc/DynArray.hpp>
#include <ibmisc/udunits2.hpp>
#include <ibmisc/ConstantSet.hpp>

#include <icebin/GCMParams.hpp>
#include <icebin/GCMPerIceSheetParams.hpp>
#include <icebin/GCMRegridder.hpp>
#include <icebin/VarSet.hpp>

namespace icebin {

class GCMCoupler;
class IceWriter;

class IceCoupler {
public:
    BOOST_ENUM_VALUES( Type, int,
        (DISMAL)        (0)     // Demo Ice Sheet Model and LandIce
        (PISM)          (1)
        (ISSM)          (2)
        (WRITER)        (3)
    );
    const IceCoupler::Type type;

    /** Ordered specification of the variables (w/ units)
    to be passed IceBin->IceCoupler and IceCoupler->IceBin */
    enum IO {INPUT, OUTPUT, _count};

public:
    GCMCoupler const *gcm_coupler;      // parent back-pointer
    IceRegridder *regridder;   // This is not const; see IceCoupler::update_elevI()
    std::unique_ptr<WeightedSparse> IvE;    // Regridding matrix made from regridder

    EigenSparseMatrix IvE0;
    SparseSet dimE0;


    // [INPUT|OUTPUT] variables
    // List of fields this dynamic ice model takes for input / output.
    std::array<VarSet, 2> contract;

    // Linear combination transforming variables from:
    //      INPUT: gcm_output --> ice_input
    //     OUTPUT: ice_output --> gcm_input
    // (eg: T_ice = T_gcm + 273.15)
    std::array<ibmisc::VarTransformer, 2> var_transformer;

    // Writers called to record the input and output seen by this IceCoupler
    std::array<std::unique_ptr<IceWriter>, 2> writer;

    // Parameters provided by the GCM, to inform the coupling
    std::unique_ptr<GCMPerIceSheetParams> gcm_per_ice_sheet_params;

public:
    std::string const &name() { return regridder->name(); }
    Grid const *gridI() { return &*regridder->gridI; }
    long ndata() const { return regridder->gridI->ndata(); }

    // ======================================================

    IceCoupler(IceCoupler::Type _type) : type(_type) {}
    virtual ~IceCoupler();

public:
    // Lifecycle

    /** (1) Initialize any grid information, etc. from the IceSheet struct.
    @param vname_base Construct variable name from this, out of which to pull parameters from netCDF */
    virtual void ncread(ibmisc::NcIO &ncio, std::string const &vname_sheet) {}

    /** (2) Event handler to let IceCouplers know the start time is (finally) set */
    virtual void set_start_time(
        ibmisc::time::tm const &time_base,
        double time_start_s);

    /** (3) Sets elevI based on the latest state from the ice model. */
    virtual blitz::Array<double,1> get_elevI() = 0;

    /** (4) Run the ice model for one coupling timestep.
    @param time_s Seconds since GCMParams::time_base.  Helps with debugging.
    @param index Index of each input grid value in ivalsI.
    @param ivalsI The values themselves (sparse representation).
           Their meaning (SMB, T, etc) is determined
           by the place in the array, as specified by the appropriate
           INPUT contract for this ice model.
    */
    void couple(
        double time_s,
        // Values from GCM, passed GCM -> Ice
        ArraySparseParallelVectors const &gcm_ovalsE,
        GCMCoupleOutput &out,    // Accumulate matrices here...
        bool do_run);

    /** (4.1) @param index Index of each grid value.
    @param time_s Time since start of simulation, in seconds
    @param do_run True if we are to actually run (otherwise just return ice_ovalsI from current state) */
    virtual void run_timestep(double time_s,
        blitz::Array<int,2> const &ice_ivalsI,
        blitz::Array<int,2> const &ice_ovalsI,
        bool do_run) = 0;

};      // class IceCoupler
// =========================================================

extern
std::unique_ptr<IceCoupler> new_ice_coupler(NcIO &ncio, std::string vname,
    GCMCoupler const *_gcm_coupler, IceRegridder *_regridder);


// =========================================================
class IceWriter
{
    /** Description of the fields we're writing */
    VarSet const *contract;

    // The output file we are writing to...
    std::string fname;

    // Dimensions to use when writing to netCDF
    std::vector<std::string> dim_names;
    std::vector<size_t> cur;        // Base index to write in netCDF
    std::vector<size_t> counts;
    std::vector<size_t> strides;

public:
    void IceWriter(
        IceCoupler &_ice_coupler,
        VarSet const *_contract,
        std::string _output_fname);

    void IceWriter::write(double time_s,
        blitz::Array<double,2> const &valsI);    // valsI[nI, nvars]

private:
    void init_output_file();

};

}