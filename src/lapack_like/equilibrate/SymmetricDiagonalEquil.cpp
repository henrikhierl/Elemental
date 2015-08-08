/*
   Copyright (c) 2009-2015, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#include "El.hpp"

namespace El {

template<typename F>
void SymmetricDiagonalEquil
( Matrix<F>& A, Matrix<Base<F>>& d, bool progress )
{
    DEBUG_ONLY(CSE cse("SymmetricDiagonalEquil"))
    // TODO: Ensure A is square
    const Int n = A.Height();
    Ones( d, n, 1 );
    if( progress )
        Output("Diagonal equilibration not yet enabled for dense matrices");
}

template<typename F>
void SymmetricDiagonalEquil
( AbstractDistMatrix<F>& A, AbstractDistMatrix<Base<F>>& d, bool progress )
{
    DEBUG_ONLY(CSE cse("SymmetricDiagonalEquil"))
    // TODO: Ensure A is square
    const Int n = A.Height();
    Ones( d, n, 1 );
    if( progress )
        Output("Diagonal equilibration not yet enabled for dense matrices");
}

template<typename F>
void SymmetricDiagonalEquil
( SparseMatrix<F>& A, Matrix<Base<F>>& d, bool progress )
{
    DEBUG_ONLY(CSE cse("SymmetricDiagonalEquil"))
    typedef Base<F> Real;
    auto maxSqrtLambda = []( F delta ) 
                         { return Sqrt(Max(Abs(delta),Real(1))); };
    function<Real(F)> maxSqrt( maxSqrtLambda );
    GetMappedDiagonal( A, d, maxSqrt );
    if( progress )
    {
        const Real maxNorm = MaxNorm( d ); 
        Output("  || d ||_max = ",maxNorm);
    }
    DiagonalSolve( LEFT, NORMAL, d, A );
    DiagonalSolve( RIGHT, NORMAL, d, A );
}

template<typename F>
void SymmetricDiagonalEquil
( DistSparseMatrix<F>& A, DistMultiVec<Base<F>>& d, 
  bool progress, bool time )
{
    DEBUG_ONLY(CSE cse("SymmetricDiagonalEquil"))
    typedef Base<F> Real;
    mpi::Comm comm = A.Comm();
    const int commRank = mpi::Rank(comm);
    Timer timer;

    d.SetComm( comm );
    auto maxSqrtLambda = []( F delta )
                         { return Sqrt(Max(Abs(delta),Real(1))); };
    function<Real(F)> maxSqrt( maxSqrtLambda );
    if( commRank == 0 && time )
        timer.Start();
    GetMappedDiagonal( A, d, maxSqrt );
    if( commRank == 0 && time )
        Output("  Get mapped diag time: ",timer.Stop());
    if( commRank == 0 && time )
        timer.Start();
    DiagonalSolve( LEFT, NORMAL, d, A );
    if( commRank == 0 && time )
        Output("  Left diag solve time: ",timer.Stop());
    if( commRank == 0 && time )
        timer.Start();
    DiagonalSolve( RIGHT, NORMAL, d, A );
    if( commRank == 0 && time )
        Output("  Right diag solve time: ",timer.Stop());
    if( progress )
    {
        const Real maxNorm = MaxNorm( d );
        if( commRank == 0 ) 
            Output("  || d ||_max = ",maxNorm); 
    }
}

#define PROTO(F) \
  template void SymmetricDiagonalEquil \
  ( Matrix<F>& A, Matrix<Base<F>>& d, bool progress ); \
  template void SymmetricDiagonalEquil \
  ( AbstractDistMatrix<F>& A,  AbstractDistMatrix<Base<F>>& d, \
    bool progress ); \
  template void SymmetricDiagonalEquil \
  ( SparseMatrix<F>& A, Matrix<Base<F>>& d, bool progress ); \
  template void SymmetricDiagonalEquil \
  ( DistSparseMatrix<F>& A, DistMultiVec<Base<F>>& d, \
    bool progress, bool time );

#define EL_NO_INT_PROTO
#include "El/macros/Instantiate.h"

} // namespace El
