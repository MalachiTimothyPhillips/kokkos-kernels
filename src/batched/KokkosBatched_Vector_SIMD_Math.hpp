#ifndef __KOKKOSBATCHED_VECTOR_SIMD_MATH_HPP__
#define __KOKKOSBATCHED_VECTOR_SIMD_MATH_HPP__

/// \author Kyungjoo Kim (kyukim@sandia.gov)

#include "Kokkos_Complex.hpp"

namespace KokkosBatched {
  namespace Experimental {

    /// simd 

    template<typename T, int l>
    inline
    static
    typename std::enable_if<std::is_same<Kokkos::Impl::ActiveExecutionMemorySpace,Kokkos::HostSpace>::value,
                            Vector<SIMD<T>,l> >::type
    sqrt(const Vector<SIMD<T>,l> &a) {
      typedef Kokkos::Details::ArithTraits<T> ats;
      Vector<SIMD<T>,l> r_val;
#if defined( KOKKOS_ENABLE_PRAGMA_IVDEP )
#pragma ivdep
#endif
#if defined( KOKKOS_ENABLE_PRAGMA_VECTOR )
#pragma vector always
#endif
      for (int i=0;i<l;++i) 
        r_val[i] = ats::sqrt(a[i]);

      return r_val;
    }

    template<typename T, int l>
    inline
    static
    typename std::enable_if<std::is_same<Kokkos::Impl::ActiveExecutionMemorySpace,Kokkos::HostSpace>::value,
                            Vector<SIMD<T>,l> >::type
    cbrt(const Vector<SIMD<T>,l> &a) {
      typedef Kokkos::Details::ArithTraits<T> ats;
      Vector<SIMD<T>,l> r_val;
#if defined( KOKKOS_ENABLE_PRAGMA_IVDEP )
#pragma ivdep
#endif
#if defined( KOKKOS_ENABLE_PRAGMA_VECTOR )
#pragma vector always
#endif
      for (int i=0;i<l;++i)
        r_val[i] = ats::cbrt(a[i]);

      return r_val;
    }

    template<typename T, int l>
    inline
    static
    typename std::enable_if<std::is_same<Kokkos::Impl::ActiveExecutionMemorySpace,Kokkos::HostSpace>::value,
                            Vector<SIMD<T>,l> >::type
    log(const Vector<SIMD<T>,l> &a) {
      typedef Kokkos::Details::ArithTraits<T> ats;
      Vector<SIMD<T>,l> r_val;
#if defined( KOKKOS_ENABLE_PRAGMA_IVDEP )
#pragma ivdep
#endif
#if defined( KOKKOS_ENABLE_PRAGMA_VECTOR )
#pragma vector always
#endif
      for (int i=0;i<l;++i)
        r_val[i] = ats::log(a[i]);

      return r_val;
    }

    template<typename T, int l>
    inline
    static
    typename std::enable_if<std::is_same<Kokkos::Impl::ActiveExecutionMemorySpace,Kokkos::HostSpace>::value,
                            Vector<SIMD<T>,l> >::type
    log10(const Vector<SIMD<T>,l> &a) {
      typedef Kokkos::Details::ArithTraits<T> ats;
      Vector<SIMD<T>,l> r_val;
#if defined( KOKKOS_ENABLE_PRAGMA_IVDEP )
#pragma ivdep
#endif
#if defined( KOKKOS_ENABLE_PRAGMA_VECTOR )
#pragma vector always
#endif
      for (int i=0;i<l;++i)
        r_val[i] = ats::log10(a[i]);

      return r_val;
    }

    template<typename T, int l>
    inline
    static
    typename std::enable_if<std::is_same<Kokkos::Impl::ActiveExecutionMemorySpace,Kokkos::HostSpace>::value,
                            Vector<SIMD<T>,l> >::type
    exp(const Vector<SIMD<T>,l> &a) {
      typedef Kokkos::Details::ArithTraits<T> ats;
      Vector<SIMD<T>,l> r_val;
#if defined( KOKKOS_ENABLE_PRAGMA_IVDEP )
#pragma ivdep
#endif
#if defined( KOKKOS_ENABLE_PRAGMA_VECTOR )
#pragma vector always
#endif
      for (int i=0;i<l;++i)
        r_val[i] = ats::exp(a[i]);

      return r_val;
    }

    template<typename T0, typename T1, int l>
    inline
    static
    typename std::enable_if<std::is_same<Kokkos::Impl::ActiveExecutionMemorySpace,Kokkos::HostSpace>::value,
                            Vector<SIMD<T0>,l> >::type
    pow(const Vector<SIMD<T0>,l> &a, const Vector<SIMD<T1>,l> &b) {
      typedef Kokkos::Details::ArithTraits<T0> ats;
      Vector<SIMD<T0>,l> r_val;
#if defined( KOKKOS_ENABLE_PRAGMA_IVDEP )
#pragma ivdep
#endif
#if defined( KOKKOS_ENABLE_PRAGMA_VECTOR )
#pragma vector always
#endif
      for (int i=0;i<l;++i)
        r_val[i] = ats::pow(a[i], b[i]);

      return r_val;
    }

