/*
   Copyright (c) 2009-2014, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#pragma once
#ifndef EL_CHOLESKY_QR_HPP
#define EL_CHOLESKY_QR_HPP

namespace El {
namespace qr {

// NOTE: This version is designed for tall-skinny matrices and is much less
//       numerically stable than Householder-based QR factorizations
//
// Computes the QR factorization of full-rank tall-skinny matrix A and 
// overwrites A with Q
//

template<typename F> 
void Cholesky( Matrix<F>& A, Matrix<F>& R )
{
    DEBUG_ONLY(CallStackEntry cse("qr::Cholesky"))
    if( A.Height() < A.Width() )
        LogicError("A^H A will be singular");
    Herk( UPPER, ADJOINT, F(1), A, R );
    El::Cholesky( UPPER, R );
    Trsm( RIGHT, UPPER, NORMAL, NON_UNIT, F(1), R, A );
}

template<typename F> 
void Cholesky( AbstractDistMatrix<F>& APre, AbstractDistMatrix<F>& RPre )
{
    DEBUG_ONLY(CallStackEntry cse("qr::Cholesky"))
    const Int m = APre.Height();
    const Int n = APre.Width();
    if( m < n )
        LogicError("A^H A will be singular");

    // Proxies cannot be resized since they might be views
    RPre.Resize( n, n );

    const Grid& g = APre.Grid();
    DistMatrix<F,VC,STAR> A(g);
    DistMatrix<F,STAR,STAR> R(g);
    Copy( APre, A, READ_WRITE_PROXY );
    Copy( RPre, R, WRITE_PROXY );

    Zero( R );
    Herk( UPPER, ADJOINT, F(1), A.Matrix(), F(0), R.Matrix() );
    R.SumOver( A.ColComm() );
    El::Cholesky( UPPER, R.Matrix() );
    Trsm( RIGHT, UPPER, NORMAL, NON_UNIT, F(1), R.Matrix(), A.Matrix() );

    Copy( A, APre, RESTORE_READ_WRITE_PROXY );
    Copy( R, RPre, RESTORE_WRITE_PROXY );
}

} // namespace qr
} // namespace El

#endif // ifndef EL_QR_CHOLESKY_HPP