/*
//@HEADER
// ************************************************************************
//
//               KokkosKernels 0.9: Linear Algebra and Graph Kernels
//                 Copyright 2017 Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
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
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
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
#include <Kokkos_Core.hpp>
#include <Kokkos_Atomic.hpp>
#include <impl/Kokkos_Timer.hpp>
#include <Kokkos_Sort.hpp>
#include <Kokkos_Random.hpp>
#include <Kokkos_MemoryTraits.hpp>
#include <Kokkos_Parallel_Reduce.hpp>
#include "KokkosBlas1_fill.hpp"
#include "KokkosGraph_Distance1Color.hpp"
#include "KokkosKernels_Uniform_Initialized_MemoryPool.hpp"

#ifndef _KOKKOS_PARTITIONING_IMP_HPP
#define _KOKKOS_PARTITIONING_IMP_HPP

namespace KokkosSparse{

namespace Impl{

//Fill a view such that v(i) = i
//Does the same thing as std::iota(begin, end)
template<typename View, typename Ordinal>
struct IotaFunctor
{
  IotaFunctor(View& v_) : v(v_) {}
  KOKKOS_INLINE_FUNCTION void operator()(const Ordinal i) const
  {
    v(i) = i;
  }
  View v;
};

template <typename HandleType, typename lno_row_view_t, typename lno_nnz_view_t>
struct RCM
{
  typedef typename HandleType::HandleExecSpace MyExecSpace;
  typedef typename HandleType::HandleTempMemorySpace MyTempMemorySpace;
  typedef typename HandleType::HandlePersistentMemorySpace MyPersistentMemorySpace;

  typedef typename HandleType::size_type size_type;
  typedef typename HandleType::nnz_lno_t nnz_lno_t;

  typedef typename lno_row_view_t::const_type const_lno_row_view_t;
  typedef typename lno_row_view_t::non_const_type non_const_lno_row_view_t;
  typedef typename non_const_lno_row_view_t::value_type offset_t;

  typedef typename lno_nnz_view_t::const_type const_lno_nnz_view_t;
  typedef typename lno_nnz_view_t::non_const_type non_const_lno_nnz_view_t;

  typedef typename HandleType::row_lno_temp_work_view_t row_lno_temp_work_view_t;
  typedef typename HandleType::row_lno_persistent_work_view_t row_lno_persistent_work_view_t;
  typedef typename HandleType::row_lno_persistent_work_host_view_t row_lno_persistent_work_host_view_t; //Host view type

  typedef typename HandleType::nnz_lno_temp_work_view_t nnz_lno_temp_work_view_t;
  typedef typename HandleType::nnz_lno_persistent_work_view_t nnz_lno_persistent_work_view_t;
  typedef typename HandleType::nnz_lno_persistent_work_host_view_t nnz_lno_persistent_work_host_view_t; //Host view type

  typedef nnz_lno_persistent_work_view_t nnz_view_t;
  typedef Kokkos::View<nnz_lno_t, MyTempMemorySpace, Kokkos::MemoryTraits<0>> single_view_t;
  typedef Kokkos::View<nnz_lno_t, Kokkos::HostSpace, Kokkos::MemoryTraits<0>> single_view_host_t;

  typedef Kokkos::RangePolicy<MyExecSpace> my_exec_space;

  typedef Kokkos::RangePolicy<MyExecSpace> range_policy_t ;
  typedef Kokkos::TeamPolicy<MyExecSpace> team_policy_t ;
  typedef typename team_policy_t::member_type team_member_t ;

  typedef nnz_lno_t LO;

  RCM(size_type numRows_, lno_row_view_t& rowmap_, lno_nnz_view_t& colinds_)
    : numRows(numRows_), rowmap(rowmap_), colinds(colinds_)
  {}

  nnz_lno_t numRows;
  const_lno_row_view_t rowmap;
  const_lno_nnz_view_t colinds;

  template<typename Rowmap>
  struct MaxDegreeFunctor
  {
    typedef typename std::remove_cv<typename Rowmap::value_type>::type size_type;
    MaxDegreeFunctor(Rowmap& rowmap_) : r(rowmap_) {}
    KOKKOS_INLINE_FUNCTION void operator()(const size_type i, size_type& lmax) const
    {
      size_type ideg = r(i + 1) - r(i);
      if(ideg > lmax)
        lmax = ideg;
    }
    Rowmap r;
  };

  //simple parallel reduction to find max degree in graph
  size_type find_max_degree()
  {
    size_type maxDeg = 0;
    Kokkos::parallel_reduce(range_policy_t(0, numRows), MaxDegreeFunctor<const_lno_row_view_t>(rowmap), Kokkos::Max<size_type>(maxDeg));
    MyExecSpace::fence();
    //max degree should be computed as an offset_t,
    //but must fit in a nnz_lno_t
    return maxDeg;
  }

  //radix sort keys according to their corresponding values ascending.
  //keys are NOT preserved since the use of this in RCM doesn't care about degree after sorting
  template<typename size_type, typename KeyType, typename ValueType, typename IndexType, typename member_t>
  KOKKOS_INLINE_FUNCTION static void
  radixSortKeysAndValues(KeyType* keys, KeyType* keysAux, ValueType* values, ValueType* valuesAux, IndexType n, const member_t& mem)
  {
    if(n <= 1)
      return;
    //sort 4 bits at a time
    KeyType mask = 0xF;
    bool inAux = false;
    //maskPos counts the low bit index of mask (0, 4, 8, ...)
    IndexType maskPos = 0;
    IndexType sortBits = 0;
    KeyType minKey = Kokkos::ArithTraits<KeyType>::max();
    KeyType maxKey = 0;
    Kokkos::parallel_reduce(Kokkos::ThreadVectorRange(mem, n),
    [=](size_type i, KeyType& lminkey)
    {
      if(keys[i] < lminkey)
        lminkey = keys[i];
    }, Kokkos::Min<KeyType>(minKey));
    Kokkos::parallel_reduce(Kokkos::ThreadVectorRange(mem, n),
    [=](size_type i, KeyType& lmaxkey)
    {
      if(keys[i] > lmaxkey)
        lmaxkey = keys[i];
    }, Kokkos::Max<KeyType>(maxKey));
    //apply a bias so that key range always starts at 0
    //also invert key values here for a descending sort
    Kokkos::parallel_for(Kokkos::ThreadVectorRange(mem, n),
    [=](size_type i)
    {
      keys[i] -= minKey;
    });
    KeyType upperBound = maxKey - minKey;
    while(upperBound)
    {
      upperBound >>= 1;
      sortBits++;
    }
    for(IndexType s = 0; s < (sortBits + 3) / 4; s++)
    {
      //Count the number of elements in each bucket
      IndexType count[16] = {0};
      IndexType offset[17];
      if(!inAux)
      {
        for(IndexType i = 0; i < n; i++)
        {
          count[(keys[i] & mask) >> maskPos]++;
        }
      }
      else
      {
        for(IndexType i = 0; i < n; i++)
        {
          count[(keysAux[i] & mask) >> maskPos]++;
        }
      }
      offset[0] = 0;
      //get offset as the prefix sum for count
      for(IndexType i = 0; i < 16; i++)
      {
        offset[i + 1] = offset[i] + count[i];
      }
      //now for each element in [lo, hi), move it to its offset in the other buffer
      //this branch should be ok because whichBuf is the same on all threads
      if(!inAux)
      {
        //copy from *Over to *Aux
        for(IndexType i = 0; i < n; i++)
        {
          IndexType bucket = (keys[i] & mask) >> maskPos;
          keysAux[offset[bucket + 1] - count[bucket]] = keys[i];
          valuesAux[offset[bucket + 1] - count[bucket]] = values[i];
          count[bucket]--;
        }
      }
      else
      {
        //copy from *Aux to *Over
        for(IndexType i = 0; i < n; i++)
        {
          IndexType bucket = (keysAux[i] & mask) >> maskPos;
          keys[offset[bucket + 1] - count[bucket]] = keysAux[i];
          values[offset[bucket + 1] - count[bucket]] = valuesAux[i];
          count[bucket]--;
        }
      }
      inAux = !inAux;
      mask = mask << 4;
      maskPos += 4;
    }
    //move keys/values back from aux if they are currently in aux,
    //and remove bias
    if(inAux)
    {
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(mem, n),
      [=](size_type i)
      {
        //TODO: when everything works, is safe to remove next line
        //since keys (BFS visit scores) will never be needed again
        keys[i] = keysAux[i];
        values[i] = valuesAux[i];
      });
    }
  }

  //Functor that does breadth-first search on a sparse graph.
  struct BfsFunctor
  {
    typedef Kokkos::View<nnz_lno_t**, MyTempMemorySpace, Kokkos::MemoryTraits<0>> WorkView;

    BfsFunctor(WorkView& workQueue_, WorkView& scratch_, nnz_view_t& visit_, const_lno_row_view_t& rowmap_, const_lno_nnz_view_t& colinds_, single_view_t& numLevels_, nnz_view_t threadNeighborCounts_, nnz_lno_t start_, nnz_lno_t numRows_)
      : workQueue(workQueue_), scratch(scratch_), visit(visit_), rowmap(rowmap_), colinds(colinds_), numLevels(numLevels_), threadNeighborCounts(threadNeighborCounts_), start(start_), numRows(numRows_)
    {}

    KOKKOS_INLINE_FUNCTION void operator()(const team_member_t mem) const
    {
      const nnz_lno_t LNO_MAX = Kokkos::ArithTraits<nnz_lno_t>::max();
      const nnz_lno_t NOT_VISITED = LNO_MAX;
      const nnz_lno_t QUEUED = NOT_VISITED - 1;
      int nthreads = mem.team_size();
      nnz_lno_t tid = mem.team_rank();
      auto neighborList = Kokkos::subview(scratch, tid, Kokkos::ALL());
      //active and next indicate which buffer in workQueue holds the nodes in current/next frontiers, respectively
      //active, next and visitCounter are thread-local, but always kept consistent across threads
      int active = 0;
      int next = 1;
      nnz_lno_t visitCounter = 0;
      Kokkos::single(Kokkos::PerTeam(mem),
      [=]()
      {
        workQueue(active, 0) = start;
        visit(start) = QUEUED;
      });
      nnz_lno_t activeQSize = 1;
      nnz_lno_t nextQSize = 0;
      //KK create_reverse_map() expects incoming values to start at 1
      nnz_lno_t level = 1;
      //do this until all nodes have been visited and added to a level
      while(visitCounter < numRows)
      {
        mem.team_barrier();
        //each thread works on a contiguous block of nodes in queue (for locality)
        //compute in size_t to avoid possible 32-bit overflow
        nnz_lno_t workStart = tid * activeQSize / nthreads;
        nnz_lno_t workEnd = (tid + 1) * activeQSize / nthreads;
        //the maximum work batch size (among all threads)
        //the following loop contains barriers so all threads must iterate same # of times
        nnz_lno_t maxBatch = (activeQSize + nthreads - 1) / nthreads;
        for(nnz_lno_t loop = 0; loop < maxBatch; loop++)
        {
          //this thread may not actually have anything to work on (if nthreads doesn't divide qSize)
          bool busy = loop < workEnd - workStart;
          nnz_lno_t neiCount = 0;
          nnz_lno_t process = LNO_MAX;
          if(busy)
          {
            process = workQueue(active, workStart + loop);
            offset_t rowStart = rowmap(process);
            offset_t rowEnd = rowmap(process + 1);
            //build a list of all non-visited neighbors
            for(offset_t j = rowStart; j < rowEnd; j++)
            {
              nnz_lno_t col = colinds(j);
              //use atomic here to guarantee neighbors are added to neighborList exactly once
              if(Kokkos::atomic_compare_exchange_strong<nnz_lno_t>(&visit(col), NOT_VISITED, QUEUED))
              {
                //this thread is the first to see that col needs to be queued
                neighborList(neiCount) = col;
                neiCount++;
              }
            }
          }
          threadNeighborCounts(tid) = neiCount;
          mem.team_barrier();
          size_type queueUpdateOffset = 0;
          for(nnz_lno_t i = 0; i < tid; i++)
          {
            queueUpdateOffset += threadNeighborCounts(i);
          }
          //write out all updates to next queue in parallel
          if(busy)
          {
            nnz_lno_t nextQueueIter = 0;
            for(nnz_lno_t i = 0; i < neiCount; i++)
            {
              nnz_lno_t toQueue = neighborList(i);
              visit(toQueue) = QUEUED;
              workQueue(next, nextQSize + queueUpdateOffset + nextQueueIter) = toQueue;
              nextQueueIter++;
            }
            //assign level to to process
            visit(process) = level;
          }
          nnz_lno_t totalAdded = 0;
          for(nnz_lno_t i = 0; i < nthreads; i++)
          {
            totalAdded += threadNeighborCounts(i);
          }
          nextQSize += totalAdded;
          mem.team_barrier();
        }
        //swap queue buffers
        active = next;
        next = 1 - next;
        //all threads have a consistent value of qSize here.
        //update visitCounter in preparation for next frontier
        visitCounter += activeQSize;
        activeQSize = nextQSize;
        nextQSize = 0;
        if(visitCounter < numRows && activeQSize == 0)
        {
          Kokkos::single(Kokkos::PerTeam(mem),
          [=]()
          {
            //Some nodes are unreachable from start (graph not connected)
            //Find an unvisited node to resume BFS
            for(nnz_lno_t search = numRows - 1; search >= 0; search--)
            {
              if(visit(search) == NOT_VISITED)
              {
                workQueue(active, 0) = search;
                visit(search) = QUEUED;
                break;
              }
            }
          });
          activeQSize = 1;
        }
        level++;
      }
      Kokkos::single(Kokkos::PerTeam(mem),
      [=]
      {
        numLevels() = level - 1;
      });
    }

    WorkView workQueue;
    WorkView scratch;
    nnz_view_t visit;
    const_lno_row_view_t rowmap;
    const_lno_nnz_view_t colinds;
    single_view_t numLevels;
    nnz_view_t threadNeighborCounts;
    nnz_lno_t start;
    nnz_lno_t numRows;
  };

  //parallel breadth-first search, producing level structure in (xadj, adj) form:
  //xadj(level) gives index in adj where level begins
  //also return the total number of levels
  nnz_lno_t parallel_bfs(nnz_lno_t start, nnz_view_t& xadj, nnz_view_t& adj, nnz_lno_t& maxDeg, nnz_lno_t nthreads)
  {
    //need to know maximum degree to allocate scratch space for threads
    maxDeg = find_max_degree();
    //view for storing the visit timestamps
    nnz_view_t visit("BFS visited nodes", numRows);
    const nnz_lno_t LNO_MAX = Kokkos::ArithTraits<nnz_lno_t>::max();
    const nnz_lno_t NOT_VISITED = LNO_MAX;
    KokkosBlas::fill(visit, NOT_VISITED);
    MyExecSpace::fence();
    //the visit queue
    //one of q1,q2 is active at a time and holds the nodes to process in next BFS level
    //elements which are LNO_MAX are just placeholders (nothing to process)
    Kokkos::View<nnz_lno_t**, MyTempMemorySpace, Kokkos::MemoryTraits<0>> workQueue("BFS queue (double buffered)", 2, numRows);
    nnz_view_t threadNeighborCounts("Number of nodes to queue on each thread", nthreads);
    single_view_t numLevels("# of BFS levels");
    single_view_host_t numLevelsHost("# of BFS levels");
    Kokkos::View<nnz_lno_t**, MyTempMemorySpace, Kokkos::MemoryTraits<0u>> scratch("Scratch buffer shared by threads", nthreads, maxDeg);
    Kokkos::parallel_for(team_policy_t(1, nthreads), BfsFunctor(workQueue, scratch, visit, rowmap, colinds, numLevels, threadNeighborCounts, start, numRows));
    Kokkos::deep_copy(numLevelsHost, numLevels);
    MyExecSpace::fence();
    //now that level structure has been computed, construct xadj/adj
    KokkosKernels::Impl::create_reverse_map<nnz_view_t, nnz_view_t, MyExecSpace>
      (numRows, numLevelsHost(), visit, xadj, adj);
    return numLevelsHost();
  }

  struct CuthillMcKeeFunctor
  {
    typedef Kokkos::View<offset_t*, MyTempMemorySpace, Kokkos::MemoryTraits<0u>> ScoreView;

    CuthillMcKeeFunctor(nnz_lno_t numLevels_, nnz_lno_t maxDegree_, const_lno_row_view_t& rowmap_, const_lno_nnz_view_t& colinds_, ScoreView& scores_, ScoreView& scoresAux_, nnz_view_t& visit_, nnz_view_t& xadj_, nnz_view_t& adj_, nnz_view_t& adjAux_)
      : numLevels(numLevels_), maxDegree(maxDegree_), rowmap(rowmap_), colinds(colinds_), scores(scores_), scoresAux(scoresAux_), visit(visit_), xadj(xadj_), adj(adj_), adjAux(adjAux_)
    {}

    KOKKOS_INLINE_FUNCTION void operator()(const team_member_t mem) const
    {
      int tid = mem.team_rank();
      int nthreads = mem.team_size();
      const nnz_lno_t LNO_MAX = Kokkos::ArithTraits<nnz_lno_t>::max();
      nnz_lno_t visitCounter = 0;
      for(nnz_lno_t level = 0; level < numLevels; level++)
      {
        //iterate over vertices in this level and compute
        //min predecessors (minimum-labeled vertices from previous level)
        nnz_lno_t levelOffset = xadj(level);
        nnz_lno_t levelSize = xadj(level + 1) - levelOffset;
        //compute as offset_t to avoid overflow, but the upper bound on
        //the scores is approx. numRows * maxDegree, which should be representable
        nnz_lno_t workStart = tid * levelSize / nthreads;
        nnz_lno_t workEnd = (tid + 1) * levelSize / nthreads;
        for(nnz_lno_t i = workStart; i < workEnd; i++)
        {
          nnz_lno_t process = adj(levelOffset + i);
          nnz_lno_t minNeighbor = LNO_MAX;
          offset_t rowStart = rowmap(process);
          offset_t rowEnd = rowmap(process + 1);
          for(offset_t j = rowStart; j < rowEnd; j++)
          {
            nnz_lno_t neighbor = colinds(j);
            nnz_lno_t neighborVisit = visit(neighbor);
            if(neighborVisit < minNeighbor)
              minNeighbor = neighborVisit;
          }
          scores(i) = ((offset_t) minNeighbor * (maxDegree + 1)) + (rowmap(process + 1) - rowmap(process));
        }
        mem.team_barrier();
        Kokkos::single(Kokkos::PerTeam(mem),
        [=]()
        {
          radixSortKeysAndValues<size_type, offset_t, nnz_lno_t, nnz_lno_t, team_member_t>
            (scores.data(), scoresAux.data(), adj.data() + levelOffset, adjAux.data(), levelSize, mem);
        });
        mem.team_barrier();
        //label all vertices (which are now in label order within their level)
        for(nnz_lno_t i = workStart; i < workEnd; i++)
        {
          nnz_lno_t process = adj(levelOffset + i);
          //visit counter increases with levels, so flip the range for the "reverse" in RCM
          visit(process) = visitCounter + i;
        }
        visitCounter += levelSize;
      }
    }

    nnz_lno_t numLevels;
    nnz_lno_t maxDegree;
    const_lno_row_view_t rowmap;
    const_lno_nnz_view_t colinds;
    ScoreView scores;
    ScoreView scoresAux;
    nnz_view_t visit;
    //The levels, stored in CRS format.
    //xadj stores offsets for each level, and adj stores the rows in each level.
    nnz_view_t xadj;
    nnz_view_t adj;
    nnz_view_t adjAux;
  };

  //Does the reversing in "reverse Cuthill-McKee")
  struct OrderReverseFunctor
  {
    OrderReverseFunctor(nnz_view_t& visit_, nnz_lno_t numRows_)
      : visit(visit_), numRows(numRows_)
    {}

    KOKKOS_INLINE_FUNCTION void operator()(const size_type i) const
    {
      visit(i) = numRows - visit(i) - 1;
    }
    nnz_view_t visit;
    nnz_lno_t numRows;
  };

  //breadth-first search, producing a reverse Cuthill-McKee ordering
  nnz_view_t parallel_rcm(nnz_lno_t start)
  {
    size_type nthreads = MyExecSpace::concurrency();
    if(nthreads > 64)
      nthreads = 64;
    #ifdef KOKKOS_ENABLE_CUDA
    if(std::is_same<MyExecSpace, Kokkos::Cuda>::value)
    {
      nthreads = 256;
    }
    #endif
    nnz_view_t xadj, adj;
    nnz_lno_t maxDegree = 0;
    //parallel_bfs will compute maxDegree
    auto numLevels = parallel_bfs(start, xadj, adj, maxDegree, nthreads);
    nnz_lno_t maxLevelSize = 0;
    Kokkos::parallel_reduce(range_policy_t(0, numLevels), MaxDegreeFunctor<nnz_view_t>(xadj), Kokkos::Max<nnz_lno_t>(maxLevelSize));
    //visit (to be returned) contains the RCM numberings of each row
    nnz_view_t visit("RCM labels", numRows);
    //Populate visit wth LNO_MAX so that the "min-labeled neighbor"
    //is always a node in the previous level
    const nnz_lno_t LNO_MAX = Kokkos::ArithTraits<nnz_lno_t>::max();
    KokkosBlas::fill(visit, LNO_MAX);
    //the "score" of a node is a single value that provides an ordering equivalent
    //to sorting by min predecessor and then by min degree
    //reduce nthreads to be a power of 2
    Kokkos::View<offset_t*, MyTempMemorySpace, Kokkos::MemoryTraits<0u>> scores("RCM scores for sorting", maxLevelSize);
    Kokkos::View<offset_t*, MyTempMemorySpace, Kokkos::MemoryTraits<0u>> scoresAux("RCM scores for sorting (radix sort aux)", maxLevelSize);
    nnz_view_t adjAux("RCM scores for sorting (radix sort aux)", maxLevelSize);
    MyExecSpace::fence();
    Kokkos::parallel_for(team_policy_t(1, nthreads), CuthillMcKeeFunctor(numLevels, maxDegree, rowmap, colinds, scores, scoresAux, visit, xadj, adj, adjAux));
    MyExecSpace::fence();
    //reverse the visit order (for the 'R' in RCM)
    Kokkos::parallel_for(range_policy_t(0, numRows), OrderReverseFunctor(visit, numRows));
    MyExecSpace::fence();
    return visit;
  }

  template<typename Reducer>
  struct MinDegreeRowFunctor
  {
    typedef typename Reducer::value_type Value;
    MinDegreeRowFunctor(const_lno_row_view_t& rowmap_) : rowmap(rowmap_) {}
    KOKKOS_INLINE_FUNCTION void operator()(const size_type i, Value& lval) const
    {
      size_type ideg = rowmap(i + 1) - rowmap(i);
      if(ideg < lval.val)
      {
        lval.val = ideg;
        lval.loc = i;
      }
    }
    const_lno_row_view_t rowmap;
  };

  //Find a peripheral node (one of minimal degree), suitable for starting RCM or BFS 
  nnz_lno_t find_peripheral()
  {
    typedef Kokkos::MinLoc<size_type, size_type> MinLocReducer;
    typedef typename MinLocReducer::value_type MinLocVal;
    MinLocVal v;
    Kokkos::parallel_reduce(range_policy_t(0, numRows),
        MinDegreeRowFunctor<MinLocReducer>(rowmap), MinLocReducer(v));
    MyExecSpace::fence();
    return v.loc;
  }

  nnz_view_t rcm()
  {
    nnz_lno_t periph = find_peripheral();
    //run Cuthill-McKee BFS from periph, then reverse the order
    return parallel_rcm(periph);
  }
};

template <typename HandleType, typename lno_row_view_t, typename lno_nnz_view_t>
struct ShuffleReorder
{
  typedef typename HandleType::HandleExecSpace MyExecSpace;
  typedef typename HandleType::HandleTempMemorySpace MyTempMemorySpace;
  typedef typename HandleType::HandlePersistentMemorySpace MyPersistentMemorySpace;

  typedef typename HandleType::size_type size_type;
  typedef typename HandleType::nnz_lno_t nnz_lno_t;

  typedef typename lno_row_view_t::const_type const_lno_row_view_t;
  typedef typename lno_row_view_t::non_const_type non_const_lno_row_view_t;
  typedef typename non_const_lno_row_view_t::value_type offset_t;

  typedef typename lno_nnz_view_t::const_type const_lno_nnz_view_t;
  typedef typename lno_nnz_view_t::non_const_type non_const_lno_nnz_view_t;

  typedef typename HandleType::row_lno_temp_work_view_t row_lno_temp_work_view_t;
  typedef typename HandleType::row_lno_persistent_work_view_t row_lno_persistent_work_view_t;
  typedef typename HandleType::row_lno_persistent_work_host_view_t row_lno_persistent_work_host_view_t; //Host view type

  typedef typename HandleType::nnz_lno_temp_work_view_t nnz_lno_temp_work_view_t;
  typedef typename HandleType::nnz_lno_persistent_work_view_t nnz_lno_persistent_work_view_t;
  typedef typename HandleType::nnz_lno_persistent_work_host_view_t nnz_lno_persistent_work_host_view_t; //Host view type

  typedef nnz_lno_persistent_work_view_t nnz_view_t;
  typedef Kokkos::View<nnz_lno_t, MyTempMemorySpace, Kokkos::MemoryTraits<0>> single_view_t;
  typedef Kokkos::View<nnz_lno_t, Kokkos::HostSpace, Kokkos::MemoryTraits<0>> single_view_host_t;

  typedef Kokkos::RangePolicy<MyExecSpace> my_exec_space;
  typedef Kokkos::Bitset<MyExecSpace> bitset_t;

  typedef Kokkos::RangePolicy<MyExecSpace> range_policy_t ;
  typedef Kokkos::TeamPolicy<MyExecSpace> team_policy_t ;
  typedef typename team_policy_t::member_type team_member_t ;

  ShuffleReorder(size_type numRows_, lno_row_view_t& rowmap_, lno_nnz_view_t& colinds_)
    : numRows(numRows_), rowmap(rowmap_), colinds(colinds_), randPool(0xDEADBEEF)
  {}

  nnz_lno_t numRows;
  const_lno_row_view_t rowmap;
  const_lno_nnz_view_t colinds;

  typedef Kokkos::Random_XorShift64_Pool<MyExecSpace> RandPool;
  RandPool randPool;

  struct ShuffleFunctor
  {
    ShuffleFunctor(nnz_view_t& order_, const_lno_row_view_t& row_map_, const_lno_nnz_view_t& col_inds_, nnz_lno_t clusterSize_, RandPool& randPool_)
      : order(order_), row_map(row_map_), col_inds(col_inds_), clusterSize(clusterSize_), numRows(row_map.extent(0) - 1), randPool(randPool_), nodeLocks(numRows)
    {}

    KOKKOS_INLINE_FUNCTION nnz_lno_t origToCluster(nnz_lno_t orig) const
    {
      return origToPermuted(orig) / clusterSize;
    }

    KOKKOS_INLINE_FUNCTION nnz_lno_t origToPermuted(nnz_lno_t orig) const
    {
      return order(orig);
    }

    struct SharedData
    {
      nnz_lno_t node1;
      nnz_lno_t node2;
      nnz_lno_t numOutsideNeighbors;
      bool lockedBoth;
    };

    KOKKOS_INLINE_FUNCTION void operator()(const team_member_t t) const
    {
      SharedData* wholeTeamShared = (SharedData*) t.team_shmem().get_shmem(t.team_size() * sizeof(SharedData));
      SharedData& sh = wholeTeamShared[t.team_rank()];
      for(int iter = 0; iter < 50; iter++)
      {
        //Each thread first chooses one row randomly
        Kokkos::single(Kokkos::PerThread(t),
          [&]()
          {
            auto state = randPool.get_state();
            do
            {
              nnz_lno_t chosen = state.rand64(numRows);
              //try to acquire the lock on chosen node
              if(nodeLocks.set(chosen))
              {
                sh.node1 = chosen;
                break;
              }
            }
            while(true);
            randPool.free_state(state);
            //use numRows to represent that no second vertex has been chosen yet
            sh.node2 = numRows;
            sh.lockedBoth = false;
          });
        nnz_lno_t cluster1 = origToCluster(sh.node1);
        //Figure out how many neighbors of node1 are not in cluster1.
        //These are the candidate values for node2 (do nothing if numOutsideNeighbors == 0).
        size_type row_start1 = row_map(sh.node1);
        nnz_lno_t degree1 = row_map(sh.node1 + 1) - row_start1;
        Kokkos::parallel_reduce(Kokkos::ThreadVectorRange(t, degree1),
          [&](const nnz_lno_t i, nnz_lno_t& lnumOutside)
          {
            nnz_lno_t nei = col_inds(row_start1 + i);
            if(origToCluster(nei) != cluster1)
              lnumOutside++;
          }, sh.numOutsideNeighbors);
        if(sh.numOutsideNeighbors == 0)
        {
          //Can't profit by swapping this row with anything,
          //since swapping two nodes in the same cluster doesn't change the cluster graph.
          Kokkos::single(Kokkos::PerThread(t),
            [&]()
            {
              nodeLocks.reset(sh.node1);
            });
          continue;
        }
        //otherwise, randomly choose one of the outside neighbors to be node2.
        Kokkos::parallel_reduce(Kokkos::ThreadVectorRange(t, degree1),
          [&](const nnz_lno_t i, nnz_lno_t& lnode2)
          {
            nnz_lno_t nei = col_inds(row_start1 + i);
            if(origToCluster(nei) != cluster1)
            {
              if(lnode2 == numRows)
                lnode2 = nei;
              else
              {
                auto state = randPool.get_state();
                if(state.rand(sh.numOutsideNeighbors) == 0)
                  lnode2 = nei;
                randPool.free_state(state);
              }
            }
          }, sh.node2);
        //acquire the lock on node 2
        Kokkos::single(Kokkos::PerThread(t),
          [&]()
          {
            if(nodeLocks.set(sh.node2))
              sh.lockedBoth = true;
            if(!sh.lockedBoth)
              nodeLocks.reset(sh.node1);
          });
        if(!sh.lockedBoth)
        {
          //can't proceed with this pair of vertices, try another in the next iteration
          continue;
        }
        //finally, compute the value of swapping node1 and node2.
        //Count the neighbors of node1 and node2 in their own clusters, and in each other's clusters.
        //node1's number of self-cluster neighbors is already available.
        nnz_lno_t cluster2 = origToCluster(sh.node2);
        nnz_lno_t node1Self = degree1 - 1 - sh.numOutsideNeighbors;
        nnz_lno_t node1Cross = 0;
        nnz_lno_t node2Self = 0;
        nnz_lno_t node2Cross = 0;
        Kokkos::parallel_reduce(Kokkos::ThreadVectorRange(t, degree1),
          [&](const nnz_lno_t i, nnz_lno_t& lnode1Cross)
          {
            nnz_lno_t nei = col_inds(row_start1 + i);
            if(origToCluster(nei) == cluster2)
              lnode1Cross++;
          }, node1Cross);
        size_type row_start2 = row_map(sh.node2);
        nnz_lno_t degree2 = row_map(sh.node2 + 1) - row_start2;
        Kokkos::parallel_reduce(Kokkos::ThreadVectorRange(t, degree2),
          [&](const nnz_lno_t i, nnz_lno_t& lnode2Self)
          {
            nnz_lno_t nei = col_inds(row_start2 + i);
            if(nei != sh.node2 && origToCluster(nei) == cluster2)
              lnode2Self++;
          }, node2Self);
        Kokkos::parallel_reduce(Kokkos::ThreadVectorRange(t, degree2),
          [&](const nnz_lno_t i, nnz_lno_t& lnode2Cross)
          {
            nnz_lno_t nei = col_inds(row_start2 + i);
            if(origToCluster(nei) == cluster1)
              lnode2Cross++;
          }, node2Cross);
        Kokkos::single(Kokkos::PerThread(t),
          [&]()
          {
            //Compare the number of in-cluster entries with and without swapping.
            //Note: neither the "self" nor "outside" counts include the diagonal edge,
            //since that will never cross between clusters.
            if(node1Self + node2Self < node1Cross + node2Cross)
            {
              nnz_lno_t tmp = order(sh.node1);
              order(sh.node1) = order(sh.node2);
              order(sh.node2) = tmp;
            }
            //in any case, can now unlock both nodes
            nodeLocks.reset(sh.node1);
            nodeLocks.reset(sh.node2);
          });
      }
    }

    size_t team_shmem_size(int team_size) const
    {
      return team_size * sizeof(SharedData);
    }

    nnz_view_t order;
    const_lno_row_view_t row_map;
    const_lno_nnz_view_t col_inds;
    nnz_lno_t clusterSize;
    nnz_lno_t numRows;
    RandPool randPool;
    bitset_t nodeLocks;
  };

  //The shuffle-reorder algorithm tries to permute a graph's vertex labels so that the graph
  //is as close to block diagonal as possible. Unlike RCM, it doesn't care about bandwidth. This
  //is fine for graph partitioning: any entry outside the diagonal blocks is equally bad.
  //
  //Requires that the graph is symmetric.
  //The graph for MT-GS/MT-SGS will always be symmetrized (if not symmetric on input) so this is fine.
  //
  //This algorithm directly tries to minimize the number of edges in the cluster graph for cluster Gauss-Seidel.
  //
  //Algorithm idea: Choose a node at random. Choose a random neighbor that is not in the same diagonal block (if any).
  //If the total number of entries in the diagonal blocks would increase by swapping the node and its neighbor, do it.
  nnz_view_t shuffledClusterOrder(nnz_lno_t clusterSize)
  {
    //order: the new label of each original node. Therefore maps from from original row to permuted row.
    nnz_view_t order("Block diag order", numRows);
    //start with the identity permutation
    Kokkos::parallel_for(range_policy_t(0, numRows), IotaFunctor<nnz_view_t, nnz_lno_t>(order));
    if(clusterSize == numRows)
    {
      //nothing to do, all in the same cluster
      return order;
    }
    ShuffleFunctor shuf(order, rowmap, colinds, clusterSize, randPool);
    int team_size = 0;
    int vector_size = 0;
#ifdef KOKKOS_ENABLE_DEPRECATED_CODE
    int max_allowed_team_size = team_policy_t::team_size_max(shuf);
    KokkosKernels::Impl::get_suggested_vector_team_size<nnz_lno_t, MyExecSpace>(
        max_allowed_team_size,
        vector_size,
        team_size,
        numRows, colinds.extent(0));
#else
    KokkosKernels::Impl::get_suggested_vector_size<nnz_lno_t, MyExecSpace>(
        vector_size,
        numRows, colinds.extent(0));
    team_size = get_suggested_team_size<team_policy_t>(shuf, vector_size);
#endif
    Kokkos::parallel_for(team_policy_t(256, team_size, vector_size), shuf);
    return order;
  }
};

}}  //KokkosSparse::Impl

#endif

