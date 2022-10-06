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

#ifndef _KOKKOSSPARSE_CCS2CRS_HPP
#define _KOKKOSSPARSE_CCS2CRS_HPP
namespace KokkosSparse {
namespace Impl {
template <class OrdinalType, class SizeType, class ValViewType,
          class ColMapViewType, class RowIdViewType>
class Ccs2Crs {
 private:
  using CrsST             = typename ValViewType::value_type;
  using CrsOT             = OrdinalType;
  using CrsET             = typename ValViewType::execution_space;
  using CrsMT             = void;
  using CrsSzT            = SizeType;
  using CrsType           = CrsMatrix<CrsST, CrsOT, CrsET, CrsMT, CrsSzT>;
  using CrsValsViewType   = typename CrsType::values_type;
  using CrsRowMapViewType = typename CrsType::row_map_type::non_const_type;
  using CrsColIdViewType  = typename CrsType::index_type;

  OrdinalType __nrows;
  OrdinalType __ncols;
  SizeType __nnz;
  ValViewType __vals;
  ColMapViewType __col_map;
  RowIdViewType __row_ids;

  RowIdViewType __crs_row_cnt;

  CrsValsViewType __crs_vals;
  CrsRowMapViewType __crs_row_map;
  CrsColIdViewType __crs_col_ids;

 public:
  Ccs2Crs(OrdinalType nrows, OrdinalType ncols, SizeType nnz, ValViewType vals,
          ColMapViewType col_map, RowIdViewType row_ids)
      : __nrows(nrows),
        __ncols(ncols),
        __nnz(nnz),
        __vals(vals),
        __col_map(col_map),
        __row_ids(row_ids) {
    __crs_vals = CrsValsViewType(
        Kokkos::view_alloc(Kokkos::WithoutInitializing, "__crs_vals"), nnz);
    __crs_row_map =
        CrsRowMapViewType(Kokkos::view_alloc("__crs_row_map"), nrows + 1);
    __crs_col_ids = CrsColIdViewType(
        Kokkos::view_alloc(Kokkos::WithoutInitializing, "__crs_col_ids"), nnz);

    KokkosSparse::Impl::transpose_matrix<
        ColMapViewType, RowIdViewType, ValViewType, CrsRowMapViewType,
        CrsColIdViewType, CrsValsViewType, CrsRowMapViewType, CrsET>(
        __ncols, __nrows, __col_map, __row_ids, __vals, __crs_row_map,
        __crs_col_ids, __crs_vals);
  }

  CrsType get_crsMat() {
    return CrsType("ccs2crs", __nrows, __ncols, __nnz, __crs_vals,
                   __crs_row_map, __crs_col_ids);
  }
};
}  // namespace Impl
// clang-format off
///
/// \brief Blocking function that converts a ccs matrix to a CrsMatrix.
/// Ccs values are copied from column-contiguous layout into row-contiguous layout.
/// \tparam OrdinalType    The view value type associated with the RowIdViewType
/// \tparam SizeType       The type of nnz
/// \tparam ValViewType    The values view type
/// \tparam ColMapViewType The column map view type
/// \tparam RowIdViewType  The row ids view type
/// \param nrows   The number of rows in the ccs matrix
/// \param ncols   The number of columns in the ccs matrix
/// \param nnz     The number of non-zeros in the ccs matrix
/// \param vals    The values view of the ccs matrix
/// \param col_map The column map view of the ccs matrix
/// \param row_ids The row ids view of the ccs matrix
/// \return A KokkosSparse::CrsMatrix.
///
/// \note In KokkosKernels sparse code, adj stands for adjacency list
///   and here we're passing in a ccs matrix with xadj=col_map and adj=row_ids.
// clang-format on
template <class OrdinalType, class SizeType, class ValViewType,
          class ColMapViewType, class RowIdViewType>
auto ccs2crs(OrdinalType nrows, OrdinalType ncols, SizeType nnz,
             ValViewType vals, ColMapViewType col_map, RowIdViewType row_ids) {
  using Ccs2crsType = Impl::Ccs2Crs<OrdinalType, SizeType, ValViewType,
                                    ColMapViewType, RowIdViewType>;
  Ccs2crsType ccs2Crs(nrows, ncols, nnz, vals, col_map, row_ids);
  return ccs2Crs.get_crsMat();
}

///
/// @brief Blocking function that converts a crs matrix to a CcsMatrix.
/// Ccs values are copied from column-contiguous layout into row-contiguous
/// layout.
///
/// \tparam ScalarType   The ccsMatrix::scalar_type
/// \tparam OrdinalType  The ccsMatrix::ordinal_type
/// \tparam Device       The ccsMatrix::device_type
/// \tparam MemoryTraits The ccsMatrix::memory_traits
/// \tparam SizeType     The ccsMatrix::size_type
/// \param ccsMatrix The KokkosSparse::CcsMatrix.
/// \return A KokkosSparse::CrsMatrix.
template <typename ScalarType, typename OrdinalType, class DeviceType,
          class MemoryTraitsType, typename SizeType>
auto ccs2crs(KokkosSparse::CcsMatrix<ScalarType, OrdinalType, DeviceType,
                                     MemoryTraitsType, SizeType> &ccsMatrix) {
  return ccs2crs(ccsMatrix.numRows(), ccsMatrix.numCols(), ccsMatrix.nnz(),
                 ccsMatrix.values, ccsMatrix.graph.col_map,
                 ccsMatrix.graph.entries);
}
}  // namespace KokkosSparse
#endif  //  _KOKKOSSPARSE_CCS2CRS_HPP
