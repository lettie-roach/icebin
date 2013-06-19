#include <boost/bind.hpp>
#include <cstdlib>
#include <glint2/Grid_XY.hpp>
#include <glint2/clippers.hpp>
//#include <glint2/constants.hpp>
#include <boost/filesystem.hpp>

static const double km = 1000.0;

using namespace glint2;

int main(int argc, char **argv)
{
	if (argc < 2) {
		printf("Usage: %s <gridbox-size>\n   eg: searise 5\n", argv[0]);
		return 0;
	}

	int size = atoi(argv[1]);
	double dsize = (double)size;

	printf("------------- Set up the local ice grid\n");

	Grid_XY grid;
	grid.name = "searise";
	grid.sproj = "+proj=stere +lon_0=-39 +lat_0=90 +lat_ts=71.0 +ellps=WGS84";

	// The true exact SeaRISE grid
	set_xy_boundaries(grid,
		(- 800.0 - .5*dsize)*km, (- 800.0 + 300.0*5 + .5*dsize)*km,   size*km,
		(-3400.0 - .5*dsize)*km, (-3400.0 + 560.0*5 + .5*dsize)*km,   size*km);

	grid.realize(boost::bind(&EuclidianClip::keep_all, _1));

	printf("Ice grid has %ld cells\n", grid.ncells_full());

	// ------------- Write it out to NetCDF
	fflush(stdout);
	printf("// ------------- Write it out to NetCDF\n");
//	boost::filesystem::path exe_path(argv[0]);
//	grid.to_netcdf(exe_path.stem().string() + ".nc");
	char fname[100];
	sprintf(fname, "searise_g%d.nc", size);
	grid.to_netcdf(std::string(fname));
}
