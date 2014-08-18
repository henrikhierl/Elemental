/*
   Copyright (c) 2009-2014, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#pragma once
#ifndef EL_TWOSIDEDTRMM_LVAR2_HPP
#define EL_TWOSIDEDTRMM_LVAR2_HPP

namespace El {
namespace twotrmm {

// The only reason a field is required is for the existence of 1/2, which is 
// an artifact of the algorithm...
template<typename F> 
inline void
LVar2( UnitOrNonUnit diag, Matrix<F>& A, const Matrix<F>& L )
{
    DEBUG_ONLY(
        CallStackEntry cse("twotrmm::LVar2");
        if( A.Height() != A.Width() )
            LogicError( "A must be square." );
        if( L.Height() != L.Width() )
            LogicError( "Triangular matrices must be square." );
        if( A.Height() != L.Height() )
            LogicError( "A and L must be the same size." );
    )
    const Int n = A.Height();
    const Int bsize = Blocksize();

    // Temporary products
    Matrix<F> Y21;

    for( Int k=0; k<n; k+=bsize )
    {
        const Int nb = Min(bsize,n-k);

        const Range<Int> ind0( 0,    k    );
        const Range<Int> ind1( k,    k+nb );
        const Range<Int> ind2( k+nb, n    );

        auto A10 =       View( A, ind1, ind0 );
        auto A11 =       View( A, ind1, ind1 );
        auto A21 =       View( A, ind2, ind1 );
        auto A22 = LockedView( A, ind2, ind2 );

        auto L11 = LockedView( L, ind1, ind1 );
        auto L21 = LockedView( L, ind2, ind1 );

        // A10 := L11' A10
        Trmm( LEFT, LOWER, ADJOINT, diag, F(1), L11, A10 );

        // A10 := A10 + L21' A20
        Gemm( ADJOINT, NORMAL, F(1), L21, A20, F(1), A10 );

        // Y21 := A22 L21
        Zeros( Y21, A21.Height(), nb );
        Hemm( LEFT, LOWER, F(1), A22, L21, F(0), Y21 );

        // A21 := A21 L11
        Trmm( RIGHT, LOWER, NORMAL, diag, F(1), L11, A21 );

        // A21 := A21 + 1/2 Y21
        Axpy( F(1)/F(2), Y21, A21 );

        // A11 := L11' A11 L11
        twotrmm::LUnb( diag, A11, L11 );

        // A11 := A11 + (A21' L21 + L21' A21)
        Her2k( LOWER, ADJOINT, F(1), A21, L21, F(1), A11 );

        // A21 := A21 + 1/2 Y21
        Axpy( F(1)/F(2), Y21, A21 );
    }
}

template<typename F> 
inline void
LVar2
( UnitOrNonUnit diag, 
  AbstractDistMatrix<F>& APre, const AbstractDistMatrix<F>& LPre )
{
    DEBUG_ONLY(
        CallStackEntry cse("twotrmm::LVar2");
        if( APre.Height() != APre.Width() )
            LogicError( "A must be square." );
        if( LPre.Height() != LPre.Width() )
            LogicError( "Triangular matrices must be square." );
        if( APre.Height() != LPre.Height() )
            LogicError( "A and L must be the same size." );
    )
    const Int n = APre.Height();
    const Int bsize = Blocksize();
    const Grid& g = APre.Grid();

    DistMatrix<F> A(g), L(g);
    Copy( APre, A, READ_WRITE_PROXY );
    Copy( LPre, L, READ_PROXY );

    // Temporary distributions
    DistMatrix<F,STAR,STAR> A11_STAR_STAR(g), L11_STAR_STAR(g),
                            X11_STAR_STAR(g);
    DistMatrix<F,STAR,MR  > L21Adj_STAR_MR(g), X10_STAR_MR(g);
    DistMatrix<F,STAR,VR  > A10_STAR_VR(g);
    DistMatrix<F,MC,  STAR> L21_MC_STAR(g), Z21_MC_STAR(g);
    DistMatrix<F,MR,  STAR> Z21_MR_STAR(g);
    DistMatrix<F,VC,  STAR> A21_VC_STAR(g), L21_VC_STAR(g);
    DistMatrix<F,VR,  STAR> L21_VR_STAR(g);
    DistMatrix<F,MR,  MC  > Z21_MR_MC(g);
    DistMatrix<F> Y21(g);

    for( Int k=0; k<n; k+=bsize )
    {
        const Int nb = Min(bsize,n-k);

        const Range<Int> ind0( 0,    k    );
        const Range<Int> ind1( k,    k+nb );
        const Range<Int> ind2( k+nb, n    );

        auto A10 =       View( A, ind1, ind0 );
        auto A11 =       View( A, ind1, ind1 );
        auto A21 =       View( A, ind2, ind1 );
        auto A22 = LockedView( A, ind2, ind2 );

        auto L11 = LockedView( L, ind1, ind1 );
        auto L21 = LockedView( L, ind2, ind1 );

        // A10 := L11' A10
        L11_STAR_STAR = L11;
        A10_STAR_VR = A10;
        LocalTrmm
        ( LEFT, LOWER, ADJOINT, diag, F(1), L11_STAR_STAR, A10_STAR_VR );
        A10 = A10_STAR_VR;

        // A10 := A10 + L21' A20
        L21_MC_STAR.AlignWith( A20 );
        L21_MC_STAR = L21;
        X10_STAR_MR.AlignWith( A10 );
        LocalGemm( ADJOINT, NORMAL, F(1), L21_MC_STAR, A20, X10_STAR_MR );
        A10.ColSumScatterUpdate( F(1), X10_STAR_MR );

        // Y21 := A22 L21
        L21_VC_STAR.AlignWith( A22 );
        L21_VR_STAR.AlignWith( A22 );
        L21_VC_STAR = L21_MC_STAR;
        L21_VR_STAR = L21_VC_STAR;
        L21Adj_STAR_MR.AlignWith( A22 );
        L21_VR_STAR.AdjointPartialColAllGather( L21Adj_STAR_MR );
        Z21_MC_STAR.AlignWith( A22 );
        Z21_MR_STAR.AlignWith( A22 );
        Zeros( Z21_MC_STAR, A21.Height(), nb );
        Zeros( Z21_MR_STAR, A21.Height(), nb );
        symm::LocalAccumulateLL
        ( ADJOINT, 
          F(1), A22, L21_MC_STAR, L21Adj_STAR_MR, Z21_MC_STAR, Z21_MR_STAR );
        Z21_MR_MC.RowSumScatterFrom( Z21_MR_STAR );
        Y21.AlignWith( A21 );
        Y21 = Z21_MR_MC;
        Y21.RowSumScatterUpdate( F(1), Z21_MC_STAR ); 

        // A21 := A21 L11
        A21_VC_STAR.AlignWith( A22 );
        A21_VC_STAR = A21;
        LocalTrmm
        ( RIGHT, LOWER, NORMAL, diag, F(1), L11_STAR_STAR, A21_VC_STAR );
        A21 = A21_VC_STAR;

        // A21 := A21 + 1/2 Y21
        Axpy( F(1)/F(2), Y21, A21 );

        // A11 := L11' A11 L11
        A11_STAR_STAR = A11;
        LocalTwoSidedTrmm( LOWER, diag, A11_STAR_STAR, L11_STAR_STAR );
        A11 = A11_STAR_STAR;

        // A11 := A11 + (A21' L21 + L21' A21)
        A21_VC_STAR = A21;
        Zeros( X11_STAR_STAR, nb, nb );
        Her2k
        ( LOWER, ADJOINT,
          F(1), A21_VC_STAR.Matrix(), L21_VC_STAR.Matrix(),
          F(0), X11_STAR_STAR.Matrix() );
        A11.SumScatterUpdate( F(1), X11_STAR_STAR );

        // A21 := A21 + 1/2 Y21
        Axpy( F(1)/F(2), Y21, A21 );
    }

    Copy( A, APre, RESTORE_READ_WRITE_PROXY );
}

} // namespace twotrmm
} // namespace El

#endif // ifndef EL_TWOSIDEDTRMM_LVAR2_HPP