    template<typename T0, typename T1, int l>
    inline
    static
    typename std::enable_if<std::is_same<Kokkos::Impl::ActiveExecutionMemorySpace,Kokkos::HostSpace>::value,
                            Vector<SIMD<T0>,l> >::type
    pow(const T0 &a, const Vector<SIMD<T1>,l> &b) {
      return pow(Vector<SIMD<T0>,l>(a), b);
    }

    template<typename T0, typename T1, int l>
    inline
    static
    typename std::enable_if<std::is_same<Kokkos::Impl::ActiveExecutionMemorySpace,Kokkos::HostSpace>::value,
                            Vector<SIMD<T0>,l> >::type
    pow(const Vector<SIMD<T0>,l> &a, const T1 &b) {
      return pow(a, Vector<SIMD<T1>,l>(b));
    }

    template<typename T, int l>
    inline
    static
    typename std::enable_if<std::is_same<Kokkos::Impl::ActiveExecutionMemorySpace,Kokkos::HostSpace>::value
                            && !std::is_integral<T>::value,
                            Vector<SIMD<T>,l> >::type
    sin(const Vector<SIMD<T>,l> &a) {
      typedef Kokkos::Details::ArithTraits<T> ats;
      Vector<SIMD<T>,l> r_val;
#if defined( KOKKOS_ENABLE_PRAGMA_IVDEP )
#pragma ivdep
#endif
#if defined( KOKKOS_ENABLE_PRAGMA_VECTOR )
#pragma vector always
#endif
      for (int i=0;i<l;++i)
        r_val[i] = ats::sin(a[i]);

      return r_val;
    }

    template<typename T, int l>
    inline
    static
    typename std::enable_if<std::is_same<Kokkos::Impl::ActiveExecutionMemorySpace,Kokkos::HostSpace>::value
                            && !std::is_integral<T>::value,
                            Vector<SIMD<T>,l> >::type
    cos(const Vector<SIMD<T>,l> &a) {
      typedef Kokkos::Details::ArithTraits<T> ats;
      Vector<SIMD<T>,l> r_val;
#if defined( KOKKOS_ENABLE_PRAGMA_IVDEP )
#pragma ivdep
#endif
#if defined( KOKKOS_ENABLE_PRAGMA_VECTOR )
#pragma vector always
#endif
      for (int i=0;i<l;++i)
        r_val[i] = ats::cos(a[i]);

      return r_val;
    }

    template<typename T, int l>
    inline
    static
    typename std::enable_if<std::is_same<Kokkos::Impl::ActiveExecutionMemorySpace,Kokkos::HostSpace>::value
                            && !std::is_integral<T>::value,
                            Vector<SIMD<T>,l> >::type
    tan(const Vector<SIMD<T>,l> &a) {
      typedef Kokkos::Details::ArithTraits<T> ats;
      Vector<SIMD<T>,l> r_val;
#if defined( KOKKOS_ENABLE_PRAGMA_IVDEP )
#pragma ivdep
#endif
#if defined( KOKKOS_ENABLE_PRAGMA_VECTOR )
#pragma vector always
#endif
      for (int i=0;i<l;++i)
        r_val[i] = ats::tan(a[i]);

      return r_val;
    }

    template<typename T, int l>
    inline
    static
    typename std::enable_if<std::is_same<Kokkos::Impl::ActiveExecutionMemorySpace,Kokkos::HostSpace>::value
                            && !std::is_integral<T>::value,
                            Vector<SIMD<T>,l> >::type
    sinh(const Vector<SIMD<T>,l> &a) {
      typedef Kokkos::Details::ArithTraits<T> ats;
      Vector<SIMD<T>,l> r_val;
#if defined( KOKKOS_ENABLE_PRAGMA_IVDEP )
#pragma ivdep
#endif
#if defined( KOKKOS_ENABLE_PRAGMA_VECTOR )
#pragma vector always
#endif
      for (int i=0;i<l;++i)
        r_val[i] = ats::sinh(a[i]);

      return r_val;
    }

    template<typename T, int l>
    inline
    static
    typename std::enable_if<std::is_same<Kokkos::Impl::ActiveExecutionMemorySpace,Kokkos::HostSpace>::value
                            && !std::is_integral<T>::value,
                            Vector<SIMD<T>,l> >::type
    cosh(const Vector<SIMD<T>,l> &a) {
      typedef Kokkos::Details::ArithTraits<T> ats;
      Vector<SIMD<T>,l> r_val;
#if defined( KOKKOS_ENABLE_PRAGMA_IVDEP )
#pragma ivdep
#endif
#if defined( KOKKOS_ENABLE_PRAGMA_VECTOR )
#pragma vector always
#endif
      for (int i=0;i<l;++i)
        r_val[i] = ats::cosh(a[i]);
      
      return r_val;
    }

