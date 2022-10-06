/*
//@HEADER
// ************************************************************************
//
//                        Kokkos v. 3.0
//       Copyright (2020) National Technology & Engineering
//               Solutions of Sandia, LLC (NTESS).
//
// Under the terms of Contract DE-NA0003525 with NTESS,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY NTESS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NTESS OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Siva Rajamanickam (srajama@sandia.gov)
//
// ************************************************************************
//@HEADER
*/

#include "KokkosKernels_Utils.hpp"
#include "KokkosSparse_CcsMatrix.hpp"
#include "KokkosSparse_CrsMatrix.hpp"

#ifndef _KOKKOSSPARSE_CRS2CCS_HPP
#define _KOKKOSSPARSE_CRS2CCS_HPP
namespace KokkosSparse {
namespace Impl {
template <class OrdinalType, class SizeType, class ValViewType,
          class RowMapViewType, class ColIdViewType>
class Crs2Ccs {
 private:
  using CcsST             = typename ValViewType::value_type;
  using CcsOT             = OrdinalType;
  using CcsET             = typename ValViewType::execution_space;
  using CcsMT             = void;
  using CcsSzT            = SizeType;
  using CcsType           = CcsMatrix<CcsST, CcsOT, CcsET, CcsMT, CcsSzT>;
  using CcsValsViewType   = typename CcsType::values_type;
  using CcsColMapViewType = typename CcsType::col_map_type::non_const_type;
  using CcsRowIdViewType  = typename CcsType::index_type;

  OrdinalType __nrows;
  OrdinalType __ncols;
  SizeType __nnz;
  ValViewType __vals;
  RowMapViewType __row_map;
  ColIdViewType __col_ids;

  CcsValsViewType __ccs_vals;
  CcsColMapViewType __ccs_col_map;
  CcsRowIdViewType __ccs_row_ids;

 public:
  Crs2Ccs(OrdinalType nrows, OrdinalType ncols, SizeType nnz, ValViewType vals,
          RowMapViewType row_map, ColIdViewType col_ids)
      : __nrows(nrows),
        __ncols(ncols),
        __nnz(nnz),
        __vals(vals),
        __row_map(row_map),
        __col_ids(col_ids) {
    __ccs_vals = CcsValsViewType(
        Kokkos::view_alloc(Kokkos::WithoutInitializing, "__ccs_vals"), nnz);
    __ccs_col_map =
        CcsColMapViewType(Kokkos::view_alloc("__ccs_col_map"), ncols + 1);
    __ccs_row_ids = CcsRowIdViewType(
        Kokkos::view_alloc(Kokkos::WithoutInitializing, "__ccs_row_ids"), nnz);

    KokkosSparse::Impl::transpose_matrix<
        RowMapViewType, ColIdViewType, ValViewType, CcsColMapViewType,
        CcsRowIdViewType, CcsValsViewType, CcsColMapViewType, CcsET>(
        __nrows, __ncols, __row_map, __col_ids, __vals, __ccs_col_map,
        __ccs_row_ids, __ccs_vals);
  }

  CcsType get_ccsMat() {
    return CcsType("crs2ccs", __nrows, __ncols, __nnz, __ccs_vals,
                   __ccs_col_map, __ccs_row_ids);
  }
};
}  // namespace Impl
// clang-format off
///
/// \brief Blocking function that converts a CrsMatrix to a CcsMatrix.
/// Crs values are copied from row-contiguous layout into column-contiguous layout.
/// \tparam OrdinalType The view value type associated with the RowIdViewType
/// \tparam SizeType The type of nnz
/// \tparam ValViewType    The values view type
/// \tparam RowMapViewType The column map view type
/// \tparam ColIdViewType  The row ids view type
/// \param nrows   The number of rows in the crs matrix
/// \param ncols   The number of columns in the crs matrix
/// \param nnz     The number of non-zeros in the crs matrix
/// \param vals    The values view of the crs matrix
/// \param row_map The row map view of the crs matrix
/// \param col_ids The col ids view of the crs matrix
/// \return A KokkosSparse::CcsMatrix.
///
/// \note In KokkosKernels sparse code, adj stands for adjacency list
///   and here we're passing in a crs matrix with xadj=row_map and adj=col_ids.
// clang-format on
template <class OrdinalType, class SizeType, class ValViewType,
          class RowMapViewType, class ColIdViewType>
auto crs2ccs(OrdinalType nrows, OrdinalType ncols, SizeType nnz,
             ValViewType vals, RowMapViewType row_map, ColIdViewType col_ids) {
  using Crs2ccsType = Impl::Crs2Ccs<OrdinalType, SizeType, ValViewType,
                                    RowMapViewType, ColIdViewType>;
  Crs2ccsType crs2Ccs(nrows, ncols, nnz, vals, row_map, col_ids);
  return crs2Ccs.get_ccsMat();
}

///
/// @brief Blocking function that converts a crs matrix to a CcsMatrix.
/// Crs values are copied from row-contiguous layout into column-contiguous
/// layout.
///
/// \tparam ScalarType   The crsMatrix::scalar_type
/// \tparam OrdinalType  The crsMatrix::ordinal_type
/// \tparam Device       The crsMatrix::device_type
/// \tparam MemoryTraits The crsMatrix::memory_traits
/// \tparam SizeType     The crsMatrix::size_type
/// \param crsMatrix The KokkosSparse::CrsMatrix.
/// \return A KokkosSparse::CcsMatrix.
template <typename ScalarType, typename OrdinalType, class DeviceType,
          class MemoryTraitsType, typename SizeType>
auto crs2ccs(KokkosSparse::CrsMatrix<ScalarType, OrdinalType, DeviceType,
                                     MemoryTraitsType, SizeType> &crsMatrix) {
  return crs2ccs(crsMatrix.numRows(), crsMatrix.numCols(), crsMatrix.nnz(),
                 crsMatrix.values, crsMatrix.graph.row_map,
                 crsMatrix.graph.entries);
}
}  // namespace KokkosSparse

#endif  //  _KOKKOSSPARSE_CRS2CCS_HPP