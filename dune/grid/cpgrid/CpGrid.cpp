//===========================================================================
//
// File: CpGrid.cpp
//
// Created: Thu Jun  4 12:55:28 2009
//
// Author(s): Atgeirr F Rasmussen <atgeirr@sintef.no>
//            B�rd Skaflestad     <bard.skaflestad@sintef.no>
//
// $Date$
//
// $Revision$
//
//===========================================================================

/*
  Copyright 2009, 2010 SINTEF ICT, Applied Mathematics.
  Copyright 2009, 2010 Statoil ASA.

  This file is part of The Open Porous Media project  (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if HAVE_MPI
#include "mpi.h"
#endif
#include "../CpGrid.hpp"
#include "CpGridData.hpp"
#include <dune/grid/common/GridPartitioning.hpp>
#include <opm/core/utility/parameters/ParameterGroup.hpp>

#include <fstream>
#include <iostream>

namespace Dune
{

    CpGrid::CpGrid()
        : data_( new cpgrid::CpGridData(*this)),
          current_view_data_(data_.get()),
          distributed_data_()
    {}

    /// Initialize the grid.
    void CpGrid::init(const Opm::parameter::ParameterGroup& param)
    {
        std::string fileformat = param.get<std::string>("fileformat");
        if (fileformat == "sintef_legacy") {
            std::string grid_prefix = param.get<std::string>("grid_prefix");
            readSintefLegacyFormat(grid_prefix);
        } else if (fileformat == "eclipse") {
            std::string filename = param.get<std::string>("filename");
            if (param.has("z_tolerance")) {
                std::cerr << "****** Warning: z_tolerance parameter is obsolete, use PINCH in deck input instead\n";
            }
            bool periodic_extension = param.getDefault<bool>("periodic_extension", false);
            bool turn_normals = param.getDefault<bool>("turn_normals", false);
            readEclipseFormat(filename, periodic_extension, turn_normals);
        } else if (fileformat == "cartesian") {
            array<int, 3> dims = {{ param.getDefault<int>("nx", 1),
                                    param.getDefault<int>("ny", 1),
                                    param.getDefault<int>("nz", 1) }};
            array<double, 3> cellsz = {{ param.getDefault<double>("dx", 1.0),
                                         param.getDefault<double>("dy", 1.0),
                                         param.getDefault<double>("dz", 1.0) }};
            createCartesian(dims, cellsz);
        } else {
            OPM_THROW(std::runtime_error, "Unknown file format string: " << fileformat);
        }
    }


bool CpGrid::scatterGrid(int overlapLayers)
{
#if HAVE_MPI && DUNE_VERSION_NEWER(DUNE_GRID, 2, 3)
    if(distributed_data_)
    {
        std::cerr<<"There is already a distributed version of the grid."
                 << " Maybe scatterGrid was called before?"<<std::endl;
        return false;
    }

    CollectiveCommunication cc(MPI_COMM_WORLD);

    std::vector<int> cell_part(current_view_data_->global_cell_.size());
    int my_num=cc.rank();
    int  num_parts=-1;
    std::array<int, 3> initial_split;
    initial_split[1]=initial_split[2]=std::pow(cc.size(), 1.0/3.0);
    initial_split[0]=cc.size()/(initial_split[1]*initial_split[2]);
    partition(*this, initial_split, num_parts, cell_part);


    MPI_Comm new_comm = MPI_COMM_NULL;

    if(num_parts < cc.size())
    {
        std::vector<int> ranks(num_parts);
        for(int i=0; i<num_parts; ++i)
            ranks[i]=i;
        MPI_Group new_group;
        MPI_Group old_group;
        MPI_Comm_group(cc, &old_group);
        MPI_Group_incl(old_group, num_parts, &(ranks[0]), &new_group);

        // Not all procs take part in the parallel computation
        MPI_Comm_create(cc, new_group, &new_comm);
        cc=CollectiveCommunication(new_comm);
    }else{
        new_comm = cc;
    }
    if(my_num<cc.size())
    {
        distributed_data_.reset(new cpgrid::CpGridData(new_comm));
        distributed_data_->distributeGlobalGrid(*this,*this->current_view_data_, cell_part,
                                                overlapLayers);
    }
    current_view_data_ = distributed_data_.get();
    return true;
#else // #if HAVE_MPI && DUNE_VERSION_NEWER(DUNE_GRID, 2, 3)
    static_cast<void>(overlapLayers);
    std::cerr << "CpGrid::scatterGrid() is non-trivial only with "
              << "MPI support and if the target Dune platform is "
              << "sufficiently recent.\n";
    return false;
#endif
}


    void CpGrid::createCartesian(const array<int, 3>& dims,
                                 const array<double, 3>& cellsize)
    {
        // Make the grdecl format arrays.
        // Pillar coords.
        std::vector<double> coord;
        coord.reserve(6*(dims[0] + 1)*(dims[1] + 1));
        double bot = 0.0;
        double top = dims[2]*cellsize[2];
        // i runs fastest for the pillars.
        for (int j = 0; j < dims[1] + 1; ++j) {
            double y = j*cellsize[1];
            for (int i = 0; i < dims[0] + 1; ++i) {
                double x = i*cellsize[0];
                double pillar[6] = { x, y, bot, x, y, top };
                coord.insert(coord.end(), pillar, pillar + 6);
            }
        }
        std::vector<double> zcorn(8*dims[0]*dims[1]*dims[2]);
        const int num_per_layer = 4*dims[0]*dims[1];
        double* offset = &zcorn[0];
        for (int k = 0; k < dims[2]; ++k) {
            double zlow = k*cellsize[2];
            std::fill(offset, offset + num_per_layer, zlow);
            offset += num_per_layer;
            double zhigh = (k+1)*cellsize[2];
            std::fill(offset, offset + num_per_layer, zhigh);
            offset += num_per_layer;
        }
        std::vector<int> actnum(dims[0]*dims[1]*dims[2], 1);

        // Process them.
        grdecl g;
        g.dims[0] = dims[0];
        g.dims[1] = dims[1];
        g.dims[2] = dims[2];
        g.coord = &coord[0];
        g.zcorn = &zcorn[0];
        g.actnum = &actnum[0];
        current_view_data_->processEclipseFormat(g, 0.0, false, false);
    }

    void CpGrid::readSintefLegacyFormat(const std::string& grid_prefix)
    {
        current_view_data_->readSintefLegacyFormat(grid_prefix);
    }
    void CpGrid::writeSintefLegacyFormat(const std::string& grid_prefix) const
    {
        current_view_data_->writeSintefLegacyFormat(grid_prefix);
    }
    void CpGrid::readEclipseFormat(const std::string& filename,
                                   bool periodic_extension, bool turn_normals)
    {
        current_view_data_->readEclipseFormat(filename, periodic_extension,
                                              turn_normals);
    }

    void CpGrid::processEclipseFormat(Opm::DeckConstPtr deck,
                                      bool periodic_extension,
                                      bool turn_normals, bool clip_z,
                                      const std::vector<double>& poreVolume)
    {
        current_view_data_->processEclipseFormat(deck, periodic_extension,
                                                 turn_normals, clip_z,
                                                 poreVolume);
    }

    void CpGrid::processEclipseFormat(Opm::EclipseGridConstPtr ecl_grid,
                                      bool periodic_extension,
                                      bool turn_normals, bool clip_z,
                                      const std::vector<double>& poreVolume)
    {
        current_view_data_->processEclipseFormat(ecl_grid, periodic_extension,
                                                 turn_normals, clip_z,
                                                 poreVolume);
    }

    void CpGrid::processEclipseFormat(const grdecl& input_data, double z_tolerance,
                                      bool remove_ij_boundary, bool turn_normals)
    {
        current_view_data_->processEclipseFormat(input_data, z_tolerance, remove_ij_boundary, turn_normals);
    }

} // namespace Dune