    template<typename T, int l>
    inline
    static
    typename std::enable_if<std::is_same<Kokkos::Impl::ActiveExecutionMemorySpace,Kokkos::HostSpace>::value
                            && !std::is_integral<T>::value,
                            Vector<SIMD<T>,l> >::type
    tanh(const Vector<SIMD<T>,l> &a) {
      typedef Kokkos::Details::ArithTraits<T> ats;
      Vector<SIMD<T>,l> r_val;
#if defined( KOKKOS_ENABLE_PRAGMA_IVDEP )
#pragma ivdep
#endif
#if defined( KOKKOS_ENABLE_PRAGMA_VECTOR )
#pragma vector always
#endif
      for (int i=0;i<l;++i)
        r_val[i] = ats::tanh(a[i]);

      return r_val;
    }

    template<typename T, int l>
    inline
    static
    typename std::enable_if<std::is_same<Kokkos::Impl::ActiveExecutionMemorySpace,Kokkos::HostSpace>::value
                            && !std::is_integral<T>::value,
                            Vector<SIMD<T>,l> >::type
    asin(const Vector<SIMD<T>,l> &a) {
      typedef Kokkos::Details::ArithTraits<T> ats;
      Vector<SIMD<T>,l> r_val;
#if defined( KOKKOS_ENABLE_PRAGMA_IVDEP )
#pragma ivdep
#endif
#if defined( KOKKOS_ENABLE_PRAGMA_VECTOR )
#pragma vector always
#endif
      for (int i=0;i<l;++i)
        r_val[i] = ats::asin(a[i]);

      return r_val;
    }

    template<typename T, int l>
    inline
    static
    typename std::enable_if<std::is_same<Kokkos::Impl::ActiveExecutionMemorySpace,Kokkos::HostSpace>::value
                            && !std::is_integral<T>::value,
                            Vector<SIMD<T>,l> >::type
    acos(const Vector<SIMD<T>,l> &a) {
      typedef Kokkos::Details::ArithTraits<T> ats;
      Vector<SIMD<T>,l> r_val;
#if defined( KOKKOS_ENABLE_PRAGMA_IVDEP )
#pragma ivdep
#endif
#if defined( KOKKOS_ENABLE_PRAGMA_VECTOR )
#pragma vector always
#endif
      for (int i=0;i<l;++i)
        r_val[i] = ats::acos(a[i]);

      return r_val;
    }

    template<typename T, int l>
    inline
    static
    typename std::enable_if<std::is_same<Kokkos::Impl::ActiveExecutionMemorySpace,Kokkos::HostSpace>::value
                            && !std::is_integral<T>::value,
                            Vector<SIMD<T>,l> >::type
    atan(const Vector<SIMD<T>,l> &a) {
      typedef Kokkos::Details::ArithTraits<T> ats;
      Vector<SIMD<T>,l> r_val;
#if defined( KOKKOS_ENABLE_PRAGMA_IVDEP )
#pragma ivdep
#endif
#if defined( KOKKOS_ENABLE_PRAGMA_VECTOR )
#pragma vector always
#endif
      for (int i=0;i<l;++i)
        r_val[i] = ats::atan(a[i]);

      return r_val;
    }

    template<typename T, int l>
    inline
    static
    typename std::enable_if<std::is_same<Kokkos::Impl::ActiveExecutionMemorySpace,Kokkos::HostSpace>::value
                            && !std::is_integral<T>::value,
                            Vector<SIMD<T>,l> >::type
    atan2(const Vector<SIMD<T>,l> &a, const Vector<SIMD<T>,l> &b) {
      //typedef Kokkos::Details::ArithTraits<T> ats;
      Vector<SIMD<T>,l> r_val;
#if defined( KOKKOS_ENABLE_PRAGMA_IVDEP )
#pragma ivdep
#endif
#if defined( KOKKOS_ENABLE_PRAGMA_VECTOR )
#pragma vector always
#endif
      for (int i=0;i<l;++i)
        r_val[i] = std::atan2(a[i], b[i]);

      return r_val;
    }

    template<typename T, int l>
    inline
    static
    typename std::enable_if<std::is_same<Kokkos::Impl::ActiveExecutionMemorySpace,Kokkos::HostSpace>::value
                            && !std::is_integral<T>::value,
                            Vector<SIMD<T>,l> >::type
    atan2(const T &a, const Vector<SIMD<T>,l> &b) {
      return atan2(Vector<SIMD<T>,l>(a), b);
    }

    template<typename T, int l>
    inline
    static
    typename std::enable_if<std::is_same<Kokkos::Impl::ActiveExecutionMemorySpace,Kokkos::HostSpace>::value
                            && !std::is_integral<T>::value,
                            Vector<SIMD<T>,l> >::type
    atan2(const Vector<SIMD<T>,l> &a, const T &b) {
      return atan2(a, Vector<SIMD<T>,l>(b));
    }

  }
}

#endif
