/*
 * IceBin: A Coupling Library for Ice Models and GCMs
 * Copyright (c) 2013-2016 by Elizabeth Fischer
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ICEBIN_SPARSE_HPP
#define ICEBIN_SPARSE_HPP

#include <spsparse/VectorCooArray.hpp>

namespace icebin {

/** The sparse vector and matrix data types we'll use in IceBin. */
typedef spsparse::VectorCooArray<long, double, 2> SparseMatrix;
typedef spsparse::VectorCooArray<long, double, 1> SparseVector;


struct WeightedSparse {
    SparseMatrix M;
    SparseVector weight;

    WeightedSparse() {}
    WeightedSparse(std::array<size_t, 2> const &shape) : M(shape), weight({shape[0]}) {}
};


}
#endif
